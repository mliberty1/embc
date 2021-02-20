/*
 * Copyright 2014-2020 Jetperch LLC
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

#include "embc/host/comm.h"
#include "embc/host/uart_thread.h"
#include "embc/stream/data_link.h"
#include "embc/stream/pubsub.h"
#include "embc/stream/stack.h"
#include "embc/os/mutex.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/platform.h"
#include "embc/time.h"
#include <stdio.h>
#include <windows.h>


// On a host computer, make plenty big
#define PUBSUB_BUFFER_SIZE (2000000)


struct embc_comm_s {
    struct embc_stack_s * stack;
    struct embc_uartt_s * uart;
    struct embc_pubsub_s * pubsub;
    embc_os_mutex_t pubsub_mutex;
    embc_pubsub_subscribe_fn subscriber_fn;
    void * subscriber_user_data;
};

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct embc_comm_s * self = (struct embc_comm_s *) user_data;
    if (embc_uartt_send_available(self->uart) >= buffer_size) {
        embc_uartt_send(self->uart, buffer, buffer_size);
    }
}

static uint32_t ll_send_available(void * user_data) {
    struct embc_comm_s * self = (struct embc_comm_s *) user_data;
    return embc_uartt_send_available(self->uart);
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    struct embc_comm_s * self = (struct embc_comm_s *) user_data;
    embc_dl_ll_recv(self->stack->dl, buffer, buffer_size);
}

struct embc_comm_s * embc_comm_initialize(struct embc_dl_config_s const * config,
                                          const char * device,
                                          uint32_t baudrate,
                                          embc_pubsub_subscribe_fn cbk_fn,
                                          void * cbk_user_data,
                                          const char * topics) {
    if (!cbk_fn) {
        EMBC_LOGW("Must provide cbk_fn");
        return NULL;
    }
    struct embc_comm_s * self = embc_alloc_clr(sizeof(struct embc_comm_s));
    if (!self) {
        return NULL;
    }

    EMBC_LOGI("embc_comm_initialize(%s, %d)", device, (int) baudrate);
    self->subscriber_fn = cbk_fn;
    self->subscriber_user_data = cbk_user_data;
    self->pubsub = embc_pubsub_initialize(PUBSUB_BUFFER_SIZE);
    if (!self->pubsub) {
        goto on_error;
    }

    self->pubsub_mutex = embc_os_mutex_alloc();
    if (!self->pubsub_mutex) {
        goto on_error;
    }
    embc_pubsub_register_mutex(self->pubsub, self->pubsub_mutex);

    struct embc_dl_ll_s ll = {
            .user_data = self,
            .send = ll_send,
            .send_available = ll_send_available,
    };

    struct uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_size_total = 3 * EMBC_FRAMER_MAX_SIZE,
            .buffer_size = EMBC_FRAMER_MAX_SIZE,
            .recv_buffer_count = 16,
            .recv_fn = on_uart_recv,
            .recv_user_data = self,
    };

    self->uart = embc_uartt_initialize(device, &uart_config);
    if (!self->uart) {
        goto on_error;
    }

    struct embc_evm_api_s evm_api;
    embc_uartt_evm_api(self->uart, &evm_api);

    self->stack = embc_stack_initialize(config, EMBC_PORT0_MODE_SERVER, "h/c/", &evm_api, &ll,
            self->pubsub, topics);
    if (!self->stack) {
        goto on_error;
    }

    if (embc_uartt_start(self->uart, (embc_uartt_process_fn) embc_stack_process, self->stack)) {
        goto on_error;
    }

    embc_os_mutex_t mutex;
    embc_uartt_mutex(self->uart, &mutex);
    embc_stack_mutex_set(self->stack, mutex);

    return self;

on_error:
    embc_comm_finalize(self);
    return NULL;
}

void embc_comm_finalize(struct embc_comm_s * self) {
    if (self) {
        embc_uartt_stop(self->uart);
        if (self->stack) {
            embc_stack_finalize(self->stack);
            self->stack = NULL;
        }
        if (self->uart) {
            embc_uartt_finalize(self->uart);
            self->uart = NULL;
        }
        if (self->pubsub) {
            embc_pubsub_finalize(self->pubsub);
            self->pubsub = NULL;
        }
        if (self->pubsub_mutex) {
            embc_os_mutex_free(self->pubsub_mutex);
            self->pubsub_mutex = NULL;
        }
        embc_free(self);
    }
}

int32_t embc_comm_publish(struct embc_comm_s * self,
                          const char * topic, const struct embc_pubsub_value_s * value) {
    return embc_pubsub_publish(self->pubsub, topic, value, self->subscriber_fn, self->subscriber_user_data);
}

int32_t embc_comm_query(struct embc_comm_s * self, const char * topic, struct embc_pubsub_value_s * value) {
    return embc_pubsub_query(self->pubsub, topic, value);
}

int32_t embc_comm_status_get(
        struct embc_comm_s * self,
        struct embc_dl_status_s * status) {
    return embc_dl_status_get(self->stack->dl, status);
}
