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
#include "embc/platform.h"

/**
 * @ingroup embc
 * @defgroup embc_framer Message framing
 *
 * @brief Provide message framing and deframing over byte streams.
 *
 * This module provides reliable message transmission over byte streams.
 * Framing multi-byte messages over a byte-oriented interface is a common
 * problem for UART and network communications.  In addition to framing and
 * deframing messages, this module also provides confirmed, in-order delivery
 * with support for up to 240 byte payloads, which allows the entire frame to
 * fit within 256 bytes.  Larger messages may be segmented over multiple
 * frames and then reassembled.
 *
 * The features of this framer include:
 *
 * - Robust framing using SOF byte, length, frame header CRC, framer CRC
 *   and EOF byte.
 * - Guaranteed in-order delivery.
 * - Reliable data delivery with per-frame acknowledgements, timeouts and
 *   automatic retransmission.
 * - Multiple outstanding frames for maximum throughput.
 * - Support for larger message using segmentation/reassembly.
 * - Support for multiple payload types using the port field.
 *
 * For extremely fast transmitters (UART CDC over USB), the maximum number of
 * outstanding frames limits the total rate.  A slower receiver can hold up
 * the transmitter by delaying ACKs.  However, the current implementation does
 * not allow higher layers to directly delay ACKs.  The implementation
 * presumes that this framer is running in the same thread as the receive
 * message processing.  The framer calls the port's rx_fn before issuing the
 * ACK.  Any processing time stalls the ACK to throttle back the transmitters
 * data rate.  Delaying incoming byte processing or dropping receive bytes
 * effectively delays the ACKs, too.
 *
 * The frame format is:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF</td></tr>
 *  <tr>
 *      <td colspan="1">frame_type</td>
 *      <td colspan="1">0</td>
 *      <td colspan="1">0</td>
 *      <td colspan="1">0</td>
 *      <td colspan="4">frame_id</td>
 *  </tr>
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
 * - "frame_type" contains the frame type.  0=DATA, 1=ACK.
 * - "frame_id" contains an identifier that is temporally unique for all
 *   DATA frames across all ports.  Frames are individually ACKed using the
 *   frame_id.  The frame_id increments sequentially with each new frame and
 *   is assigned by the framer implementation.
 * - "port" contains an application-specific payload format identifier.
 *   This field is used to multiplex multiple message types onto a single
 *   byte stream, similar to a TCP port.  Port 0 is reserved for
 *   link management.
 * - "message_id" contains an identifier that is assigned by the application.
 *   Normally, message_id values are unique within each port.
 * - "length" is the payload length (not full frame length) in bytes.  The
 *   maximum payload length is 240 bytes.  Since the frame overhead is 13
 *   bytes, the actual frame length ranges from 13 to 253 bytes.
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
 * optionally send a ACK with status EMBC_ERROR_SYNCHRONIZATION.
 *
 * If the 32-bit frame CRC check fails, then the receiver should send a
 * ACK frame with status EMBC_ERROR_MESSAGE_INTEGRITY that also contains
 * the id.  If the frame is received successfully, then the receiver should
 * provide the frame to the next higher level.
 *
 * Port 0 is is reserved for acknowledgements and link management.
 * Acknowledgements which allow for confirmed, in-order delivery.
 * For every frame received successfully or with a detectable error, the
 * receiver sends a ACK frame back to the transmitter that acknowledges
 * receipt of a DATA frame.
 *
 * The ACK frame fields are:
 * - frame_type = 1 (ACK)
 * - frame_id = the originating frame_id.
 * - port = the originating port.
 * - message_id = the originating message_id
 * - port_def = The bitmask of frames that have been successfully received.
 *   Bit 8 corresponds to this frame_id.  Bit 9 is the next frame it while
 *   bit 7 is the previous frame id.
 * - port_def[15:8] = The bitmask of prior frame_ids that were successfully
 *   received.  Bit 8 corresponds to (frame_id - 1) and bit 15 corresponds
 *   to (frame_id - 9).  This field simplifies transmitter retry and reduces
 *   the impact of lost/corrupted ACK frames.
 * - payload: 1 byte status code which is 0 on success or an error code on
 *   failure.
 *
 * This framer contains support for backpressure by providing notifications
 * when a frame transmission completes, either successfully or with error.
 * The higher level should only allow a limited number of transmissions to
 * be pending.  The application can determine the desired number of pending
 * transmissions based upon memory availability and application complexity.
 * The framer uses embc_buffer_s to allocate memory.  Any out of memory
 * condition will result in an assert.
 *
 * ## References
 *
 *    - Overview:
 *      [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications),
 *      [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *    - PPP:
 *      [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      [RFC](https://tools.ietf.org/html/rfc1661),
 *      [Segger embOS/IP](https://www.segger.com/products/connectivity/embosip/add-ons/ppppppoe/)
 *    - Constant Overhead Byte Stuffing (COBS):
 *      [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *    - [Telemetry](https://github.com/Overdrivr/Telemetry)
 *    - [Microcontroller Interconnect Network](https://github.com/min-protocol/min)
 */

