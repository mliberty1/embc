/*
 * Copyright 2017-2018 Jetperch LLC
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
 
#include "embc/event_manager.h"
#include "embc/ec.h"
#include "embc/collections/list.h"
#include "embc/time.h"
#include "embc/platform.h"
#include <stdlib.h>


struct event_s {
    int32_t event_id;
    int64_t timestamp;
    embc_evm_callback cbk_fn;
    void * cbk_user_data;
    struct embc_list_s node;
};

#define EVGET(item) embc_list_entry(item, struct event_s, node)

struct embc_evm_s {
    int32_t event_counter;
    embc_os_mutex_t mutex;
    struct embc_list_s events_pending;
    struct embc_list_s events_free;
};

static inline void lock(struct embc_evm_s * self) {
    if (self->mutex) {
        embc_os_mutex_lock(self->mutex);
    }
}

static inline void unlock(struct embc_evm_s * self) {
    if (self->mutex) {
        embc_os_mutex_unlock(self->mutex);
    }
}

struct embc_evm_s * embc_evm_allocate() {
    struct embc_evm_s * self = embc_alloc_clr(sizeof(struct embc_evm_s));
    if (self) {
        embc_list_initialize(&self->events_pending);
        embc_list_initialize(&self->events_free);
    }
    return self;
}

static void event_list_free(struct embc_list_s * list) {
    struct embc_list_s * item;
    struct event_s * ev;
    embc_list_foreach(list, item) {
        ev = EVGET(item);
        embc_free(ev);
    }
    embc_list_initialize(list);
}

void embc_evm_free(struct embc_evm_s * self) {
    if (self) {
        embc_os_mutex_t mutex = self->mutex;
        lock(self);
        event_list_free(&self->events_pending);
        event_list_free(&self->events_free);
        embc_free(self);
        if (mutex) {
            embc_os_mutex_unlock(mutex);
        }
    }
}

int32_t embc_evm_schedule(struct embc_evm_s * self, int64_t timestamp,
                          embc_evm_callback cbk_fn, void * cbk_user_data) {
    struct event_s * ev;
    lock(self);
    if (embc_list_is_empty(&self->events_free)) {
        ++self->event_counter;
        ev = embc_alloc_clr(sizeof(struct event_s));
        ev->event_id = self->event_counter;
    } else {
        ev = EVGET(embc_list_remove_head(&self->events_free));
    }
    embc_list_initialize(&ev->node);
    ev->timestamp = timestamp;
    ev->cbk_fn = cbk_fn;
    ev->cbk_user_data = cbk_user_data;

    struct embc_list_s * node;
    struct event_s * ev_next;
    embc_list_foreach(&self->events_pending, node) {
        ev_next = EVGET(node);
        if (ev->timestamp < ev_next->timestamp) {
            embc_list_insert_before(node, &ev->node);
            unlock(self);
            return ev->event_id;
        }
    }
    embc_list_add_tail(&self->events_pending, &ev->node);
    unlock(self);
    return ev->event_id;
}

int32_t embc_evm_cancel(struct embc_evm_s * self, int32_t event_id) {
    struct embc_list_s * node;
    struct event_s * ev;
    lock(self);
    embc_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->event_id == event_id) {
            embc_list_remove(node);
            embc_list_add_tail(&self->events_free, node);
            break;
        }
    }
    unlock(self);
    return 0;
}

int64_t embc_evm_time_next(struct embc_evm_s * self) {
    int64_t rv;
    lock(self);
    if (embc_list_is_empty(&self->events_pending)) {
        rv = EMBC_TIME_MIN;
    } else {
        rv = EVGET(embc_list_peek_head(&self->events_pending))->timestamp;
    }
    unlock(self);
    return rv;
}

int64_t embc_evm_interval_next(struct embc_evm_s * self, int64_t time_current) {
    lock(self);
    if (embc_list_is_empty(&self->events_pending)) {
        unlock(self);
        return -1;
    }
    struct event_s * ev = EVGET(embc_list_peek_head(&self->events_pending));
    if (ev->timestamp <= time_current) {
        unlock(self);
        return 0;
    } else {
        unlock(self);
        return ev->timestamp - time_current;
    }
}

int32_t embc_evm_process(struct embc_evm_s * self, int64_t time_current) {
    struct embc_list_s * node;
    struct event_s * ev;
    int32_t count = 0;
    lock(self);
    embc_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->timestamp > time_current) {
            break;
        }
        embc_list_remove(node);
        unlock(self);
        ev->cbk_fn(ev->cbk_user_data, ev->event_id);
        lock(self);
        embc_list_add_tail(&self->events_free, node);
        ++count;
    }
    unlock(self);
    return count;
}

int64_t timestamp_default(struct embc_evm_s * self) {
    (void) self;
    return embc_time_rel();
}

void embc_evm_register_mutex(struct embc_evm_s * self, embc_os_mutex_t mutex) {
    self->mutex = mutex;
}

int32_t embc_evm_api_config(struct embc_evm_s * self, struct embc_evm_api_s * api) {
    if (!self || !api) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    api->evm = self;
    api->timestamp = timestamp_default;
    api->schedule = (embc_evm_schedule_fn) embc_evm_schedule;
    api->cancel = (embc_evm_cancel_fn) embc_evm_cancel;
    return 0;
}
