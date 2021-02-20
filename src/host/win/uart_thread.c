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

#include "embc/host/uart_thread.h"
#include "embc/host/uart.h"
#include "embc/event_manager.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/platform.h"
#include "embc/os/mutex.h"
#include "embc/time.h"
#include <stdio.h>
#include <windows.h>


struct embc_uartt_s {
    embc_uartt_process_fn process_fn;
    void * process_user_data;
    struct embc_evm_s * evm;
    embc_os_mutex_t mutex;
    HANDLE thread;
    volatile int quit;
    struct uart_s * uart;
};

struct embc_uartt_s * embc_uartt_initialize(const char *device_path, struct uart_config_s const * config) {
    char dev_str[1024];
    struct embc_uartt_s * self = embc_alloc_clr(sizeof(struct embc_uartt_s));
    if (!self) {
        return NULL;
    }

    EMBC_LOGI("embc_uartt_initialize(%s, %d)", device_path, (int) config->baudrate);
    self->mutex = embc_os_mutex_alloc();

    self->evm = embc_evm_allocate();
    embc_evm_register_mutex(self->evm, self->mutex);
    struct embc_evm_api_s evm_api;
    embc_evm_api_config(self->evm, &evm_api);

    self->uart = uart_alloc();
    if (!self->uart) {
        embc_uartt_finalize(self);
        return NULL;
    }

    snprintf(dev_str, sizeof(dev_str), "\\\\.\\%s", device_path);
    int32_t rv = uart_open(self->uart, dev_str, config);
    if (rv) {
        embc_uartt_finalize(self);
        return NULL;
    }

    return self;
}

void embc_uartt_finalize(struct embc_uartt_s *self) {
    if (self) {
        embc_uartt_stop(self);
        if (self->uart) {
            uart_close(self->uart);
            embc_free(self->uart);
            self->uart = NULL;
        }
        if (self->evm) {
            embc_evm_free(self->evm);
            self->evm = NULL;
        }
        if (self->mutex) {
            embc_os_mutex_free(self->mutex);
            self->mutex = NULL;
        }
        embc_free(self);
    }
}

static DWORD WINAPI task(LPVOID lpParam) {
    uint32_t handle_count;
    HANDLE handles[16];

    struct embc_uartt_s * self = (struct embc_uartt_s *) lpParam;
    while (!self->quit) {
        handle_count = 0;
        uart_handles(self->uart, &handle_count, &handles[handle_count]);
        if (handle_count) {
            // note: could use embc_evm_interval_next to be less aggressive here if needed.
            WaitForMultipleObjects(handle_count, handles, FALSE, 1);
        } else {
            Sleep(1);
        }

        embc_os_mutex_lock(self->mutex);
        uart_process(self->uart);
        embc_evm_process(self->evm, embc_time_rel());
        if (self->process_fn) {
            self->process_fn(self->process_user_data);
        }
        embc_os_mutex_unlock(self->mutex);
    }
    return 0;
}

int32_t embc_uartt_start(struct embc_uartt_s * self,
                         embc_uartt_process_fn process_fn, void * process_user_data) {
    self->process_fn = process_fn;
    self->process_user_data = process_user_data;
    if (self->thread) {
        EMBC_LOGW("embc_udl_start but already running");
        return EMBC_ERROR_BUSY;
    }

    self->thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            task,                   // thread function name
            self,                   // argument to thread function
            0,                      // use default creation flags
            NULL);                  // returns the thread identifier
    if (!self->thread) {
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!SetThreadPriority(self->thread, THREAD_PRIORITY_TIME_CRITICAL)) {
        EMBC_LOGE("Could not elevate thread priority: %d", (int) GetLastError());
    }
    return 0;
}

int32_t embc_uartt_stop(struct embc_uartt_s * self) {
    int rc = 0;
    if (self) {
        self->process_fn = NULL;
        self->quit = 1;
        if (self->thread) {
            if (WAIT_OBJECT_0 != WaitForSingleObject(self->thread, 1000)) {
                EMBC_LOGW("UART thread failed to shut down gracefully");
                rc = EMBC_ERROR_TIMED_OUT;
            }
            CloseHandle(self->thread);
            self->thread = NULL;
        }
    }
    return rc;
}

int32_t embc_uartt_evm_api(struct embc_uartt_s * self, struct embc_evm_api_s * api) {
    return embc_evm_api_config(self->evm, api);
}

void embc_uartt_send(struct embc_uartt_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    if (uart_send_available(self->uart) >= buffer_size) {
        uart_write(self->uart, buffer, buffer_size);
    }
}

uint32_t embc_uartt_send_available(struct embc_uartt_s * self) {
    return uart_send_available(self->uart);
}

void embc_uartt_mutex(struct embc_uartt_s * self, embc_os_mutex_t * mutex) {
    *mutex = self->mutex;
}