EMBC_CPP_GUARD_START

/// The value for the start of frame byte.
#define EMBC_FRAMER_SOF ((uint8_t) 0xAA)
/// The framer header size in bytes.
#define EMBC_FRAMER_HEADER_SIZE (8)
/// The maximum payload size in bytes.
#define EMBC_FRAMER_PAYLOAD_MAX_SIZE (240)
/// The framer footer size in bytes.
#define EMBC_FRAMER_FOOTER_SIZE (5)
/// The framer total maximum size in bytes
#define EMBC_FRAMER_FRAME_MAX_SIZE (\
    EMBC_FRAMER_HEADER_SIZE + \
    EMBC_FRAMER_PAYLOAD_MAX_SIZE + \
    EMBC_FRAMER_FOOTER_SIZE)
/**
 * @brief The maximum number of outstanding frames (not yet acknowledged).
 *
 * The protocol is designed to support up to 7 maximum outstanding DATA frames,
 * but most designs should only need 2 or 3 to achieve maximum throughput
 * for full-sized frames.
 */
#define EMBC_FRAMER_OUTSTANDING_FRAMES_MAX (3)
/// The acknowledgement payload size.
#define EMBC_FRAMER_ACK_PAYLOAD_SIZE (1)
/// The acknowledgement frame size.
#define EMBC_FRAMER_ACK_FRAME_SIZE (\
    EMBC_FRAMER_HEADER_SIZE + \
    EMBC_FRAMER_ACK_PAYLOAD_SIZE + \
    EMBC_FRAMER_FOOTER_SIZE)
/// The maximum number of ports supported by implementations
#define EMBC_FRAMER_PORTS (16)
/// The ack mask (port_def) for the current frame.
#define EMBC_FRAMER_ACK_MASK_CURRENT ((uint16_t) 0x0100)
/// The framer_id field mask for the frame id.
#define EMBC_FRAMER_ID_MASK ((uint8_t) 0x0f)

/// The framer type mask for the framer_id field
#define EMBC_FRAMER_TYPE_MASK ((uint8_t) 0x80)
/// The framer type DATA for framer_id field
#define EMBC_FRAMER_TYPE_DATA ((uint8_t) 0x00)
/// The framer type ACK for framer_id field
#define EMBC_FRAMER_TYPE_ACK ((uint8_t) 0x80)

/// The maximum number of retries per frame before giving up
#define EMBC_FRAMER_MAX_RETRIES ((uint8_t) 16)

/**
 * @brief The frame header.
 */
struct embc_framer_header_s {
    /// Start of frame = EMBC_FRAMER_SOF.
    uint8_t sof;
    /// The frame_type in bit 7 (0=Data, 1=ACK) and lower nibble is frame_id.
    uint8_t frame_id;  // upper nibble reserved
    /// The port number.  0 is reserved for the framer link management.
    uint8_t port;
    /// The message identifier assigned by the application.
    uint8_t message_id;
    /// The payload length in bytes.
    uint8_t length;
    /// The port-specific data (lower byte).
    uint8_t port_def0;
    /// The port specific data (upper byte)
    uint8_t port_def1;
    /// The CRC over the first 7 header bytes.
    uint8_t crc8;
};

/**
 * @brief The framer status.
 */
