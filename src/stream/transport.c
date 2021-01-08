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

#include "embc/stream/transport.h"
#include "embc/stream/data_link.h"
#include "embc/log.h"
#include "embc/ec.h"
#include "embc/platform.h"


struct port_s {
    void *user_data;
    embc_transport_event_fn event_fn;
    embc_transport_recv_fn recv_fn;
};

/// The transport instance.
struct embc_transport_s {
    /// The underlying data link layer.
    struct embc_dl_s * dl;
    /// The defined ports.
    struct port_s ports[EMBC_TRANSPORT_PORT_MAX];
};

static void on_event(void *user_data, enum embc_dl_event_e event) {
    struct embc_transport_s * self = (struct embc_transport_s *) user_data;
    for (uint32_t i = 0; i < EMBC_TRANSPORT_PORT_MAX; ++i) {
        if (self->ports[i].event_fn) {
            self->ports[i].event_fn(self->ports[i].user_data, event);
        }
    }
}

static void on_recv(void *user_data, uint32_t metadata,
                    uint8_t *msg, uint32_t msg_size) {
    struct embc_transport_s * self = (struct embc_transport_s *) user_data;
    uint8_t port_id = metadata & EMBC_TRANSPORT_PORT_MAX;
    enum embc_transport_seq_e seq = (enum embc_transport_seq_e) ((metadata >> 6) & 3);
    uint16_t port_data = (uint16_t) ((metadata >> 8) & 0xffff);
    if (self->ports[port_id].recv_fn) {
        self->ports[port_id].recv_fn(self->ports[port_id].user_data, port_id, seq, port_data, msg, msg_size);
    }
}

struct embc_transport_s * embc_transport_initialize(struct embc_dl_s * data_link) {
    if (!data_link) {
        return 0;
    }
    struct embc_transport_s * t = embc_alloc_clr(sizeof(struct embc_transport_s));
    EMBC_ASSERT_ALLOC(t);
    t->dl = data_link;

    struct embc_dl_api_s api = {
            .user_data = t,
            .event_fn = on_event,
            .recv_fn = on_recv,
    };
    embc_dl_register_upper_layer(t->dl, &api);

    return t;
}

void embc_transport_finalize(struct embc_transport_s * self) {
    if (self) {
        embc_free(self);
    }
}

int32_t embc_transport_port_register(struct embc_transport_s * self, uint8_t port_id,
                                     embc_transport_event_fn event_fn,
                                     embc_transport_recv_fn recv_fn,
                                     void * user_data) {
    if (port_id > EMBC_TRANSPORT_PORT_MAX) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    self->ports[port_id].event_fn = NULL;
    self->ports[port_id].recv_fn = NULL;
    self->ports[port_id].user_data = user_data;
    self->ports[port_id].event_fn = event_fn;
    self->ports[port_id].recv_fn = recv_fn;
    return 0;
}

int32_t embc_transport_send(struct embc_transport_s * self,
                            uint8_t port_id,
                            enum embc_transport_seq_e seq,
                            uint16_t port_data,
                            uint8_t const *msg, uint32_t msg_size) {
    if (port_id > EMBC_TRANSPORT_PORT_MAX) {
        return EMBC_ERROR_PARAMETER_INVALID;
    }
    uint32_t metadata = ((seq & 0x3) << 6)
        | (port_id & EMBC_TRANSPORT_PORT_MAX)
        | (((uint32_t) port_data) << 8);
    return embc_dl_send(self->dl, metadata, msg, msg_size);
}
