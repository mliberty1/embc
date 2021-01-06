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


/**
 * @file
 *
 * @brief Transport layer for byte streams.
 */

#ifndef EMBC_STREAM_TRANSPORT_H__
#define EMBC_STREAM_TRANSPORT_H__

#include <stdint.h>
#include "embc/stream/data_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_transport Transport layer for byte streams
 *
 * @brief Provide port multiplexing and segmentation / reassembly.
 *
 * @{
 */


#define EMBC_TRANSPORT_PORT_MAX (0x1FU)

enum embc_transport_seq_e {
    EMBC_TRANSPORT_SEQ_MIDDLE = 0,
    EMBC_TRANSPORT_SEQ_STOP = 1,
    EMBC_TRANSPORT_SEQ_START = 2,
    EMBC_TRANSPORT_SEQ_SINGLE = 3,
};

/**
 * @brief The function called on events.
 *
 * @param user_data The arbitrary user data.
 * @param event The signaled event.
 */
typedef void (*embc_transport_event_fn)(void *user_data, enum embc_dl_event_e event);

/**
 * @brief The function called upon message receipt.
 *
 * @param user_data The arbitrary user data.
 * @param port_id The port id for this port.
 * @param seq The frame reassembly information.
 * @param port_data The arbitrary 16-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 */
typedef void (*embc_transport_recv_fn)(void *user_data,
                                       uint8_t port_id,
                                       enum embc_transport_seq_e seq,
                                       uint16_t port_data,
                                       uint8_t *msg, uint32_t msg_size);

/// The opaque transport instance.
struct embc_transport_s;

/**
 * @brief Allocate and initialize the instance.
 *
 * @param data_link The data link layer.  The only functions used are:
 *  - embc_dl_register_upper_layer
 *  - embc_dl_send
 * @return 0 or error code.
 */
struct embc_transport_s * embc_transport_initialize(struct embc_dl_s * data_link);

/**
 * @brief Finalize and deallocate the instance.
 *
 * @param self The transport instance.
 */
void embc_transport_finalize(struct embc_transport_s * self);

/**
 * @brief Register (or deregister) port callbacks.
 *
 * @param self The transport instance.
 * @param port_id The port_id to register.
 * @param event_fn The function to call on events, which may be NULL.
 * @param recv_fn The function to call on data received, which may be NULL.
 * @param user_data The arbitrary data for event_fn and recv_fn.
 * @return 0 or error code.
 */
int32_t embc_transport_port_register(struct embc_transport_s * self, uint8_t port_id,
                                     embc_transport_event_fn event_fn,
                                     embc_transport_recv_fn recv_fn,
                                     void * user_data);

/**
 * @brief Send a message.
 *
 * @param self The instance.
 * @param port_id The port id for this port.
 * @param port_data The arbitrary 16-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The msg_buffer containing the message.  The data link layer
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 *
 * The port send_done_cbk callback will be called when the send completes.
 */
int32_t embc_transport_send(struct embc_transport_s * self,
                            uint8_t port_id,
                            uint16_t port_data,
                            uint8_t const *msg, uint32_t msg_size);


/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_TRANSPORT_H__ */
