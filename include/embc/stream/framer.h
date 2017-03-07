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
 * support for up to 256 byte payloads.  Larger messages may be segmented
 * over multiple frames and then reassembled.
 *
 * The features of this framer include:
 *
 * - Robust framing using SOF byte, length, frame header CRC, framer CRC
 *   and EOF byte.
 * - Support for per-frame acknowledgement and retransmission.
 * - Support for multiple outstanding frames for maximum throughput.
 * - Support for higher-level message segmentation/reassembly.
 * - Support for multiple payload types using the port field.
 *
 * The frame format is:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF</td></tr>
 *  <tr>
 *      <td colspan="4">reserved</td>
 *      <td colspan="4">id</td>
 *  </tr>
 *  <tr><td colspan="8">port</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">header_crc</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">EOF/td></tr>
 * </table>
 *
 * - "SOF" is the start of frame byte.  Although SOF is not unique and also not
 *   escaped, the SOF drastically reduces the framing search space.  The
 *   SOF may also be used for autobaud detection.
 * - "reserved" are reserved bits that must be set to zero.  If the reserved
 *   bits are not zero, then the framer will emit a framing error.
 * - "id" contains an identifier that is temporally unique for all DATA
 *   frames (port != 0) currently in flight.  DATA frames may be individually
 *   CONFIRMed (port = 0) using this id.
 * - "port" contains an application-specific payload format identifier.
 *   This field is used to multiplex multiple message types onto a single
 *   byte stream, similar to a TCP port.  Port 0 is reserved for
 *   CONFIRM (acknowledgements).  CONFIRM frames always contain a single
 *   status byte (0=success) as the payload.  Many stacks also reserve Port 1
 *   for link management.
 * - "length" is the payload length (not full frame length) in bytes.  A
 *   value of 0 is 256, and zero length messages are not allowed.  Since the
 *   frame overhead is 8 bytes, the actual frame length ranges from 9 to
 *   264 bytes.
 * - "header_crc" contains the CRC computed over the SOF and two header bytes
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
 * CRC is the CRC-8 computed over the SOF byte and the next three bytes.
 * If the computed header CRC matches the fifth byte, then the header is
 * valid, and all header fields are presumed valid.  The CRC-16-CCITT
 * is computed over the entire frame from SOF through payload.  If the
 * frame_crc bytes match the computed CRC, then the entire frame is valid.
 *
 * If the 8-bit header CRC check fails, then the receiver should ignore the
 * sync byte and search for the next start of frame.  The receiver may
 * optionally send a CONFIRMed with EMBC_ERROR_SYNCHRONIZATION with "id" 0.
 *
 * If the 16-bit frame CRC check fails, then the receiver should send a
 * CONFIRM frame with status EMBC_ERROR_MESSAGE_INTEGRITY that also contains
 * the id.  If the frame is received successfully, then the receiver should
 * provide the frame to the next higher level.  Most implementations should
 * send a CONFIRM (port = 0) frame with the same id.
 *
 * This framer contains no built-in mechanism for backpressure.  The framer
 * is intended to be used with a higher level protocol that supports a
 * maximum number of outstanding frames along with acknowledgements to
 * implement appropriate backpressure and avoid data overflow conditions.
 * The framer implementation will drop data on data overflow conditions.
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

/**
 * @brief The opaque framer instance.
 */
struct embc_framer_s;

/**
 * @brief Function called after a frame is received.
 *
 * @param user_data The user data.
 * @param id The frame identifier.
 * @param port The port (payload type).
 * @param buffer The buffer containing the received data.
 * @param length The length buffer in bytes.
 */
typedef void (*embc_framer_rx_fn)(
        void *user_data,
        uint8_t id, uint8_t port,
        uint8_t const * buffer, uint32_t length);

/**
 * @brief Function called after a receive frame error is detected.
 *
 * @param user_data The user data.
 * @param id The frame identifier (if applicable).
 * @param status The error code.
 */
typedef void (*embc_framer_error_fn)(
        void *user_data, uint8_t id, uint8_t status);

/**
 * @brief Function called to transmit bytes out the byte stream.
 *
 * @param user_data The user data.
 * @param buffer The buffer containing the transmit data.
 * @param length The length of buffer in bytes.
 */
typedef void (*embc_framer_tx_fn)(
        void * user_data,
        uint8_t const *buffer, uint16_t length);

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
 * @param tx_fn The function called to send bytes.
 * @param tx_user_data The optional user data for tx_fn.
 * @return 0 or error code.
 *
 * The self instance, tx_fn and tx_user_data must remain valid until
 * embc_framer_finalize().  Use embc_framer_event_callbacks() to set the event
 * callbacks.
 */
EMBC_API void embc_framer_initialize(
        struct embc_framer_s * self,
        embc_framer_tx_fn tx_fn, void * tx_user_data);

/**
 * @brief Set the receive callback.
 *
 * @param self The instance.
 * @param fn The function called on receive frames.
 * @param user_data The data provided to rx_fn.
 *
 * The fn and user_data must remain valid until either
 * embc_framer_event_callbacks() is called again or embc_framer_finalize().
 */
EMBC_API void embc_framer_rx_callback(
        struct embc_framer_s * self,
        embc_framer_rx_fn fn, void * user_data);

/**
 * @brief Set the framer error callback.
 *
 * @param self The instance.
 * @param fn The function called on receive frames.
 * @param user_data The data provided to error_fn.
 *
 * The fn and user_data must remain valid until either
 * embc_framer_event_callbacks() is called again or embc_framer_finalize().
 */
EMBC_API void embc_framer_error_callback(
        struct embc_framer_s * self,
        embc_framer_error_fn fn, void * user_data);

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
 * @param buffer The buffer containing the transmit payload.
 * @param length The length of buffer in bytes.
 */
EMBC_API void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t id, uint8_t port,
        uint8_t const * buffer, uint32_t length);

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_STREAM_FRAMER_H_ */
