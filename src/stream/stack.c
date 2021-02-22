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

#include "embc/stream/stack.h"
#include "embc/platform.h"
#include "embc/ec.h"
#include "embc/log.h"
#include <inttypes.h>


struct embc_stack_s * embc_stack_initialize(
        struct embc_dl_config_s const * config,
        enum embc_port0_mode_e port0_mode,
        const char * port0_topic_prefix,
        struct embc_evm_api_s * evm_api,
        struct embc_dl_ll_s const * ll_instance,
        struct embc_pubsub_s * pubsub)  {

    struct embc_stack_s * self = embc_alloc_clr(sizeof(struct embc_stack_s));
    self->pubsub = pubsub;

    self->dl = embc_dl_initialize(config, evm_api, ll_instance);
    if (!self->dl) {
        embc_stack_finalize(self);
        return NULL;
    }

    self->transport = embc_transport_initialize((embc_transport_ll_send) embc_dl_send, self->dl);
    if (!self->transport) {
        embc_stack_finalize(self);
        return NULL;
    }

    struct embc_dl_api_s dl_api = {
            .user_data = self->transport,
            .event_fn = (embc_dl_event_fn) embc_transport_on_event_cbk,
            .recv_fn = (embc_dl_recv_fn) embc_transport_on_recv_cbk,
    };
    embc_dl_register_upper_layer(self->dl, &dl_api);

    self->port0 = embc_port0_initialize(port0_mode, self->dl, self->transport, embc_transport_send,
                                        pubsub, port0_topic_prefix);
    if (!self->port0) {
        embc_stack_finalize(self);
        return NULL;
    }
    if (embc_transport_port_register(self->transport, 0,
                                     EMBC_PORT0_META,
                                     (embc_transport_event_fn) embc_port0_on_event_cbk,
                                     (embc_transport_recv_fn) embc_port0_on_recv_cbk,
                                      self->port0)) {
        embc_stack_finalize(self);
        return NULL;
    }

    enum embc_pubsubp_mode_e pmode;
    switch (port0_mode) {
        case EMBC_PORT0_MODE_CLIENT: pmode = EMBC_PUBSUBP_MODE_UPSTREAM; break;
        case EMBC_PORT0_MODE_SERVER: pmode = EMBC_PUBSUBP_MODE_DOWNSTREAM; break;
        default:
            embc_stack_finalize(self);
            return NULL;
    }
    self->pubsub_port = embc_pubsubp_initialize(pubsub, pmode);
    if (!self->pubsub_port) {
        embc_stack_finalize(self);
        return NULL;
    }
    if (embc_pubsubp_transport_register(self->pubsub_port, 1, self->transport)) {
        embc_stack_finalize(self);
        return NULL;
    }

    return self;
}

int32_t embc_stack_finalize(struct embc_stack_s * self) {
    if (self) {
        if (self->dl) {
            embc_dl_finalize(self->dl);
            self->dl = NULL;
        }
        if (self->transport) {
            embc_transport_finalize(self->transport);
            self->transport = NULL;
        }
        if (self->port0) {
            embc_port0_finalize(self->port0);
            self->port0 = NULL;
        }
        if (self->pubsub_port) {
            embc_pubsubp_finalize(self->pubsub_port);
            self->pubsub_port = NULL;
        }
        embc_free(self);
    }
    return 0;
}

void embc_stack_process(struct embc_stack_s * self) {
    embc_dl_process(self->dl);
}

void embc_stack_mutex_set(struct embc_stack_s * self, embc_os_mutex_t mutex) {
    embc_dl_register_mutex(self->dl, mutex);
}