struct embc_framer_status_s {
    uint32_t version;
    uint32_t rx_count;
    uint32_t rx_data_count;
    uint32_t rx_ack_count;
    uint32_t rx_deduplicate_count;
    uint32_t rx_synchronization_error;
    uint32_t rx_mic_error;
    uint32_t rx_frame_id_error;
    uint32_t tx_count;
    uint32_t tx_retransmit_count;
};

/**
 * @brief The framer Port 0 commands
 */
enum embc_framer_port0_cmd_e {
    EMBC_FRAMER_PORT0_RSV1 = 0,
    EMBC_FRAMER_PORT0_RSV2 = 1,
    EMBC_FRAMER_PORT0_PING_REQ = 2,
    EMBC_FRAMER_PORT0_PING_RSP = 3,
    EMBC_FRAMER_PORT0_STATUS_REQ = 4,
    EMBC_FRAMER_PORT0_STATUS_RSP = 5,
};

/**
 * @brief The opaque framer instance.
 */
struct embc_framer_s;

// forward declaration from embc/memory/buffer.h
struct embc_buffer_s;
struct embc_buffer_allocator_s;

/**
 * @brief The framer port callbacks, registered for each port.
 */
struct embc_framer_port_callbacks_s {
    /**
     * @brief Function called after a frame is received.
     *
     * @param user_data The user data.
     * @param port The port (payload type).
     * @param message_id The application-defined message identifier.
     * @param port_def The application-defined frame-associated data.
     * @param buffer The buffer containing the frame.  The cursor is
     *      set to the start of the payload which is at
     *      buffer->data + buffer->cursor.  The actual payload length
     *      is embc_buffer_read_remaining(buffer), not length!
     *      The function takes ownership of buffer and must call
     *      embc_buffer_free() when done.
     *      The buffer.buffer_id contains the message_id and
     *      buffer.flags contains the port_def data.
     */
    void (*rx_fn)(void *user_data,
                  uint8_t port, uint8_t message_id, uint16_t port_def,
                  struct embc_buffer_s * buffer);

    void * rx_user_data;

    /**
     * @brief Function called on successful transmissions.
     *
     * @param user_data The user data.
     * @param port The port (payload type).
     * @param message_id The application-defined message identifier.
     * @param status 0 or error.  The framer module automatically retransmits
     *      frames as needed, and only the last error condition is reported.
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
     * @param buffer The buffer containing the transmit data.  The function
     *      takes ownership of buffer and the HAL can use the item field to
     *      manage its pending list.  When the buffer is transmitted, call
     *      embc_framer_hal_tx_done() to return buffer ownership to the framer.
     *      The callback is also necessary to provide backpressure.  The HAL
     *      must NOT call embc_buffer_free()!
     * @param length The length of buffer in bytes.
     */
    void (*tx_fn)(void * user_data, struct embc_buffer_s * buffer);

    void * tx_user_data;

