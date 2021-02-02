/*
 * Copyright 2020 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_INFO
#include "embc/stream/pubsub.h"
#include "embc/stream/msg_ring_buffer.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/platform.h"
#include "embc/collections/list.h"
#include "embc/cstr.h"

struct subscriber_s {
    embc_pubsub_subscribe_fn cbk_fn;
    void * cbk_user_data;
    struct embc_list_s item;
};

struct topic_s {
    struct embc_pubsub_value_s value;
    struct topic_s * parent;
    const char * meta;
    struct embc_list_s item;  // used by parent->children
    struct embc_list_s children;
    struct embc_list_s subscribers;
    char name[EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL];
};

struct message_s {
    char name[EMBC_PUBSUB_TOPIC_LENGTH_MAX];
    struct embc_pubsub_value_s value;
    embc_pubsub_subscribe_fn src_fn;
    void * src_user_data;
    struct embc_list_s item;
};

struct embc_pubsub_s {
    embc_pubsub_on_publish_fn cbk_fn;
    void * cbk_user_data;
    embc_pubsub_lock lock;
    embc_pubsub_unlock unlock;
    void * lock_user_data;
    struct topic_s * topic;
    struct embc_list_s subscriber_free;
    struct embc_list_s msg_pend;
    struct embc_list_s msg_free;

    struct embc_mrb_s mrb;
    uint8_t buffer[];
};

static void lock_default(void * user_data) {
    (void) user_data;
}

static void unlock_default(void * user_data) {
    (void) user_data;
}

static inline void lock(struct embc_pubsub_s * self) {
    self->lock(self->lock_user_data);
}

static inline void unlock(struct embc_pubsub_s * self) {
    self->unlock(self->lock_user_data);
}

static struct message_s * msg_alloc(struct embc_pubsub_s * self) {
    struct message_s * msg;
    if (embc_list_is_empty(&self->msg_free)) {
        msg = embc_alloc(sizeof(struct message_s));
        EMBC_ASSERT_ALLOC(msg);
    } else {
        struct embc_list_s * item = embc_list_remove_head(&self->msg_free);
        msg = EMBC_CONTAINER_OF(item, struct message_s, item);
    }
    embc_list_initialize(&msg->item);
    msg->name[0] = 0;
    msg->value.type = EMBC_PUBSUB_DTYPE_NULL;
    return msg;
}

static struct subscriber_s * subscriber_alloc(struct embc_pubsub_s * self) {
    struct subscriber_s * sub;
    if (!embc_list_is_empty(&self->subscriber_free)) {
        struct embc_list_s * item;
        item = embc_list_remove_head(&self->subscriber_free);
        sub = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
    } else {
        sub = embc_alloc_clr(sizeof(struct subscriber_s));
        EMBC_ASSERT_ALLOC(sub);
        EMBC_LOGD3("subscriber alloc: %p", (void *) sub);
    }
    embc_list_initialize(&sub->item);
    sub->cbk_fn = NULL;
    sub->cbk_user_data = NULL;
    return sub;
}

static void subscriber_free(struct embc_pubsub_s * self, struct subscriber_s * sub) {
    embc_list_add_tail(&self->subscriber_free, &sub->item);
}

static bool topic_name_set(struct topic_s * topic, const char * name) {
    for (int i = 0; i < (EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL - 1); ++i) {
        if (*name) {
            topic->name[i] = *name++;
        } else {
            topic->name[i] = 0;
            return true;
        }
    }
    EMBC_LOGW("topic name truncated: %s", name);
    topic->name[EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL - 1] = 0;
    return false;
}

static bool topic_str_copy(char * topic_str, const char * src) {
    size_t sz = 0;
    while (src[sz] && (sz < EMBC_PUBSUB_TOPIC_LENGTH_MAX)) {
        topic_str[sz] = src[sz];
        ++sz;
        if (sz >= EMBC_PUBSUB_TOPIC_LENGTH_MAX) {
            return false;
        }
    }
    topic_str[sz] = 0;
    return true;
}

static void topic_str_append(char * topic_str, const char * topic_sub_str) {
    // WARNING: topic_str must be >= TOPIC_LENGTH_MAX
    // topic_sub_str <= TOPIC_LENGTH_PER_LEVEL
    size_t topic_len = 0;
    char * t = topic_str;

    // find the end of topic_str
    while (*t) {
        ++topic_len;
        ++t;
    }
    if (topic_len >= (EMBC_PUBSUB_TOPIC_LENGTH_MAX - 1)) {
        return;
    }

    // add separator
    if (topic_len) {
        *t++ = '/';
        ++topic_len;
    }

    // Copy substring
    while (*topic_sub_str && (topic_len < (EMBC_PUBSUB_TOPIC_LENGTH_MAX - 1))) {
        *t++ = *topic_sub_str++;
        ++topic_len;
    }
    *t = 0;  // null terminate
}

static struct topic_s * topic_alloc(struct embc_pubsub_s * self, const char * name) {
    (void) self;
    struct topic_s * topic = embc_alloc_clr(sizeof(struct topic_s));
    EMBC_ASSERT_ALLOC(topic);
    topic->value.type = EMBC_PUBSUB_DTYPE_NULL;
    embc_list_initialize(&topic->item);
    embc_list_initialize(&topic->children);
    embc_list_initialize(&topic->subscribers);
    topic_name_set(topic, name);
    EMBC_LOGD3("topic alloc: %p", (void *)topic);
    return topic;
}

static void topic_free(struct embc_pubsub_s * self, struct topic_s * topic) {
    struct embc_list_s * item;
    struct subscriber_s * subscriber;
    embc_list_foreach(&topic->subscribers, item) {
        subscriber = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
        EMBC_LOGD3("subscriber free: %p", (void *)subscriber);
        embc_free(subscriber);
    }
    struct topic_s * subtopic;
    embc_list_foreach(&topic->children, item) {
        subtopic = EMBC_CONTAINER_OF(item, struct topic_s, item);
        topic_free(self, subtopic);
    }
    EMBC_LOGD3("topic free: %p", (void *)topic);
    embc_free(topic);
}

static bool subtopic_get_str(const char ** topic, char * subtopic) {
    const char * t = *topic;
    for (int i = 0; i < EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL; ++i) {
        if (*t == 0) {
            *subtopic = 0;
            *topic = t;
            return true;
        } else if (*t == '/') {
            *subtopic = 0;
            t++;
            *topic = t;
            return true;
        } else {
            *subtopic++ = *t++;
        }
    }
    return false;
}

struct topic_s * subtopic_find(struct topic_s * parent, const char * subtopic_str) {
    struct embc_list_s * item;
    struct topic_s * topic;
    embc_list_foreach(&parent->children, item) {
        topic = EMBC_CONTAINER_OF(item, struct topic_s, item);
        if (0 == strcmp(subtopic_str, topic->name)) {
            return topic;
        }
    }
    return NULL;
}

static struct topic_s * topic_find(struct embc_pubsub_s * self, const char * topic, bool create) {
    char subtopic_str[EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL];
    const char * c = topic;

    struct topic_s * t = self->topic;
    struct topic_s * subtopic;
    while (*c != 0) {
        if (!subtopic_get_str(&c, subtopic_str)) {
            return NULL;
        }
        subtopic = subtopic_find(t, subtopic_str);
        if (!subtopic) {
            if (!create) {
                return NULL;
            }
            EMBC_LOGD1("%s: create new topic %s", topic, subtopic_str);
            subtopic = topic_alloc(self, subtopic_str);
            subtopic->parent = t;
        }
        embc_list_add_tail(&t->children, &subtopic->item);
        t = subtopic;
    }
    return t;
}

struct embc_pubsub_s * embc_pubsub_initialize(uint32_t buffer_size) {
    EMBC_LOGI("initialize");
    struct embc_pubsub_s * self = (struct embc_pubsub_s *) embc_alloc_clr(sizeof(struct embc_pubsub_s) + buffer_size);
    EMBC_ASSERT_ALLOC(self);
    self->lock = lock_default;
    self->unlock = unlock_default;
    self->topic = topic_alloc(self, "");
    embc_list_initialize(&self->subscriber_free);
    embc_list_initialize(&self->msg_pend);
    embc_list_initialize(&self->msg_free);
    embc_mrb_init(&self->mrb, self->buffer, buffer_size);
    return self;
}

void subscriber_list_free(struct embc_list_s * list) {
    struct embc_list_s * item;
    struct subscriber_s * sub;
    embc_list_foreach(list, item) {
        sub = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
        embc_free(sub);
    }
    embc_list_initialize(list);
}

void msg_list_free(struct embc_list_s * list) {
    struct embc_list_s * item;
    struct message_s * msg;
    embc_list_foreach(list, item) {
        msg = EMBC_CONTAINER_OF(item, struct message_s, item);
        embc_free(msg);
    }
    embc_list_initialize(list);
}

void embc_pubsub_finalize(struct embc_pubsub_s * self) {
    EMBC_LOGI("finalize");

    if (self) {
        embc_pubsub_unlock unlock = self->unlock;
        void * lock_user_data = self->lock_user_data;
        self->lock(self->lock_user_data);
        topic_free(self, self->topic);
        subscriber_list_free(&self->subscriber_free);
        msg_list_free(&self->msg_pend);
        msg_list_free(&self->msg_free);
        embc_free(self);
        unlock(lock_user_data);
    }
}

void embc_pubsub_register_on_publish(struct embc_pubsub_s * self,
                                     embc_pubsub_on_publish_fn cbk_fn, void * cbk_user_data) {
    self->cbk_fn = cbk_fn;
    self->cbk_user_data = cbk_user_data;
}

void subscribe_traverse(struct topic_s * topic, char * topic_str, embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    size_t topic_str_len = strlen(topic_str);
    char * topic_str_last = topic_str + topic_str_len;
    if (((topic->value.type & EMBC_PUBSUB_DTYPE_MASK) != EMBC_PUBSUB_DTYPE_NULL) && (topic->value.type & EMBC_PUBSUB_DFLAG_RETAIN)) {
        cbk_fn(cbk_user_data, topic_str, &topic->value);
    }
    struct embc_list_s * item;
    struct topic_s * subtopic;
    embc_list_foreach(&topic->children, item) {
        subtopic = EMBC_CONTAINER_OF(item, struct topic_s, item);
        topic_str_append(topic_str, subtopic->name);
        subscribe_traverse(subtopic, topic_str, cbk_fn, cbk_user_data);
        *topic_str_last = 0;  // reset string to original
    }
}

int32_t embc_pubsub_subscribe(struct embc_pubsub_s * self, const char * topic, embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    char topic_str[EMBC_PUBSUB_TOPIC_LENGTH_MAX] = {0};
    if (!self || !cbk_fn) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    struct topic_s * t;
    lock(self);
    t = topic_find(self, topic, true);
    struct subscriber_s * sub = subscriber_alloc(self);
    sub->cbk_fn = cbk_fn;
    sub->cbk_user_data = cbk_user_data;
    embc_list_add_tail(&t->subscribers, &sub->item);

    topic_str_append(topic_str, topic);
    subscribe_traverse(t, topic_str, cbk_fn, cbk_user_data);
    unlock(self);
    return 0;
}

int32_t embc_pubsub_unsubscribe(struct embc_pubsub_s * self, const char * topic,
                                embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    struct topic_s * t = topic_find(self, topic, false);
    struct embc_list_s * item;
    struct subscriber_s * subscriber;
    int count = 0;
    if (!t) {
        return EMBC_ERROR_NOT_FOUND;
    }
    embc_list_foreach(&t->subscribers, item) {
        subscriber = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
        if ((subscriber->cbk_fn == cbk_fn) && (subscriber->cbk_user_data == cbk_user_data)) {
            embc_list_remove(item);
            subscriber_free(self, subscriber);
            ++count;
        }
    }
    if (!count) {
        return EMBC_ERROR_NOT_FOUND;
    }
    return 0;
}

static void publish(struct topic_s * topic, struct message_s * msg) {
    uint8_t status = 0;
    struct embc_list_s * item;
    struct subscriber_s * subscriber;
    while (topic) {
        embc_list_foreach(&topic->subscribers, item) {
            subscriber = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
            if ((msg->src_fn != subscriber->cbk_fn) || (msg->src_user_data != subscriber->cbk_user_data)) {
                uint8_t rv = subscriber->cbk_fn(subscriber->cbk_user_data, msg->name, &msg->value);
                if (!status && rv) {
                    status = rv;
                }
            }
        }
        topic = topic->parent;
    }
}

static bool is_ptr_type(uint8_t type) {
    switch (type & EMBC_PUBSUB_DTYPE_MASK) {
        case EMBC_PUBSUB_DTYPE_STR:   // intentional fall-through
        case EMBC_PUBSUB_DTYPE_JSON:  // intentional fall-through
        case EMBC_PUBSUB_DTYPE_BIN:
            return true;
        default:
            return false;
    }
}

static bool is_str_type(uint8_t type) {
    switch (type & EMBC_PUBSUB_DTYPE_MASK) {
        case EMBC_PUBSUB_DTYPE_STR:   // intentional fall-through
        case EMBC_PUBSUB_DTYPE_JSON:  // intentional fall-through
            return true;
        default:
            return false;
    }
}

int32_t embc_pubsub_publish(struct embc_pubsub_s * self,
        const char * topic, const struct embc_pubsub_value_s * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data) {

    bool do_copy = false;
    uint32_t size = value->size;
    if (is_ptr_type(value->type)) {
        if ((!value->size) && is_str_type(value->type)) {
            size = strlen(value->value.str) + 1;
        }
        if (0 == (value->type & EMBC_PUBSUB_DFLAG_CONST)) {
            if (value->type & EMBC_PUBSUB_DFLAG_RETAIN) {
                EMBC_LOGE("non-const retained ptr not allowed");
                return EMBC_ERROR_PARAMETER_INVALID;
            }
            do_copy = true;
            if (size > (self->mrb.buf_size / 2)) {
                EMBC_LOGE("too big for available buffer");
                return EMBC_ERROR_PARAMETER_INVALID;
            }
        }
    } else {
        size = 0;
    }

    lock(self);
    struct message_s * msg = msg_alloc(self);
    if (!topic_str_copy(msg->name, topic)) {
        unlock(self);
        return EMBC_ERROR_PARAMETER_INVALID;
    }

    msg->src_fn = src_fn;
    msg->src_user_data = src_user_data;
    msg->value = *value;
    msg->value.size = size;

    if (do_copy && size) {
        uint8_t *buf = embc_mrb_alloc(&self->mrb, size);
        if (!buf) { // full!
            embc_list_add_tail(&self->msg_free, &msg->item);
            unlock(self);
            return EMBC_ERROR_NOT_ENOUGH_MEMORY;
        }
        embc_memcpy(buf, value->value.str, size);
        msg->value.value.bin = buf;
    }

    embc_list_add_tail(&self->msg_pend, &msg->item);
    if (self->cbk_fn) {
        self->cbk_fn(self->cbk_user_data);
    }
    unlock(self);
    return 0;
}

int32_t embc_pubsub_meta(struct embc_pubsub_s * self, const char * topic, const char * meta_json) {
    size_t sz = strlen(topic);
    if (sz > 30) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    lock(self);
    struct message_s * msg = msg_alloc(self);
    if (!topic_str_copy(msg->name, topic)) {
        unlock(self);
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    embc_memcpy(msg->name, topic, sz);
    msg->name[sz] = '$';
    msg->name[sz + 1] = 0;
    msg->value.type = EMBC_PUBSUB_DFLAG_CONST | EMBC_PUBSUB_DFLAG_RETAIN | EMBC_PUBSUB_DTYPE_JSON;
    msg->value.value.str = meta_json;
    msg->value.size = sz + 2;
    embc_list_add_tail(&self->msg_pend, &msg->item);
    unlock(self);
    return 0;
}

int32_t embc_pubsub_query(struct embc_pubsub_s * self, const char * topic, struct embc_pubsub_value_s * value) {
    lock(self);  // embc_list_foreach not thread-safe.  Need mutex.
    struct topic_s * t = topic_find(self, topic, false);
    unlock(self);
    if (!t || (0 == (t->value.type & EMBC_PUBSUB_DFLAG_RETAIN))) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    if (value) {
        *value = t->value;
    }
    return 0;
}

static void metadata_publish(struct topic_s * topic, char * topic_str) {
    struct embc_list_s * item;
    struct subscriber_s * subscriber;

    if (!topic || !topic->meta) {
        return;
    }
    size_t idx = strlen(topic_str);
    if (idx == 0) {
        return;  // no metadata for root
    }
    if (topic_str[idx - 1] != '$') {
        topic_str[idx] = '$';
        topic_str[idx + 1] = 0;
    }
    struct embc_pubsub_value_s meta = embc_pubsub_cjson_r(topic->meta);
    while (topic) {
        embc_list_foreach(&topic->subscribers, item) {
            subscriber = EMBC_CONTAINER_OF(item, struct subscriber_s, item);
            subscriber->cbk_fn(subscriber->cbk_user_data, topic_str, &meta);
        }
        topic = topic->parent;
    }
    topic_str[idx] = 0;
}

static void metadata_request(struct topic_s * t, struct message_s * msg) {
    if (!t) {
        return;
    }
    char * topic_str = msg->name;
    size_t topic_str_len = strlen(topic_str);
    char * topic_str_last = topic_str + topic_str_len;
    struct embc_list_s * item;
    struct topic_s * subtopic;
    embc_list_foreach(&t->children, item) {
        subtopic = EMBC_CONTAINER_OF(item, struct topic_s, item);
        topic_str_append(topic_str, subtopic->name);
        if (msg->src_fn) {
            size_t idx = strlen(topic_str);
            topic_str[idx] = '$';
            topic_str[idx + 1] = 0;
            msg->src_fn(msg->src_user_data, topic_str, &embc_pubsub_cjson_r(subtopic->meta));
            topic_str[idx] = 0;
        } else {
            metadata_publish(subtopic, topic_str);
        }
        metadata_request(subtopic, msg);
        *topic_str_last = 0;  // reset string to original
    }
}

void embc_pubsub_process(struct embc_pubsub_s * self) {
    struct embc_list_s * item;
    struct message_s * msg;
    struct topic_s * t;
    uint8_t dtype;
    lock(self);
    embc_list_foreach(&self->msg_pend, item) {
        embc_list_remove(item);
        msg = EMBC_CONTAINER_OF(item, struct message_s, item);
        dtype = msg->value.type & EMBC_PUBSUB_DTYPE_MASK;
        switch (dtype) {
            case EMBC_PUBSUB_DTYPE_NULL: break;
            case EMBC_PUBSUB_DTYPE_STR: break;
            case EMBC_PUBSUB_DTYPE_JSON: break;
            case EMBC_PUBSUB_DTYPE_BIN: break;
            case EMBC_PUBSUB_DTYPE_U32: break;
            default:
                EMBC_LOGW("unsupported type for %s: %d", msg->name, (int) msg->value.type);
                goto free_item;
        }

        uint32_t name_sz = strlen(msg->name);  // excluding terminator
        if ((name_sz == 1) && (msg->name[0] == '$')) {
            // metadata request root
            t = self->topic;
            msg->name[0] = 0;
            metadata_request(t, msg);
        } else if ((name_sz > 1) && (msg->name[name_sz - 1] == '$') && (msg->name[name_sz - 2] == '/')) {
            // metadata request topic
            msg->name[name_sz - 2] = 0;
            t = topic_find(self, msg->name, false);
            metadata_request(t, msg);
        } else if (msg->name[name_sz - 1] == '$') {
            // metadata publish
            msg->name[name_sz - 1] = 0;
            t = topic_find(self, msg->name, true);
            if (t) {
                if ((dtype == EMBC_PUBSUB_DTYPE_JSON)
                        && (msg->value.type & EMBC_PUBSUB_DFLAG_RETAIN)
                        && (msg->value.type & EMBC_PUBSUB_DFLAG_CONST)) {
                    t->meta = msg->value.value.str;
                } else if (dtype == EMBC_PUBSUB_DTYPE_NULL) {
                    t->meta = NULL;
                }
                metadata_publish(t, msg->name);
            }
        } else {
            t = topic_find(self, msg->name, true);
            if (t) {
                // todo map alternate values to actual value using metadata
                t->value = msg->value;
                publish(t, msg);
            }
        }


        free_item:
        if (is_ptr_type(msg->value.type) && (0 == (msg->value.type & EMBC_PUBSUB_DFLAG_CONST))) {
            uint32_t sz = 0;
            uint8_t * buf = embc_mrb_pop(&self->mrb, &sz);
            if ((buf != msg->value.value.bin) || (sz != msg->value.size)) {
                EMBC_LOGE("internal msgbuf sync error");
            }
        }
        embc_list_add_tail(&self->msg_free, item);
    }
    unlock(self);
}

void embc_pubsub_register_lock(struct embc_pubsub_s * self, embc_pubsub_lock lock, embc_pubsub_unlock unlock, void * user_data) {
    self->lock = lock;
    self->unlock = unlock;
    self->lock_user_data = user_data;
}
