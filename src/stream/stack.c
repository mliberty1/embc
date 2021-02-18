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


static void on_event(void *user_data, enum embc_dl_event_e event) {
    struct embc_stack_s * self = (struct embc_stack_s *) user_data;
    EMBC_LOGI("event %d", (int) event);
    if ((EMBC_DL_EV_RX_RESET_REQUEST == event) && (self->port0_mode == EMBC_PORT0_MODE_CLIENT)) {
        embc_dl_reset_tx_from_event(self->dl);
    }
    embc_transport_on_event_cbk(self->transport, event);
}

static void on_recv(void *user_data, uint32_t metadata, uint8_t *msg, uint32_t msg_size) {
    struct embc_stack_s * self = (struct embc_stack_s *) user_data;
    EMBC_LOGI("on_recv(metadata=%" PRIu32 " sz=%" PRIu32 ")", metadata, msg_size);
    embc_transport_on_recv_cbk(self->transport, metadata, msg, msg_size);
}

struct embc_stack_s * embc_stack_initialize(
        struct embc_dl_config_s const * config,
        enum embc_port0_mode_e port0_mode,
        const char * port0_topic_prefix,
        struct embc_evm_api_s const * evm,
        struct embc_dl_ll_s const * ll_instance,
        struct embc_pubsub_s * pubsub) {

    struct embc_stack_s * self = embc_alloc_clr(sizeof(struct embc_stack_s));
    self->pubsub = pubsub;

    self->dl = embc_dl_initialize(config, evm, ll_instance);
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
            .user_data = self,
            .event_fn = on_event,
            .recv_fn = on_recv,
    };
    embc_dl_register_upper_layer(self->dl, &dl_api);

    self->port0 = embc_port0_initialize(port0_mode, self->transport, embc_transport_send,
                                        pubsub, port0_topic_prefix);
    self->port0_mode = port0_mode;
    if (!self->port0) {
        embc_stack_finalize(self);
        return NULL;
    }

    if (embc_transport_port_register(self->transport, 0,
                                     (embc_transport_event_fn) embc_port0_on_event_cbk,
                                     (embc_transport_recv_fn) embc_port0_on_recv_cbk,
                                      self->port0)) {
        embc_stack_finalize(self);
        return NULL;
    }

    // pubsub_port created as needed.

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