    /**
     * @brief Set a timer.
     *
     * @param user_data The user data provided to embc_framer_initialize().
     * @param duration The timer duration as 34Q30 seconds relative to the
     *      current time (compatible with time.h).
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
     * @param user_data The user data provided to embc_framer_initialize().
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
EMBC_API embc_size_t embc_framer_instance_size(void);

/**
 * @brief Initialize or reinitialize an instance.
 *
 * @param self The instance which must be at least embc_framer_instance_size()
 *      bytes in size.
 * @param buffer_allocator The buffer allocator for use by embc_framer_alloc().
 * @param callbacks The hardware abstraction layer callbacks.
 * @return 0 or error code.
 *
 * The self instance and the hal callbacks must remain valid until
 * embc_framer_finalize().  Use embc_framer_event_callbacks() to set the event
 * callbacks.
 */
EMBC_API void embc_framer_initialize(
        struct embc_framer_s * self,
        struct embc_buffer_allocator_s * buffer_allocator,
        struct embc_framer_hal_callbacks_s * callbacks);

/**
 * @brief Register the callbacks for a port.
 *
 * @param self The framer instance.
 * @param port The port to register.
 * @param callbacks The event callbacks.
 * @return 0 or error code.
 *
 * The event callbacks must remain valid until another call to
 * embc_framer_register_port_callbacks() or embc_framer_finalize().
 */
EMBC_API void embc_framer_register_port_callbacks(
        struct embc_framer_s * self,
        uint8_t port,
        struct embc_framer_port_callbacks_s const * callbacks);

/**
 * @brief The receive hook for unit testing.
 *
 * @param user_data The arbitrary user data.
 * @param frame The buffer that was received containing the entire frame.
 */
typedef void (*embc_framer_rx_hook_fn)(
        void * user_data,
        struct embc_buffer_s * frame);

/**
 * @brief Register the receive hook for unit testing.
 *
 * @param self The framer instance.
 * @param status The status: 0 on success or error code.
 * @param rx_fn The receive callback hook.  0 resets to the default internal
 *      handler.
 * @param rx_user_data The arbitrary data for rx_fn.
 * @return 0 or error code.
 *
 * This function is exposed only to facilitate unit testing of the underlying
 * framer.  It bypasses the framer logic for issuing ACKs and properly
 * passing frames to the ports.
 */
EMBC_API void embc_framer_register_rx_hook(
        struct embc_framer_s * self,
        embc_framer_rx_hook_fn rx_fn,
        void * rx_user_data);

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
EMBC_API void embc_framer_hal_rx_byte(struct embc_framer_s * self, uint8_t byte);

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
EMBC_API void embc_framer_hal_rx_buffer(
        struct embc_framer_s * self,
        uint8_t const * buffer, embc_size_t length);

/**
 * @brief Handle frame transmit completion.
 *
 * @param self The instance.
 * @param buffer The buffer that was successfully transmitted.
 */
EMBC_API void embc_framer_hal_tx_done(
        struct embc_framer_s * self,
        struct embc_buffer_s * buffer);

/**
 * @brief Construct a raw frame (for unit testing)
 *
 * @param self The instance.
 * @param frame_id The frame_id nibble.
 * @param port The port (payload type).
 * @param message_id The application-defined message identifier.
 * @param port_def The application-defined frame-associated data.
 * @param payload The payload data.
 * @param length The length of payload in bytes.
 * @return The buffer containing the constructed frame.
 */
EMBC_API struct embc_buffer_s * embc_framer_construct_frame(
        struct embc_framer_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const *payload, uint8_t length);

/**
 * @brief Construct an acknowledgement frame (for unit testing)
 *
 * @param self The instance
 * @param frame_id The frame_id nibble.
 * @param port The port (payload type).
 * @param message_id The application-defined message identifier.
 * @param ack_mask The frame ack mask for successfully received data frames.
 *   Bit 8 corresponds to (frame_id - 1) and bit 15 corresponds
 *   to (frame_id - 9).  This field simplifies transmitter retry and reduces
 *   the impact of lost/corrupted ACK frames.
 * @param status The data frame status: 0 on success or error code.
 * @return The buffer containing the acknowledgement frame.
 */
EMBC_API struct embc_buffer_s * embc_framer_construct_ack(
        struct embc_framer_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t ack_mask,
        uint8_t status);

/**
 * @brief Send a frame.
 *
 * @param self The instance.
 * @param port The port (payload type).
 * @param message_id The application-defined message identifier.
 * @param port_def The application-defined frame-associated data.
 * @param buffer The buffer containing the transmit payload.  This function
 *      takes ownership.
 *
 * This function is not reentrant!
 */
EMBC_API void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        struct embc_buffer_s * buffer);

/**
 * @brief Send a frame.
 *
 * @param self The instance.
 * @param port The port (payload type).
 * @param message_id The application-defined message identifier.
 * @param port_def The application-defined frame-associated data.
 * @param data The data.
 * @param length The length of data in bytes.
 *
 * This function is not reentrant!
 */
EMBC_API void embc_framer_send_payload(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * data, uint8_t length);

/**
 * @brief Allocate a buffer for frame payload.
 *
 * @param self The instance.
 * @return The buffer which has the cursor and reserve set for zero-copy
 *      operations.  The caller takes ownership.
 */
EMBC_API struct embc_buffer_s * embc_framer_alloc(
        struct embc_framer_s * self);

/**
 * @brief Get the status for the framer.
 * @param self The framer instance.
 * @return The framer status.
 */
EMBC_API struct embc_framer_status_s embc_framer_status_get(
        struct embc_framer_s * self);

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_STREAM_FRAMER_H_ */
