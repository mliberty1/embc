/*
 * Copyright 2014-2017 Jetperch LLC
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
 * @brief Message framer for byte streams.
 */

#ifndef EMBC_STREAM_FRAMER_H_
#define EMBC_STREAM_FRAMER_H_

#include "embc/cmacro_inc.h"
#include <stdint.h>

/**
 * @ingroup embc
 * @defgroup embc_framer Message framing
 *
 * @brief Provide message framing and deframing over byte streams.
 *
 * This module provides reliable message transmission over byte streams.
 * Framing multi-byte messages over a byte-oriented interface is a common
 * problem for UART and TCP communications.  In addition to framing and
 * deframing messages, this module also provides confirmed delivery with
 * support for up to 256 byte payloads.  All implementations must support
 * the full 256 payload as the maximum transmission unit (MTU).
 * Larger messages may be segmented over multiple frames and then reassembled.
 *
 * The features of this framer include:
 *
 * - Robust framing using SOF byte, length, frame header CRC, framer CRC
 *   and EOF byte.
 * - Guaranteed in-order delivery.
 * - Reliable data delivery with per-frame acknowledgement and
 *   automatic retransmission.
 * - Multiple outstanding frames for maximum throughput.
 * - Support for higher-level message segmentation/reassembly.
 * - Support for multiple payload types using the port field.
 *
 * The frame format is:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF</td></tr>
 *  <tr><td colspan="8">frame_id</td></tr>
 *  <tr><td colspan="8">port</td></tr>
 *  <tr><td colspan="8">message_id</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">port_def[7:0]</td></tr>
 *  <tr><td colspan="8">port_def[15:8]</td></tr>
 *  <tr><td colspan="8">header_crc</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF/td></tr>
 * </table>
 *
 * - "SOF" is the start of frame byte.  Although SOF is not unique and also not
 *   escaped, the SOF drastically reduces the framing search space.  The
 *   SOF may also be used for autobaud detection.
 * - "frame_id" contains an identifier that is temporally unique for all
 *   frames across all ports, except for port 0.  Frames are
 *   individually CONFIRMed on port 0 using the frame_id.  The
 *   frame_id increments sequentially with each new frame and is assigned
 *   by the framer implementation.
 * - "port" contains an application-specific payload format identifier.
 *   This field is used to multiplex multiple message types onto a single
 *   byte stream, similar to a TCP port.  Port 0 is reserved for
 *   CONFIRM (acknowledgements).  Port 1 is reserved for link management.
 * - "message_id" contains an identifier that is assigned by the application.
 *   Normally, message_id values are unique within each port.
 * - "length" is the payload length (not full frame length) in bytes.  A
 *   value of 0 is 256, and zero length messages are not allowed.  Since the
 *   frame overhead is 13 bytes, the actual frame length ranges from 14 to
 *   269 bytes.
 * - "port_def" may contain arbitrary data that is defined by the specific
 *   port and application.
 * - "header_crc" contains the CRC computed over the SOF and six header bytes
 *   The header crc allows the protocol to more reliably indicate payload
 *   errors for faster retransmission.
 * - "payload" contains the arbitrary payload of "length" bytes.
 * - "frame_crc" contains the CRC computed over the SOF through the payload.
 * - "EOF" contains a end of frame byte which allows for reliable receiver
 *   timeouts and receiver framer reset.  Typically the value for EOF and
 *   SOF are the same.  Repeated SOF/EOF bytes between frames are ignored
 *   by the framer and can be used for autobaud detection.
 *
 * Framing is performed by first searching for the sync byte.  The header
 * CRC is the CRC-8 computed over the SOF byte and the next six bytes.
 * If the computed header CRC matches the eight byte, then the header is
 * valid, and all header fields are presumed valid.  The CRC-32-CCITT
 * is computed over the entire frame from SOF through payload.  If the
 * frame_crc bytes match the computed CRC, then the entire frame is valid.
 *
 * If the 8-bit header CRC check fails, then the receiver should ignore the
 * sync byte and search for the next start of frame.  The receiver may
 * optionally send a CONFIRMed with EMBC_ERROR_SYNCHRONIZATION with "id" 0.
 *
 * If the 32-bit frame CRC check fails, then the receiver should send a
 * CONFIRM frame with status EMBC_ERROR_MESSAGE_INTEGRITY that also contains
 * the id.  If the frame is received successfully, then the receiver should
 * provide the frame to the next higher level.  Most implementations should
 * send a CONFIRM (port = 0) frame with the same id.
 *
 * This framer contains no built-in mechanism for backpressure.  The framer
 * is intended to be used with a higher level protocol that supports a
 * maximum number of outstanding frames along with acknowledgements to
 * implement appropriate backpressure and avoid data overflow conditions.
 * The framer implementation is free to drop data on overflow.
 *
 * ## References
 *
 *    - Overview:
 *      [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications),
 *      [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *    - PPP:
 *      [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      [RFC](https://tools.ietf.org/html/rfc1661)
 *    - Constant Overhead Byte Stuffing (COBS):
 *      [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *    - [Telemetry](https://github.com/Overdrivr/Telemetry)
 */

