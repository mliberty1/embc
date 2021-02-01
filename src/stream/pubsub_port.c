/*
 * Copyright 2021 Jetperch LLC
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
#include "embc/stream/pubsub_port.h"
#include "embc/bbuf.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/cstr.h"

struct embc_pubsubp_s {
    embc_pubsub_publish_fn pubsub_publish_fn;
    struct embc_pubsub_s * pubsub;
    uint8_t port_id;
    embc_transport_send_fn transport_send_fn;
    struct embc_transport_s * transport;
    uint8_t msg[EMBC_FRAMER_PAYLOAD_MAX_SIZE];
};

struct embc_pubsubp_s * embc_pubsubp_initialize() {
    struct embc_pubsubp_s * self = embc_alloc_clr(sizeof(struct embc_pubsubp_s));
    EMBC_ASSERT_ALLOC(self);
    return self;
}

void embc_pubsubp_finalize(struct embc_pubsubp_s * self) {
    if (self) {
        embc_free(self);
    }
}

void embc_pubsubp_pubsub_register(struct embc_pubsubp_s * self,
                                  embc_pubsub_publish_fn publish_fn, struct embc_pubsub_s * pubsub) {
    self->pubsub_publish_fn = NULL;
    self->pubsub = pubsub;
    self->pubsub_publish_fn = publish_fn;
}


void embc_pubsubp_transport_register(struct embc_pubsubp_s * self,
                                     uint8_t port_id,
                                     embc_transport_send_fn send_fn, struct embc_transport_s * transport) {
    self->transport_send_fn = NULL;
    self->port_id = port_id;
    self->transport = transport;
    self->transport_send_fn = send_fn;
}

void embc_pubsubp_on_event(struct embc_pubsubp_s *self, enum embc_dl_event_e event)  {
    (void) self;
    (void) event;
    // do nothing
}

void embc_pubsubp_on_recv(struct embc_pubsubp_s *self,
                          uint8_t port_id,
                          enum embc_transport_seq_e seq,
                          uint16_t port_data,
                          uint8_t *msg, uint32_t msg_size) {
    if (!self->pubsub_publish_fn) {
        return;
    }
    if (port_id != self->port_id) {
        EMBC_LOGW("port_id mismatch: %d != %d", (int) port_id, (int) self->port_id);
        return;
    }
    if (seq != EMBC_TRANSPORT_SEQ_SINGLE) {
        EMBC_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    if (port_data != 0) {
        EMBC_LOGW("invalide port_data: %d", (int) port_data);
        return;
    }
    if (msg_size < 3) {
        EMBC_LOGW("msg too small");
        return;
    }
    uint8_t payload_type = (msg[0] >> 5) & 0x07;
    uint8_t topic_len = (msg[0] & 0x1f) + 1;  // 32 max
    char * topic = (char *) (msg + 1);

    uint32_t sz = 2 + topic_len;
    if (msg_size < sz) {
        EMBC_LOGW("msg too small: %d < %d", (int) msg_size, (int) sz);
        return;
    }
    if (topic[topic_len - 1]) {
        EMBC_LOGW("topic invalid");
        return;
    }

    uint8_t *payload = msg + sz - 1;
    uint8_t payload_len = *payload++;
    sz += payload_len;
    if (!payload_len || (msg_size < sz)) {
        EMBC_LOGW("msg too small: %d < %d", (int) msg_size, (int) sz);
        return;
    }

    // parse message
    struct embc_pubsub_value_s value;
    value.type = payload_type;
    switch (payload_type) {
        case EMBC_PUBSUB_DTYPE_NULL:
            break;
        case EMBC_PUBSUB_DTYPE_STR:
            if (payload[payload_len - 1]) {
                EMBC_LOGW("invalid payload string");
            } else {
                EMBC_LOGW("CSTR not fully supported yet");
                // WARNING todo support this correctly
                value.value.str = (char *) payload;  // not const and persistent!!!
            }
            break;
        case EMBC_PUBSUB_DTYPE_U32:
            if (payload_len != 4) {
                EMBC_LOGW("invalid payload u32");
            } else {
                value.value.u32 = EMBC_BBUF_DECODE_U32_LE(payload);
            }
            break;

        default:
            EMBC_LOGW("unsupported type: %d", (int) payload_type);
            return;
    }
    self->pubsub_publish_fn(self->pubsub, topic, &value,
                            (embc_pubsub_subscribe_fn) embc_pubsubp_on_update, self);
}

uint8_t embc_pubsubp_on_update(struct embc_pubsubp_s *self,
                               const char * topic, const struct embc_pubsub_value_s * value) {
    if (!self->transport_send_fn) {
        return 0;
    }
    uint8_t topic_len = 0;
    char * p = (char *) (self->msg + 1);
    while (*topic) {
        if (topic_len >= (EMBC_PUBSUB_TOPIC_LENGTH_MAX - 1)) {
            EMBC_LOGW("topic too long");
            return EMBC_ERROR_PARAMETER_INVALID;
        }
        *p++ = *topic++;
        ++topic_len;
    }
    *p++ = 0;       // add string terminator
    ++topic_len;

    self->msg[0] = ((topic_len - 1) & 0x1f) | ((value->type & 0x07) << 5);
    uint8_t payload_sz_max = sizeof(self->msg) - ((uint8_t *) (p + 1) - self->msg);
    uint8_t payload_sz = 0;
    uint8_t * hdr = (uint8_t *) p++;

    switch (value->type) {
        case EMBC_PUBSUB_DTYPE_NULL:
            break;
        case EMBC_PUBSUB_DTYPE_STR:  // intentional fall-through
        case EMBC_PUBSUB_DTYPE_JSON: {
            const char * s = value->value.str;
            while (*s) {
                if (payload_sz >= (payload_sz_max - 1)) {
                    EMBC_LOGW("payload full");
                    return EMBC_ERROR_PARAMETER_INVALID;
                }
                *p++ = *s++;
                ++payload_sz;
            }
            *p++ = 0;       // add string terminator
            ++payload_sz;
            break;
        }
        case EMBC_PUBSUB_DTYPE_BIN: {
            if (payload_sz_max < value->size) {
                EMBC_LOGW("payload full");
                return EMBC_ERROR_PARAMETER_INVALID;
            }
            const uint8_t * s = value->value.bin;
            for (uint8_t sz = 0; sz < value->size; ++sz) {
                *p++ = *s++;
                ++payload_sz;
            }
            break;
        }
        case EMBC_PUBSUB_DTYPE_U32:
            if (payload_sz_max < 4) {
                EMBC_LOGW("payload full");
                return EMBC_ERROR_PARAMETER_INVALID;
            } else {
                EMBC_BBUF_ENCODE_U32_LE(p, value->value.u32);
                payload_sz = 4;
            }
            break;
        default:
            EMBC_LOGW("unsupported type: %d", (int) value->type);
            return EMBC_ERROR_PARAMETER_INVALID;
    }
    *hdr = (uint8_t) payload_sz;
    self->transport_send_fn(self->transport, self->port_id, EMBC_TRANSPORT_SEQ_SINGLE,
                            0, self->msg, 2 + topic_len + payload_sz);
    return 0;
}
