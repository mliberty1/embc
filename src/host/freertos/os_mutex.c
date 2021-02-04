/*
 * Copyright 2014-2021 Jetperch LLC
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


#include "embc/os/mutex.h"
#include "embc/assert.h"
#include "FreeRTOS.h"
#include "semphr.h"


#define MUTEX_LOCK_TIMEOUT_TICKS  ((TickType_t) 500)

embc_os_mutex_t embc_os_mutex_alloc() {
    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    EMBC_ASSERT_ALLOC(mutex);
    return mutex;
}

void embc_os_mutex_free(embc_os_mutex_t mutex) {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

void embc_os_mutex_lock(embc_os_mutex_t mutex) {
    if (pdFALSE == xSemaphoreTake(mutex, MUTEX_LOCK_TIMEOUT_TICKS)) {
        EMBC_FATAL("mutex lock failed");
    }
}

void embc_os_mutex_unlock(embc_os_mutex_t mutex) {
    if (pdFALSE == xSemaphoreGive(mutex)) {
        EMBC_FATAL("mutex unlock failed");
    }
}