EMBC_CPP_GUARD_START

/// The value for the start of frame byte.
#define EMBC_FRAMER_SOF ((uint8_t) 0xAA)
/// The framer header size in bytes.
#define EMBC_FRAMER_HEADER_SIZE (8)
/// The maximum payload size in bytes.
#define EMBC_FRAMER_PAYLOAD_MAX_SIZE (256)
/// The framer footer size in bytes.
#define EMBC_FRAMER_FOOTER_SIZE (5)
/// The framer total maximum size in bytes
#define EMBC_FRAMER_FRAME_MAX_SIZE (\
    EMBC_FRAMER_HEADER_SIZE + \
    EMBC_FRAMER_PAYLOAD_MAX_SIZE + \
    EMBC_FRAMER_FOOTER_SIZE)
/// The maximum number of outstanding frames (not yet acknowledged).
#define EMBC_FRAMER_OUTSTANDING_FRAMES_MAX (2)
/// The acknowledgement payload size.
#define EMBC_FRAMER_ACK_PAYLOAD_SIZE (1)
/// The acknowledgement frame size.
#define EMBC_FRAMER_ACK_FRAME_SIZE (\
    EMBC_FRAMER_HEADER_SIZE + \
    EMBC_FRAMER_ACK_PAYLOAD_SIZE + \
    EMBC_FRAMER_FOOTER_SIZE)

/**
 * @brief The frame header.
 */
struct embc_framer_header_s {
    uint8_t sof;
    uint8_t frame_id;
    uint8_t port;
    uint8_t message_id;
    uint8_t length;
    uint8_t port_def0;
    uint8_t port_def1;
    uint8_t crc8;
};

/**
 * @brief The opaque framer instance.
 */
struct embc_framer_s;


struct embc_framer_event_callbacks_s {
    /**
     * @brief Function called after a frame is received.
     *
     * @param user_data The user data.
     * @param port The port (payload type).
     * @param message_id The application-defined message identifier.
     * @param port_def The application-defined frame-associated data.
     * @param buffer The buffer containing the received data.  The buffer
     *      only remains valid for the duration of the callback.
     * @param length The length buffer in bytes.
     */
    void (*rx_fn)(
            void *user_data,
            uint8_t port, uint8_t message_id, uint16_t port_def,
            uint8_t const * buffer, uint16_t length);

    void * rx_user_data;

    /**
     * @brief Function called on successful transmissions.
     *
     * @param user_data The user data.
     * @param port The port (payload type).
     * @param message_id The application-defined message identifier.
     * @param status 0 or error code.
     */
    void (*tx_done_fn)(
            void * user_data,
            uint8_t port,
            uint8_t message_id,
            int32_t status);

    void * tx_done_user_data;
};

