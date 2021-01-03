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

#include "embc/host/uart_data_link.h"
#include "embc/host/uart.h"
#include "embc/stream/data_link.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/platform.h"
#include <stdio.h>
#include <windows.h>

struct embc_udl_s {
    embc_udl_process_fn process_fn;
    void * process_user_data;
    HANDLE thread;
    HANDLE mutex;
    volatile int quit;
    struct embc_dl_s * dl;
    struct uart_s * uart;
};

static void lock(void * user_data) {
    struct embc_udl_s * self = (struct embc_udl_s *) user_data;
    DWORD rc = WaitForSingleObject(self->mutex, 1000);
    if (WAIT_OBJECT_0 != rc) {
        EMBC_LOGE("mutex lock failed");
    }
}

static void unlock(void * user_data) {
    struct embc_udl_s * self = (struct embc_udl_s *) user_data;
    if (! ReleaseMutex(self->mutex)) {
        EMBC_LOGE("mutex unlock failed");
    }
}

static uint32_t ll_time_get_ms(void * user_data) {
    (void) user_data;
    return embc_time_get_ms();
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct embc_udl_s * self = (struct embc_udl_s *) user_data;
    if (uart_send_available(self->uart) >= buffer_size) {
        uart_write(self->uart, buffer, buffer_size);
    }
}

static uint32_t ll_send_available(void * user_data) {
    struct embc_udl_s * self = (struct embc_udl_s *) user_data;
    return uart_send_available(self->uart);
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    struct embc_udl_s * self = (struct embc_udl_s *) user_data;
    embc_dl_ll_recv(self->dl, buffer, buffer_size);
}

struct embc_udl_s * embc_udl_initialize(struct embc_dl_config_s const * config,
                                        const char * uart_device,
                                        uint32_t baudrate) {
    char dev_str[1024];
    struct embc_udl_s * self = embc_alloc_clr(sizeof(struct embc_udl_s));
    if (!self) {
        return NULL;
    }

    EMBC_LOGI("embc_udl_initialize(%s, %d)", uart_device, (int) baudrate);
    snprintf(dev_str, sizeof(dev_str), "\\\\.\\%s", uart_device);

    struct embc_dl_ll_s ll = {
            .user_data = self,
            .time_get_ms = ll_time_get_ms,
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

    self->uart = uart_alloc();
    if (!self->uart) {
        embc_free(self);
        return NULL;
    }
    int32_t rv = uart_open(self->uart, dev_str, &uart_config);
    if (rv) {
        embc_free(self->uart);
        embc_free(self);
        return NULL;
    }

    self->dl = embc_dl_initialize(config, &ll);
    if (!self->dl) {
        embc_free(self->uart);
        embc_free(self);
        return NULL;
    }

    embc_dl_register_lock(self->dl, lock, unlock, self);
    return self;
}

DWORD WINAPI udl_task(LPVOID lpParam) {
    uint32_t handle_count;
    HANDLE handles[16];

    struct embc_udl_s * self = (struct embc_udl_s *) lpParam;
    while (!self->quit) {
        handle_count = 0;
        uart_handles(self->uart, &handle_count, &handles[handle_count]);
        if (handle_count) {
            WaitForMultipleObjects(handle_count, handles, FALSE, 1);
        } else {
            Sleep(1);
        }

        uart_process(self->uart);
        embc_dl_process(self->dl);
        if (self->process_fn) {
            self->process_fn(self->process_user_data);
        }
    }
    return 0;
}

int32_t embc_udl_start(struct embc_udl_s * self,
        struct embc_dl_api_s const * ul,
        embc_udl_process_fn process_fn,
        void * process_user_data) {
    self->process_fn = process_fn;
    self->process_user_data = process_user_data;
    if (self->thread) {
        EMBC_LOGW("embc_udl_start but already running");
        return EMBC_ERROR_BUSY;
    }
    embc_dl_register_upper_layer(self->dl, ul);

    self->mutex = CreateMutex(
            NULL,                   // default security attributes
            FALSE,                  // initially not owned
            NULL);                  // unnamed mutex
    if (!self->mutex) {
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }

    self->thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            udl_task,               // thread function name
            self,                   // argument to thread function
            0,                      // use default creation flags
            NULL);                  // returns the thread identifier
    if (!self->thread) {
        CloseHandle(self->mutex);
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!SetThreadPriority(self->thread, THREAD_PRIORITY_TIME_CRITICAL)) {
        EMBC_LOGE("Could not elevate thread priority: %d", (int) GetLastError());
    }
    return 0;
}

int32_t embc_udl_send(struct embc_udl_s * self, uint32_t metadata,
                      uint8_t const *msg, uint32_t msg_size) {
    return embc_dl_send(self->dl, metadata, msg, msg_size);
}

void embc_udl_reset(struct embc_udl_s * self) {
    embc_dl_reset(self->dl);
}

int32_t embc_udl_finalize(struct embc_udl_s * self) {
    self->quit = 1;
    WaitForSingleObject(self->thread, 1000);
    CloseHandle(self->thread);
    self->thread = NULL;
    return 0;
}

int32_t embc_udl_status_get(
        struct embc_udl_s * self,
        struct embc_dl_status_s * status) {
    return embc_dl_status_get(self->dl, status);
}

uint32_t embc_udl_send_available(struct embc_udl_s * self) {
    return uart_send_available(self->uart);
}
