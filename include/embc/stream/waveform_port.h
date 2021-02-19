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
 * @brief Waveform port.
 */

#ifndef EMBC_STREAM_WAVEFORM_PORT_H__
#define EMBC_STREAM_WAVEFORM_PORT_H__

#include <stdint.h>
#include <stdbool.h>
#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_waveform_port Waveform Port
 *
 * @brief Waveform port.
 *
 * @{
 */

/*
 * FEATURES to implement
 *
 * - Send & receive waveform data for float32
 * - Send & receive waveform data for signed & unsigned integers of various widths
 * - Send & receive timing data: sample_id & UTC pairs
 * - Compression
 *
 * port_data[15:12] != 0xf: data packet
 *     port[3:0] sample width in nibbles - 1
 *     port[5:4] data format: bool, unsigned int, signed int, float
 *     port[11:6] fixed point location, for int types
 *     port[15:12] compression mode
 *     Payload starts with 32-bit sample_id.
 * port_data[15:12] == 0xf: all other packets
 *     port_data[11:8]: packet type
 */


/// Opaque wavform port instance.
struct embc_pwave_s;


/**
 * @brief Allocate and initialize the instance.
 *
 * @return The new instance or NULL on error.
 */
struct embc_pwave_s * embc_pwave_initialize();

/**
 * @brief Finalize and deallocate the instance.
 *
 * @param self The port0 instance.
 */
void embc_pwave_finalize(struct embc_pwave_s * self);

/**
 * @brief The function to call when the transport layer receives an event.
 *
 * @param self The instance.
 * @param event The event.
 *
 * This function can be safely cast to embc_transport_event_fn and provided
 * to embc_transport_port_register().
 *
 */
void embc_pwave_on_event_cbk(struct embc_pwave_s * self, enum embc_dl_event_e event);

/**
 * @brief The function to call when the transport layer receives a message.
 *
 * @param self The instance.
 * @param metadata The arbitrary 24-bit metadata associated with the message.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 *
 * This function can be safely cast to embc_dl_recv_fn and provided
 * to embc_transport_port_register().
 */
void embc_pwave_on_recv_cbk(struct embc_pwave_s * self,
                            uint8_t port_id,
                            enum embc_transport_seq_e seq,
                            uint16_t port_data,
                            uint8_t *msg, uint32_t msg_size);


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_WAVEFORM_PORT_H__ */