struct embc_framer_hal_callbacks_s {
    /**
     * @brief Function called to transmit bytes out the byte stream.
     *
     * @param user_data The user data.
     * @param buffer The buffer containing the transmit data.  The
     *      buffer will remain valid until an acknowledgement is received
     *      or a timeout occurs.  In either case, the HAL may safely use
     *      this buffer directly during data transmission without needing
     *      to copy.
     * @param length The length of buffer in bytes.
     */
    void (*tx_fn)(
            void * user_data,
            uint8_t const *buffer, uint16_t length);

    void * tx_user_data;

    /**
     * @brief Set a timer.
     *
     * @param user_data The user data provided to fpe_initialize().
     * @param duration The timer duration as 34Q30 seconds relative to the
     *      current time.
     * @param cbk_fn The function to call if the timer expires which has
     *      arguments of (user_data, timer_id).
     * @param cbk_user_data The additional data to provide to cbk_fn.
     * @param[out] timer_id The assigned event id.
     * @return 0 or error code.
     */
    int32_t (*timer_set_fn)(void * user_data, uint64_t duration,
                         void (*cbk_fn)(void *, uint32_t), void * cbk_user_data,
                         uint32_t * timer_id);

    void * timer_set_user_data;

    /**
     * @brief Cancel a timer.
     *
     * @param user_data The user data provided to fpe_initialize().
     * @param timer_id The timer id assigned by timer_set().
     */
    int32_t (*timer_cancel_fn)(void * user_data, uint32_t timer_id);

    void * timer_cancel_user_data;
};


/**
 * @brief Get the sizeof(embc_framer_s) for external allocation.
 *
 * @return The size of embc_framer_s in bytes.
 *
 * This function is used to dynamically allocate embc_framer_s instances
 * without knowing about the embc_framer_s details.
 */
EMBC_API uint32_t embc_framer_instance_size(void);

/**
 * @brief Initialize or reinitialize an instance.
 *
 * @param self The instance which must be at least embc_framer_instance_size()
 *      bytes in size.
 * @param callbacks The hardware abstraction layer callbacks.
 * @return 0 or error code.
 *
 * The self instance and the hal callbacks must remain valid until
 * embc_framer_finalize().  Use embc_framer_event_callbacks() to set the event
 * callbacks.
 */
EMBC_API void embc_framer_initialize(
        struct embc_framer_s * self,
        struct embc_framer_hal_callbacks_s * callbacks);

/**
 * @brief Set the event callbacks.
 *
 * @param self The framer instance.
 * @param callbacks The event callbacks.
 * @return 0 or error code.
 *
 * The event callbacks must remain valid until another call to
 * embc_framer_event_callbacks() or embc_framer_finalize().
 */
EMBC_API void embc_framer_event_callbacks(
        struct embc_framer_s * self,
        struct embc_framer_event_callbacks_s * callbacks);

/**
 * @brief Finalize a framer instance.
 *
 * @param self The instance.
 */
EMBC_API void embc_framer_finalize(struct embc_framer_s * self);

/**
 * @brief Handle the next byte in the incoming receive byte stream.
 *
 * @param self The instance.
 * @param byte The next received byte.
 */
EMBC_API void embc_framer_rx_byte(struct embc_framer_s * self, uint8_t byte);

/**
 * @brief Handle the next bytes in the incoming receive byte stream.
 *
 * @param self The instance.
 * @param buffer The buffer containing the receive payload.
 * @param length The length of buffer in bytes.
 *
 * This function is functionally equivalent to calling embc_framer_rx_byte()
 * on each byte in buffer.
 */
EMBC_API void embc_framer_rx_buffer(
        struct embc_framer_s * self,
        uint8_t const * buffer, uint32_t length);

/**
 * @brief Send a frame.
 *
 * @param self The instance.
 * @param id The frame identifier.
 * @param port The port (payload type).
 * @param message_id The application-defined message identifier.
 * @param port_def The application-defined frame-associated data.
 * @param buffer The buffer containing the transmit payload.  This data only
 *      needs to remain valid for the duration of the function call.  The
 *      framer will copy this data to support retransmission as needed.
 * @param length The length of buffer in bytes.
 */
EMBC_API void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * buffer, uint32_t length);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_STREAM_FRAMER_H_ */
