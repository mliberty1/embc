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
 * @brief Port 0 definitions.
 */

#ifndef EMBC_STREAM_PORT0_H__
#define EMBC_STREAM_PORT0_H__

#include <stdint.h>
#include <stdbool.h>
#include "transport.h"
#include "data_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_port0 Transport Port 0
 *
 * @brief Transport port 0.
 *
 * Port 0 allocates port_data:
 *      port_data[15:8]: cmd_meta defined by each command
 *      port_data[7]: 0=request or unused, 1=response
 *      port_data[6:3]: reserved, set to 0
 *      port_data[2:0]: The embc_port0_op_e operation.
 * @{
 */

/**
 * @brief The service operations provided by port 0.
 */
enum embc_port0_op_e {
    EMBC_PORT0_OP_UNKNOWN = 0,
    EMBC_PORT0_OP_STATUS = 1,       // cmd_meta=0, rsp_payload=embc_dl_status_s
    EMBC_PORT0_OP_ECHO = 2,         // cmd_meta=any
    EMBC_PORT0_OP_TIMESYNC = 3,     // cmd_meta=0, payload=3x64-bit times: [src tx, tgt rx, tgt tx]

    /**
     * @brief Retrieve port metadata definitions.
     *
     * On request, the payload is ignored.
     * On response, the payload contains a NULL-terminated JSON formatted string.
     * The JSON response structure consists of a "type" key and a
     * user-meaningful "name" key.
     * All other keys are defined by the type.
     *
     * - oam: Operations, administration and management.  This port 0 only.
     * - pubsub: The publish-subscribe port.  Not other keys defined.
     * - text: Provides UTF-8 text communication, often for a command console,
     *   such as SCPI.  The "protocol" key describes the actual protocol.
     * - stream: An data stream with custom format.
     * - msg: Raw messages with custom payload format.
     * - sample: Binary data samples.  Each message on this
     *   port contains a 32-bit sample identifier corresponding to the first
     *   sample in the message followed by packed sample data.
     *   The additional metadata keys are:
     *   - prefix: The topic prefix for controlling this stream.
     *     If available, subtopics must include [ctrl, div, format, compression].
     *   - dir: source (device transmits data), sink (device receives data).
     *   - fs: The maximum frequency for this stream.
     *   - formats: The list of supported data formats, with the recommended
     *     default format listed first.  Formats include:
     *     - Single bit is b.
     *     - IEEE floating point is either f32 or f64.
     *     - Signed integers are iZ where Z is a multiple of 4.
     *     - Unsigned integer are uZ where Z is a multiple of 4.
     *     - Signed fixed-point integers are iMqN where M+N is a multiple of 4.
     *     - Unsigned fixed-point integers are uMqN where M+N is a multiple of 4.
     *   - compression: The list of available compression algorithms.
     *   The single bit format is packed with the bit N in byte 0, bit 0.
     *   Bit N+1 goes to byte 0, bit 1.  Bit N+8 goes to byte 1, bit 0.
     *
     *   The integer types are fully packed in little-endian format.  For types
     *   with odd nibbles. The even samples are represented "normally", and the odd
     *   samples fill their most significant nibble in the upper 4 bits of the same
     *   byte occupied by the even sample's most significant nibble.
     *
     * If the port is not defined, respond with an empty string "" consisting
     * of only the NULL terminator.
     */
    EMBC_PORT0_OP_META = 4,         // cmd_meta=port_id
    EMBC_PORT0_OP_RAW = 5,          // raw UART loopback mode request for error rate testing
};

enum embc_port0_mode_e {
    EMBC_PORT0_MODE_CLIENT, ///< Clients sync time.
    EMBC_PORT0_MODE_SERVER, ///< Servers provide reference time.
};

/// Opaque port0 instance.
struct embc_port0_s;

// Opaque transport instance, from "transport.h".
struct embc_transport_s;

/**
 * @brief Allocate and initialize the instance.
 *
 * @param mode The port0 operating mode for this instance.
 * @param transport The transport instance.
 * @param send_fn The function to call to send data, which should be
 *      embc_transport_send() except during unit testing.
 * @return 0 or error code.
 */
struct embc_port0_s * embc_port0_initialize(enum embc_port0_mode_e mode,
        struct embc_transport_s * transport,
        embc_transport_send_fn send_fn);

/**
 * @brief Finalize and deallocate the instance.
 *
 * @param self The port0 instance.
 */
void embc_port0_finalize(struct embc_port0_s * self);

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
void embc_port0_on_event_cbk(struct embc_port0_s * self, enum embc_dl_event_e event);

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
void embc_port0_on_recv_cbk(struct embc_port0_s * self,
                            uint8_t port_id,
                            enum embc_transport_seq_e seq,
                            uint16_t port_data,
                            uint8_t *msg, uint32_t msg_size);

/**
 * @brief Set the port metadata.
 *
 * @param self The port0 instance.
 * @param port_id The port_id for the metadata.
 * @param meta The JSON-formatted port metadata.  This pointer must remain
 *      valid until embc_transport_finalize().  You can remove the metadata
 *      by passing NULL.
 * @return 0 or error code.
 *
 * This function sets the port metadata.  When port 0 receives a META
 * request, then this transport layer will respond with the metadata message.
 */
int32_t embc_port0_meta_set(struct embc_port0_s * self, uint8_t port_id, const char * meta);

/**
 * @brief Get the port metadata.
 *
 * @param self The port0 instance.
 * @param port_id The port_id for the metadata.
 * @return The metadata.
 */
const char * embc_port0_meta_get(struct embc_port0_s * self, uint8_t port_id);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PORT0_H__ */
