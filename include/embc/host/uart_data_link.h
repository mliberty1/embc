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


/**
 * @file
 *
 * @brief UART data link.
 */

#ifndef EMBC_HOST_UART_DATA_LINK_H__
#define EMBC_HOST_UART_DATA_LINK_H__

#include <stdint.h>
#include "embc/stream/data_link.h"

#ifdef __cplusplus
extern "C" {
#endif


struct embc_udl_s;

/**
 * @brief Initialize a new UART data link interface.
 *
 * @param config The data link configuration.
 * @param uart_device The UART device name.
 * @param baudrate The baud rate.
 * @return The new instance or NULL.
 */
struct embc_udl_s * embc_udl_initialize(struct embc_dl_config_s const * config,
                                        const char * uart_device,
                                        uint32_t baudrate);

/**
 * @brief Function called on UART processing.
 *
 * @param user_data The arbitrary data.
 *
 * This function will be called from the UART data link thread
 * running at high priority.  This function must not block and
 * should return quickly.
 */
typedef void (*embc_udl_process_fn)(void * user_data);

/**
 * @brief Start the instance.
 *
 * @param self The instance.
 * @param ul The upper-layer callback functions.
 * @param process_fn The optional function to call on UART processing.
 * @param process_user_data The arbitrary data for process_fn.
 */
int32_t embc_udl_start(struct embc_udl_s * self,
                       struct embc_dl_api_s const * ul,
                       embc_udl_process_fn process_fn,
                       void * process_user_data);

/**
 * @brief Send a message.
 *
 * @param self The instance.
 * @param metadata The arbitrary 24-bit metadata associated with the message.
 * @param msgr The msg_buffer containing the message.  The driver
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 *
 * The port send_done_cbk callback will be called when the send completes.
 */
int32_t embc_udl_send(struct embc_udl_s * self, uint32_t metadata,
                     uint8_t const *msg, uint32_t msg_size);

/**
 * @brief Stop, finalize, and deallocate the data link instance.
 *
 * @param self The data link instance.
 * @return 0 or error code.
 *
 * While this method is provided for completeness, most embedded systems will
 * not use it.  Implementations without "free" may fail.
 */
int32_t embc_udl_finalize(struct embc_udl_s * self);

/**
 * @brief Get the status for the data link.
 *
 * @param self The data link instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
int32_t embc_udl_status_get(
        struct embc_udl_s * self,
        struct embc_dl_status_s * status);

/**
 * @brief Get the number of send bytes currently available.
 *
 * @param self The data link instance.
 * @return The number of free bytes available in the send buffer.
 */
uint32_t embc_udl_send_available(struct embc_udl_s * self);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_HOST_UART_DATA_LINK_H__ */
