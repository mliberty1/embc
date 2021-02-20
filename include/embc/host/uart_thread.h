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


/**
 * @file
 *
 * @brief Threaded UART abstraction.
 */

#ifndef EMBC_HOST_UART_THREAD_H__
#define EMBC_HOST_UART_THREAD_H__

#include "embc/host/uart.h"
#include "embc/stream/data_link.h"
#include "embc/os/mutex.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_uart_thread Threaded UART.
 *
 * @brief Threaded UART.
 *
 * This UART thread implementation is compatible with the stream data_link
 * lower level API.
 *
 * @{
 */


struct embc_uartt_s;


/**
 * @brief Function called on UART processing.
 *
 * @param user_data The arbitrary data.
 *
 * This function will be called from the UART data link thread
 * running at high priority.  This function must not block and
 * should return quickly.
 */
typedef void (*embc_uartt_process_fn)(void * user_data);


struct embc_uartt_s * embc_uartt_initialize(const char *device_path, struct uart_config_s const * config);

void embc_uartt_finalize(struct embc_uartt_s *self);

int32_t embc_uartt_start(struct embc_uartt_s * self,
                         embc_uartt_process_fn process_fn, void * process_user_data);

int32_t embc_uartt_stop(struct embc_uartt_s * self);

/**
 * @brief Populate the EVM API.
 *
 * @param self The event manager instance.
 * @param api[out] The API instance to populate with the default functions.
 * @return 0 or error code.
 *
 * Use embc_time_rel for api->timestamp by default.
 */
int32_t embc_uartt_evm_api(struct embc_uartt_s * self, struct embc_evm_api_s * api);

/**
 * @brief Send data out the UART.
 *
 * @param self The UART thread instance.
 * @param buffer The buffer containing the data to send. The caller retains
 *      ownership, and the buffer is only valid for the duration of the call.
 * @param buffer_size The size of buffer in total_bytes.
 *
 * Compatible with embc_dl_ll_send_fn.
 */
void embc_uartt_send(struct embc_uartt_s * self, uint8_t const * buffer, uint32_t buffer_size);

/**
 * @brief The number of bytes currently available to send().
 *
 * @param self The UART thread instance.
 * @return The non-blocking free space available to send().
 *
 * Compatible with embc_dl_ll_send_available_fn.
 */
uint32_t embc_uartt_send_available(struct embc_uartt_s * self);

/**
 * @brief Get the mutex for accessing this thread.
 *
 * @param self The UART thread instance.
 * @param mutex[out] The mutex.
 */
void embc_uartt_mutex(struct embc_uartt_s * self, embc_os_mutex_t * mutex);


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_HOST_UART_THREAD_H__ */
