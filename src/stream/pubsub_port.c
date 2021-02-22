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

enum state_e {
    ST_INIT,
    ST_ACTIVE,
};

struct embc_pubsubp_s {
    uint8_t state;
    uint8_t port_id;
    uint8_t mode;
    struct embc_pubsub_s * pubsub;
    struct embc_transport_s * transport;
    uint8_t msg[EMBC_FRAMER_PAYLOAD_MAX_SIZE];
};

const char EMBC_PUBSUBP_META[] = "{\"type\":\"pubsub\"}";

struct embc_pubsubp_s * embc_pubsubp_initialize(struct embc_pubsub_s * pubsub, enum embc_pubsubp_mode_e mode) {
    struct embc_pubsubp_s * self = embc_alloc_clr(sizeof(struct embc_pubsubp_s));
    EMBC_ASSERT_ALLOC(self);
    self->mode = mode;
    self->pubsub = pubsub;
    return self;
}

void embc_pubsubp_finalize(struct embc_pubsubp_s * self) {
    if (self) {
        embc_free(self);
    }
}

static int32_t connect(struct embc_pubsubp_s * self) {
    int32_t rc = 0;
    if (self->state != ST_INIT) {
        return 0;
    }

    if (self->mode == EMBC_PUBSUBP_MODE_UPSTREAM) {
        rc = embc_pubsub_subscribe_link(self->pubsub, "",
                                        (embc_pubsub_subscribe_fn) embc_pubsubp_on_update,
                                        self);
        if (rc) {
            EMBC_LOGE("pubsub subscribe failed");
            return rc;
        }
    }
    self->state = ST_ACTIVE;

    return 0;
}

int32_t embc_pubsubp_transport_register(struct embc_pubsubp_s * self,
                                        uint8_t port_id,
                                        struct embc_transport_s * transport) {
    self->port_id = port_id;
    self->transport = transport;
    int32_t rc = embc_transport_port_register(
            transport,
            port_id,
            EMBC_PUBSUBP_META,
            (embc_transport_event_fn) embc_pubsubp_on_event,
            (embc_transport_recv_fn) embc_pubsubp_on_recv,
            self);
    if (rc) {
        return rc;
    }
    return connect(self);
}

void embc_pubsubp_on_event(struct embc_pubsubp_s *self, enum embc_dl_event_e event) {
    (void) self;
    (void) event;
}

void embc_pubsubp_on_recv(struct embc_pubsubp_s *self,
                          uint8_t port_id,
                          enum embc_transport_seq_e seq,
                          uint16_t port_data,
                          uint8_t *msg, uint32_t msg_size) {
    if (!self->pubsub) {
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
    uint8_t type = (port_data >> 8) & 0xff;
    uint8_t dtype = type & EMBC_PUBSUB_DTYPE_MASK;
    if ((port_data & 0x00ff) != 0) {
        EMBC_LOGW("invalid port_data: %d", (int) port_data);
        return;
    }
    if (msg_size < 3) {
        EMBC_LOGW("msg too small");
        return;
    }
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
    value.type = type;
    switch (dtype) {
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
            EMBC_LOGW("unsupported type: %d", (int) dtype);
            return;
    }
    embc_pubsub_publish(self->pubsub, topic, &value,
                        (embc_pubsub_subscribe_fn) embc_pubsubp_on_update, self);
}

uint8_t embc_pubsubp_on_update(struct embc_pubsubp_s *self,
                               const char * topic, const struct embc_pubsub_value_s * value) {
    if (!self->transport) {
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

    uint8_t dtype = value->type & EMBC_PUBSUB_DTYPE_MASK;
    uint8_t dflag = value->type & EMBC_PUBSUB_DFLAG_MASK;
    dflag &= ~EMBC_PUBSUB_DFLAG_CONST;  // remove const, only applicable locally
    uint16_t port_data = 0 | (((uint16_t) (dflag | dtype)) << 8);

    self->msg[0] = ((topic_len - 1) & 0x1f);
    uint8_t payload_sz_max = (uint8_t) (sizeof(self->msg) - ((uint8_t *) (p + 1) - self->msg));
    uint8_t payload_sz = 0;
    uint8_t * hdr = (uint8_t *) p++;

    switch (dtype) {
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
    embc_transport_send(self->transport, self->port_id, EMBC_TRANSPORT_SEQ_SINGLE,
                        port_data, self->msg, 2 + topic_len + payload_sz);
    return 0;
}
