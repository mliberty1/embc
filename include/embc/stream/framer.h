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
 * @brief Message framer and multiplexer for byte streams.
 */

#ifndef EMBC_FRAMER_H__
#define EMBC_FRAMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @ingroup embc
 * @defgroup embc_framer Message framer and multiplexer for byte streams.
 *
 * @brief Provide reliable framing and multiplexing over total_bytes
 *      streams, such as UART and sockets.
 *
 * This module provides reliable frame transmission over byte streams.
 * Framing multi-byte messages over a byte-oriented interface is a common
 * problem for UART and network communications.  In addition to framing and
 * deframing messages, this module provides reliable delivery through frame
 * retransmission.  The maximum payload size is limited to 256 total_bytes,
 * but larger messages are segmented over multiple frames and then
 * reassembled at the application layer.
 *
 * The features of this framer include:
 *
 * - Robust framing using SOF byte, length, and CRC32.
 * - Fast recovery on errors for minimal RAM usage.
 * - Reliable transmission with acknowledgements.
 * - Guaranteed in-order delivery.
 * - Data corruption detection with not-acknowledgements.
 * - Multiple pending transmit frames for maximum throughput.
 * - Three (3) priority levels to ensure critical communications,
 *   can jump in front of bulk data.
 * - Multiple multiplexed endpoints using "ports".
 * - Large message support using segmentation and reassembly.
 * - Raw loopback for bit-error rate testing.
 * - Full statistics, including frames transferred.
 *
 * For extremely fast transmitters (UART CDC over USB), the maximum number of
 * outstanding frames and acknowledgement turn-around time limit the total
 * rate.  USB has a typically response time of 2 milliseconds, which this
 * framer easily accommodates.  The implementation presumes that this framer
 * is running in the same thread as the receive message processing and
 * can keep up with the raw data rate.
 *
 * The protocol consists of two different frame formats:
 * - data frame
 * - link frame used by ack and nack
 *
 * The data frame format is variable length:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type=000</td>
 *      <td colspan="1">start</td>
 *      <td colspan="1">stop</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="5">port[4:0]</td>
 *  </tr>
 *  <tr><td colspan="8">message_id</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF (optional)</td></tr>
 * </table>
 *
 * - "SOF" is the start of frame byte.  Although SOF is not unique and also not
 *   escaped, the SOF drastically reduces the framing search space.  The
 *   SOF may also be used for autobaud detection.
 * - "frame_type" is the frame type identifier.
 *   - 000: data
 *   - 100: acknowledge (ACK)
 *   - 110: not acknowledge (NACK)
 *   - all other values are invalid and must be discarded.
 * - "frame_id" contains an identifier that is temporally unique for all
 *   DATA frames across all ports.  The frame_id increments sequentially with
 *   each new frame and is assigned by the framer implementation.
 * - "length" is the payload length (not full frame length) in total_bytes, minus 1.
 *   The maximum payload length is 256 total_bytes.  Since the frame overhead is 9
 *   total_bytes, the actual frame length ranges from 9 to 265 total_bytes.
 * - "start" indicates that this frame is the first in a possibly segmented message.
 * - "stop" indicates that this frame is the last in a possibly segmented message.
 * - "port" contains an application-specific payload format identifier.
 *   This field is used to multiplex multiple message types onto a single
 *   byte stream, similar to a TCP port.  Port 0 is reserved for
 *   link management.
 * - "message_id" contains an identifier that is assigned by the application.
 *   Although not required, many applications ensure that message_id values are
 *   unique within each port for all messages currently in flight.
 * - "payload" contains the arbitrary payload of "length" total_bytes.
 * - "frame_crc" contains the CRC32 computed over the header and payload.
 *   The SOF, frame_crc and EOF total_bytes are excluded from the calculation.
 * - "EOF" contains an optional end of frame byte which allows for reliable
 *   receiver timeouts and receiver framer reset.   The value for
 *   EOF and SOF are the same.  Repeated SOF/EOF total_bytes between frames are ignored
 *   by the framer and can be used for autobaud detection.
 *
 * Framing is performed by first searching for the sync byte.  The CRC-32-CCITT
 * is computed over the entire frame from the first non-SOF byte through
 * payload, using the length byte to determine the total byte count.  If the
 * frame_crc total_bytes match the computed CRC, then the entire frame is valid.
 *
 * The acknowledgement (ACK) frame format is a fixed-length frame with
 * 4 total_bytes:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type=100</td>
 *      <td colspan="1">1</td>
 *      <td colspan="1">1</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 * </table>
 *
 * The not-acknowledgement (NACK) frame format is a fixed-length frame
 * with 6 total_bytes:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type=110</td>
 *      <td colspan="1">1</td>
 *      <td colspan="1">1</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr>
 *      <td colspan="1">cause</td>
 *      <td colspan="4">rsv=0</td>
 *      <td colspan="3">cause_frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">cause_frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 * </table>
 *
 * The NACK always returns the frame_id for the most recently,
 * valid frame received in sequence.  The cause indicates
 * more information to the receiver to help simplify retransmission.
 * The cause can be:
 * - 0: framer error.  cause_frame_id is 0 and should be ignored.
 * - 1: invalid frame_id.  The cause_frame_id contains the actual
 *   frame_id received.
 *
 *
 * Ack / Nack process
 *
 * The transmitter constructs and sends frames with an incrementing frame_id
 * that rolls over to zero after reaching the maximum.  When the receiver
 * receives the next frame in the sequence, it sends an ACK back to the
 * sender.  When the sender receives an ACK, it can recycle the frame buffers
 * for all frame_ids up to the ACK.
 *
 * The transmitter can have at most frame_id_max / 2 frames - 1 outstanding,
 * which we call frame_pend_max.  The receiver can then treat
 * frame_id - frame_pend_max (computed with wrap) as the "past" and
 * frame_id + frame_pend_max (computed with wrap) as the "future".
 *
 * When the receiver receives a "past" frame, the receiver discards it but
 * does send an ACK to the transmitter.
 * If the receiver receives a "future" frame beyond the expected next frame_id,
 * the the receiver must have missed at least one frame. The receiver
 * immediately sends a NACK containing the frame_id of the last successfully
 * received frame. The receiver then discards all future frames until it
 * finally receives the expected frame_id.  The receiver continues to send
 * NACKs for each future frame received.  The transmitter will receive
 * multiple NACKs, but it can use cause_frame_id to differentiate
 * the duplicated NACKs.  Even with frames still in flight, the transmitter
 * can retransmit and ignore the NACKs that will be returned for any
 * frames still in flight.
 *
 * If the receiver receives a framing error or the frame_id does not increment,
 * it immediately sends a NACK containing the frame_id of the expected frame.
 *
 * When the transmitter receives a NACK, it may either complete the current
 * frame transmission or immediately halt it.  The transmitter should then
 * retransmit starting with the frame_id + 1 indicated in the NACK.  The
 * transmitter knows the outstanding frames, and should expect to receive
 * NACKs for them.  Since the transmitter knows the expected NACKs, it can
 * safely ignore them.
 *
 * This framer contains support for backpressure by providing notifications
 * when the recipient acknowledge the frame transmission.
 * The application can configure the desired number of pending
 * send frames based upon memory availability and application complexity.
 *
 *
 * Bandwidth analysis
 *
 * The protocol provides support for up to 2 ^ 11 / 2  - 1 = 1023 frames in flight.
 * With maximum payload size of 256 total_bytes and 10 total_bytes overhead on a 3 Mbaud link,
 * the protocol is capable of a maximum outstanding duration of:
 *
 *     (1023 * (256 + 10)) / (3000000 / 10) = 0.907 seconds
 *
 * However, the transmitter must have a transmit buffer of at least 272,118 total_bytes,
 * too much for many intended microcontrollers.  A typical microcontroller may
 * allocate 32 outstanding frames, which is 8512 total_bytes.  To prevent stalling the
 * link with full payloads, the receive must ACK within:
 *
 *     (32 * (256 + 10)) / (3000000 / 10) = 28 milliseconds
 *
 * While direct communication between microcontrollers with fast interrupt handling
 * can use even smaller buffers, this delay is reasonable for communication
 * between a microcontroller and a host computer over USB CDC.
 *
 * Under Windows, you may want to ensure that your timer resolution is set to
 * 1 millisecond or less.  You can use
 * [TimerTool](https://github.com/tebjan/TimerTool) to check.
 *
 *
 * ## References
 *
 *    - Overview:
 *      [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications),
 *      [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *    - PPP:
 *      [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      [RFC](https://tools.ietf.org/html/rfc1661),
 *      [Segger embOS/embNet PPP/PPPoE](https://www.segger.com/products/connectivity/emnet/add-ons/ppppppoe/)
 *    - Constant Overhead Byte Stuffing (COBS):
 *      [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *    - [Telemetry](https://github.com/Overdrivr/Telemetry)
 *    - [Microcontroller Interconnect Network](https://github.com/min-protocol/min)
 */

enum embc_framer_priorities_e {
    EMBC_FRAMER_PRIORITY_LOW = 0,
    EMBC_FRAMER_PRIORITY_NORMAL = 1,
    EMBC_FRAMER_PRIORITY_HIGH = 2,
    EMBC_FRAMER_PRIORITY_ACK = 3  ///< Reserved for ACK/NACK only.
};

/// The value for the start of frame byte.
#define EMBC_FRAMER_SOF ((uint8_t) 0x55)
/// The framer header size in total_bytes.
#define EMBC_FRAMER_HEADER_SIZE (6)
/// The maximum payload size in total_bytes.
#define EMBC_FRAMER_PAYLOAD_MAX_SIZE (256)
/// The framer footer size in total_bytes.
#define EMBC_FRAMER_FOOTER_SIZE (4)
/// The framer total maximum size in total_bytes
#define EMBC_FRAMER_FRAME_MAX_SIZE (\
    EMBC_FRAMER_HEADER_SIZE + \
    EMBC_FRAMER_PAYLOAD_MAX_SIZE + \
    EMBC_FRAMER_FOOTER_SIZE)
/// The maximum available number of ports
#define EMBC_FRAMER_PORTS_MAX (32)
#define EMBC_FRAMER_ACK_SIZE (4)
#define EMBC_FRAMER_NACK_SIZE (6)
#define EMBC_FRAMER_MIN_SIZE (EMBC_FRAMER_HEADER_SIZE + EMBC_FRAMER_FOOTER_SIZE + 1)
#define EMBC_FRAMER_OVERHEAD_SIZE (EMBC_FRAMER_HEADER_SIZE + EMBC_FRAMER_FOOTER_SIZE)
#define EMBC_FRAMER_COUNT (2048)
#define EMBC_FRAMER_INFLIGHT_MAX (EMBC_FRAMER_COUNT / 2 - 1)

/// opaque framer instance.
struct embc_framer_s;

enum embc_framer_frame_type_e {
    EMBC_FRAMER_FT_INVALID,
    EMBC_FRAMER_FT_DATA,
    EMBC_FRAMER_FT_ACK,
    EMBC_FRAMER_FT_NACK,
};

enum embc_framer_nack_cause_e {
    EMBC_FRAMER_NACK_CAUSE_FRAME_ERROR = 0,
    EMBC_FRAMER_NACK_CAUSE_FRAME_ID = 1,
};

struct embc_framer_rx_status_s {
    uint64_t total_bytes;
    uint64_t invalid_bytes;
    uint64_t data_frames;
    uint64_t crc_errors;
    uint64_t frame_id_errors;
    uint64_t frames_missing;
    uint64_t resync;
    uint64_t frame_too_big;
};

struct embc_framer_tx_status_s {
    uint64_t bytes;
    uint64_t data_frames;
};

/**
 * @brief The framer instance statistics.
 */
struct embc_framer_status_s {
    uint32_t version;
    uint32_t reserved;
    struct embc_framer_rx_status_s rx;
    struct embc_framer_tx_status_s tx;
    uint64_t send_buffers_free;
};

enum embc_framer_sequence_e {
    EMBC_FRAMER_SEQUENCE_MIDDLE = 0x0,
    EMBC_FRAMER_SEQUENCE_END = 0x1,
    EMBC_FRAMER_SEQUENCE_START = 0x2,
    EMBC_FRAMER_SEQUENCE_SINGLE = 0x3
};

/**
 * @brief The framer events.
 */
enum embc_framer_event_s {
    EMBC_FRAMER_EV_UNDEFINED,
    EMBC_FRAMER_EV_CONNECT,
};

/**
 * @brief The function called on framer events.
 */
typedef void (*embc_framer_event_cbk)(void * user_data,
                                      struct embc_framer_s * instance,
                                      enum embc_framer_event_s event);

/**
 * @brief The commands defined for port 0.
 */
enum embc_framer_port0_cmd_e {
    EMBC_FRAMER_PORT0_IGNORE = 0x00,
    EMBC_FRAMER_PORT0_CONNECT = 0x01,
    EMBC_FRAMER_PORT0_CONNECT_ACK = 0x81,
    EMBC_FRAMER_PORT0_PING_REQ = 0x02,
    EMBC_FRAMER_PORT0_PING_RSP = 0x82,
    EMBC_FRAMER_PORT0_STATUS_REQ = 0x03,
    EMBC_FRAMER_PORT0_STATUS = 0x83,
    EMBC_FRAMER_PORT0_LOOPBACK_REQ = 0x05,
    EMBC_FRAMER_PORT0_LOOPBACK_RSP = 0x85,
};

/**
 * @brief The framer configuration.
 */
struct embc_framer_config_s {
    /// The number of ports to use (EMBC_FRAMER_PORTS_MAX max).
    uint32_t ports;

    /// The number of send frame buffers to allocate.
    uint32_t send_frames;

    /// The function called on events.
    embc_framer_event_cbk event_cbk;
    /// The arbitrary data for event_cbk.
    void * event_user_data;
};

/**
 * @brief The application interface for each port.
 */
struct embc_framer_port_s {
    /**
     * @brief The arbitrary application data for this instance.
     *
     * This value is passed as the first variable to each of the port callback
     * functions.
     */
    void * user_data;

    /**
     * @brief The function called after the framer sends a frame.
     *
     * @param user_data The arbitrary user data.
     * @param port_id The port identifier used to send the frame.
     * @param message_id The message_id provided to embc_framer_send()
     *
     * This function is called from the transmit UART thread.  The
     * function is responsible for thread resynchronization.
     */
    void (*send_done_cbk)(void *user_data,
                     uint8_t port_id, uint8_t message_id);

    /**
     * @brief The function called when the framer receives a frame.
     *
     * @param user_data The arbitrary user data.
     * @param port_id The received message port.
     * @param message_id The received message_id.
     * @param msg_buffer The msg_buffer containing the message.  This buffer
     *      is only valid for the duration of the callback.  The receiver
     *      must copy the message if it is not immediately processed.
     * @param msg_size The size of msg_buffer in total_bytes.
     *
     * This function is called from the receive UART thread.  The
     * function is responsible for thread resynchronization.
     */
    void (*recv_cbk)(void *user_data,
                     uint8_t port_id, uint8_t message_id,
                     uint8_t *msg_buffer, uint32_t msg_size);
};

/**
 * @brief Register the port application client that receives callbacks.
 *
 * @param self The framer instance.
 * @param port_id The port to register.
 * @param port_instance The port instance.
 * @return 0 or error code.
 *
 * The event callbacks must remain valid until another call to
 * embc_framer_register_port_callbacks() or embc_framer_finalize().
 *
 * Only one callback may be registered per port.  Registering a new callback
 * will unregister the existing callback.  Provide NULL to unregister the port.
 */
int32_t embc_framer_port_register(struct embc_framer_s * self,
                                  uint8_t port_id,
                                  struct embc_framer_port_s * port_instance);

/**
 * @brief Send a message.
 *
 * @param self The framer instance.
 * @param priority The message priority in the range of 0 to UART_PRIORITIES.
 *      0 is the lowest priority.
 * @param port_id The port identifier for the message.
 * @param message_id The arbitrary message_id, often used to match the
 *      send callback and received response messages.
 * @param msg_buffer The msg_buffer containing the message.  The driver
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 *
 * The port send_done_cbk callback will be called when the send completes.
 */
int32_t embc_framer_send(struct embc_framer_s * self,
                         uint8_t priority, uint8_t port_id, uint8_t message_id,
                         uint8_t const *msg_buffer, uint32_t msg_size);

/**
 * @brief The upper-level framer callbacks used by the low-level driver.
 */
struct embc_framer_ul_s {
    /**
     * @brief The upper-level framer instance.
     *
     * This value is passed as the first variable to each
     * low-level driver callback.
     */
    void *ul_user_data;

    /**
     * @brief Provide receive data to the upper-level framer.
     *
     * @param ul_user_data The instance.
     * @param buffer The data received, which is only valid for the
     *      duration of the callback.
     * @param buffer_size The size of buffer in total_bytes.
     */
    void (*recv)(void *ul_user_data, uint8_t const * buffer, uint32_t buffer_size);

    /**
     * @brief Function call to indicate that the send operation completed.
     *
     * @param ul_user_data The instance.
     * @param buffer The data buffer sent.  Ownership is returned to
     *      the upper level driver.
     * @param buffer_size The size of buffer in total_bytes.
     */
    void (*send_done)(void *ul_user_data, uint8_t * buffer, uint32_t buffer_size);
};

/**
 * @brief The low-level abstract driver implementation.
 */
struct embc_framer_ll_s {
    /**
     * @brief The low-level driver instance.
     *
     * This value is passed as the first variable to each
     * low-level driver callback.
     */
    void * ll_user_data;

    /**
     * @brief Open the low-level driver instance.
     *
     * @param ll_user_data The instance.
     * @return 0 or error code.
     */
    int32_t (*open)(void * ll_user_data,
                    struct embc_framer_ul_s * ul_instance);

    /**
     * @brief Close the low-level driver instance.
     *
     * @param ll_user_data The instance.
     * @return 0 or error code.
     */
    int32_t (*close)(void * ll_user_data);

    /**
     * @brief Write data to the low-level driver instance.
     *
     * @param ll_user_data The instance.
     * @param buffer The buffer containing the data to send which must
     *      remain valid until the send completes.  The low-level driver
     *      must call uart_ll_cbk_send_done() when complete.
     * @param buffer_size The size of buffer in total_bytes.
     *
     * This function must call ul_instance->send_done() when it no
     * longer needs buffer.  The send_done() may occur within this function.
     */
    void (*send)(void * ll_user_data, uint8_t * buffer, uint32_t buffer_size);
    // note: recv from the lower-level driver using uart_ul_s.recv.
};

/**
 * @brief The framer OS timer interface.
 */
struct embc_framer_hal_s {
    /**
     * @brief The HAL instance.
     *
     * This value is passed as the first variable to each of the HAL callback
     * functions.  Most HAL implementations will either ignore this value
     * and have static (singleton) implementations, or they will pass a struct
     * for a C-style class implementation.
     */
    void * hal;

    /**
     * @brief Get the current time.
     *
     * @param hal The HAL instance (user_data).
     * @return The current time as 34Q30 which is compatible with embc/time.h.
     *      The framer module only uses relative time.  The HAL
     *      implementation is free to select any definition of 0
     *      relative to UTC, and it may be relative to the last reset.
     */
    int64_t (*time_get)(void * hal);

    /**
     * @brief Set a timer.
     *
     * @param hal The HAL instance (user_data).
     * @param timeout The timer timeout as 34Q30 seconds in the same relative
     *      units as the time_get() value.  This argument is compatible with
     *      embc/time.h.  The timeout rather than the duration is used to
     *      eliminate any errors due to processing or servicing delays.
     * @param cbk_fn The function to call if the timer expires which has
     *      arguments of (user_data, timer_id).
     * @param cbk_user_data The additional data to provide to cbk_fn.
     * @param[out] timer_id The assigned event id.
     * @return 0 or error code.
     *
     * If timeout has already passed, then cbk_fn may be called from within
     * this function.  The caller should be ready for the callback before
     * invoking timer_set_fn().
     *
     * A framer implementation is guaranteed to only set a single timer at
     * a time.  The framer implementation will cancel any existing timer before
     * scheduling (or rescheduling) the next timer.
     */
    int32_t (*timer_set_fn)(void * hal, int64_t timeout,
                            void (*cbk_fn)(void *, uint32_t), void * cbk_user_data,
                            uint32_t * timer_id);

    /**
     * @brief Cancel a timer.
     *
     * @param hal The HAL instance (user_data).
     * @param timer_id The timer id assigned by timer_set().
     */
    int32_t (*timer_cancel_fn)(void * hal, uint32_t timer_id);
};

/**
 * @brief Allocate, initialize, and start the framer.
 *
 * @param config The framer configuration.
 * @param hal The framer OS abstraction layer.
 * @param ll_instance The lower-level driver instance.
 * @return The new framer instance.
 */
struct embc_framer_s * embc_framer_initialize(
        struct embc_framer_config_s const * config,
        struct embc_framer_hal_s * hal,
        struct embc_framer_ll_s * ll_instance);

/**
 * @brief Stop, finalize, and deallocate the framer.
 *
 * @param self The framer instance.
 * @return 0 or error code.
 *
 * While this method is provided for completeness, most embedded systems will
 * not use it.  Implementations without "free" may fail.
 */
int32_t embc_framer_finalize(struct embc_framer_s * self);

/**
 * @brief Get the status for the framer.
 *
 * @param self The framer instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
int32_t embc_framer_status_get(
        struct embc_framer_s * self,
        struct embc_framer_status_s * status);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_FRAMER_H__ */
