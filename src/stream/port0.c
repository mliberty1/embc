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

#include "embc/stream/port0.h"
#include "embc/stream/transport.h"
#include "embc/log.h"
#include "embc/ec.h"
#include "embc/platform.h"


static const char * PORT0_META = "{\"type\":\"oam\"}";


struct embc_port0_s {
    enum embc_port0_mode_e mode;
    struct embc_transport_s * transport;
    embc_transport_send_fn send_fn;
    const char * meta[EMBC_TRANSPORT_PORT_MAX + 1];
};

void embc_port0_on_event_cbk(struct embc_port0_s * self, enum embc_dl_event_e event) {
    (void) self;
    (void) event;
}

#define pack_req(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x00) | (((uint16_t) cmd_meta) << 8))

#define pack_rsp(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x08) | (((uint16_t) cmd_meta) << 8))

typedef void (*dispatch_fn)(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size);

void op_echo_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(ECHO, cmd_meta), msg, msg_size);
}

void op_meta_req(struct embc_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) msg;  // ignore
    (void) msg_size;
    uint16_t port_data = pack_rsp(META, cmd_meta);
    if ((cmd_meta <= EMBC_TRANSPORT_PORT_MAX) && (self->meta[cmd_meta])) {
        self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data,
                      (uint8_t *) self->meta[cmd_meta], strlen(self->meta[cmd_meta]) + 1);
    } else {
        uint8_t empty = 0;
        self->send_fn(self->transport, 0, EMBC_TRANSPORT_SEQ_SINGLE, port_data, &empty, 1);
    }
}

void embc_port0_on_recv_cbk(struct embc_port0_s * self,
                uint8_t port_id,
                enum embc_transport_seq_e seq,
                uint16_t port_data,
                uint8_t *msg, uint32_t msg_size) {
    if (port_id != 0) {
        return;
    }
    if (seq != EMBC_TRANSPORT_SEQ_SINGLE) {
        // all messages are single frames only.
        EMBC_LOGW("port0 received segmented message");
        return;
    }
    uint8_t cmd_meta = (port_data >> 8) & 0xff;
    bool req = (port_data & 0x08) == 0;
    uint8_t op = port_data & 0x07;
    dispatch_fn fn = NULL;


    if (req) {
        switch (op) {
            //case EMBC_PORT0_OP_STATUS:      fn = op_status_req; break;
            case EMBC_PORT0_OP_ECHO:        fn = op_echo_req; break;
            //case EMBC_PORT0_OP_TIMESYNC:    fn = op_timesync_req; break;
            case EMBC_PORT0_OP_META:        fn = op_meta_req; break;
            //case EMBC_PORT0_OP_RAW:         fn = op_raw_req; break;
            default:
                break;
        }
    } else {
        switch (op) {
            //case EMBC_PORT0_OP_STATUS:      fn = op_status_rsp; break;
            //case EMBC_PORT0_OP_ECHO:        fn = op_echo_rsp; break;
            //case EMBC_PORT0_OP_TIMESYNC:    fn = op_timesync_rsp; break;
            //case EMBC_PORT0_OP_META:        fn = op_meta_rsp; break;
            //case EMBC_PORT0_OP_RAW:         fn = op_raw_rsp; break;
            default:
                break;
        }
    }
    if (fn == NULL) {
        EMBC_LOGW("unsupported: mode=%d, req=%d, op=%d", (int) self->mode, (int) req, (int) op);
    } else {
        fn(self, cmd_meta, msg, msg_size);
    }
}

struct embc_port0_s * embc_port0_initialize(enum embc_port0_mode_e mode,
        struct embc_transport_s * transport, embc_transport_send_fn send_fn) {
    if (mode != EMBC_PORT0_MODE_CLIENT) {
        EMBC_LOGE("only client mode currently supported");
        return NULL;
    }
    struct embc_port0_s * p = embc_alloc_clr(sizeof(struct embc_port0_s));
    EMBC_ASSERT_ALLOC(p);
    p->mode = mode;
    p->transport = transport;
    p->send_fn = send_fn;
    p->meta[0] = PORT0_META;
    return p;
}

void embc_port0_finalize(struct embc_port0_s * self) {
    if (self) {
        embc_free(self);
    }
}

int32_t embc_port0_meta_set(struct embc_port0_s * self, uint8_t port_id, const char * meta) {
    if ((port_id < 1) || port_id > EMBC_TRANSPORT_PORT_MAX) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->meta[port_id] = meta;
    return 0;
}

const char * embc_port0_meta_get(struct embc_port0_s * self, uint8_t port_id) {
    if (port_id > EMBC_TRANSPORT_PORT_MAX) {
        return NULL;
    }
    return self->meta[port_id];
}
