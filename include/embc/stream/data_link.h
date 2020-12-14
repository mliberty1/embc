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
 * @brief Reliable data link layer for byte streams.
 */

#ifndef EMBC_STREAM_DATA_LINK_H__
#define EMBC_STREAM_DATA_LINK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "embc/stream/framer.h"
#include <stdint.h>

/**
 * @ingroup embc
 * @defgroup embc_data_link Reliable data link layer for byte streams.
 *
 * @brief Provide reliable framing and retransmission over byte
 *      streams, such as UART and sockets.
 *
 * This module provides reliable message transmission over byte streams.
 * Framing multi-byte messages over a byte-oriented interface is a common
 * problem for UART and network communications.  In addition to framing and
 * deframing messages, this module provides reliable delivery through frame
 * retransmission.  The maximum payload size is limited to 256 total_bytes,
 * but larger messages are segmented over multiple frames and then
 * reassembled at the application layer.
 *
 * The features of this data link include:
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
 * - link frame used by acks, nacks, and reset
 *
 * The data frame format is variable length:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF1[7:0]</td></tr>
 *  <tr><td colspan="8">SOF2[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type=000</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[15:8]</td></tr>
 *  <tr><td colspan="8">metadata[23:16]</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF (optional)</td></tr>
 * </table>
 *
 * - "SOF1" is the start of frame byte.  Although SOF1 and SOF2 are not unique
 *   and also not escaped, the SOF bytes drastically reduces the framing
 *   search space.  The SOF1 value can be selected for autobaud detection.
 *   Typical values are 0x55 and 0xAA.
 * - "SOF2" is the second start of frame byte.  The SOF2 value can be selected
 *   to ensure proper UART framing.  Typical values are 0x00.
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
 * - "metadata" contains arbitrary 24-bit data that is transmitted along with the
 *   message payload.  The metadata format is usually assigned by the higher-level
 *   protocol or application.  The embc/stream/transport.h defines this field
 *   for the optional transport level.  Common "metadata" uses include:
 *   - "port" to multiplex multiple message types or endpoints onto
 *     this single byte stream, similar to a TCP port.
 *   - "start" and "stop" bits to segment and reassemble messages larger than
 *     the frame payload size.  For example,
 *     10 is start, 01 is end, 00 is middle and 11 is a single frame message.
 *   - "unique_id" that is unique for all messages in flight for a prot.
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
 * The link frame format is a fixed-length frame with
 * 8 total_bytes:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF1[7:0]</td></tr>
 *  <tr><td colspan="8">SOF2[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 * </table>
 *
 * The link frame frame_types are:
 *
 * - ACK_ALL: Receiver has all frame_ids up to and including the
 *   indicated frame_id.
 * - ACK_ONE: Receiver has received frame_id, but is missing
 *   one or more previous frame_ids.
 * - NACK_ONE: Receiver did not correctly receive the frame_id.
 * - NACK_FRAMING_ERROR: A framing error occurred.  The frame_id
 *   indicates the most recent, correctly received frame.
 *   Note that this may not be lowest frame_id.
 * - RESET: Reset all state.
 *
 *
 * Retranmissions
 *
 * This data link layer
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
 *      - [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications),
 *      - [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *      - [Daniel Beer](https://dlbeer.co.nz/articles/packet.html)
 *    - Selective Repeat Automated Repeat Request (SR-ARQ)
 *      - [wikipedia](https://en.wikipedia.org/wiki/Selective_Repeat_ARQ)
 *    - PPP:
 *      [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      [RFC](https://tools.ietf.org/html/rfc1661),
 *      [Segger embOS/embNet PPP/PPPoE](https://www.segger.com/products/connectivity/emnet/add-ons/ppppppoe/)
 *    - HDLC
 *      - [wikipedia](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control)
 *    - Constant Overhead Byte Stuffing (COBS):
 *      - [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *      - [Embedded Related post](https://www.embeddedrelated.com/showarticle/113.php)
 *    - Alternatives:
 *      - [Microcontroller Interconnect Network](https://github.com/min-protocol/min)
 *      - [Telemetry](https://github.com/Overdrivr/Telemetry)
 *      - [SerialFiller](https://github.com/gbmhunter/SerialFiller)
 *      - [TinyFrame](https://github.com/MightyPork/TinyFrame)
 *      - [P2P UART Network](https://github.com/bowen-liu/P2P-UART-Network)
 */

/// opaque data link instance.
struct embc_dl_s;


#define EMBC_DL_INFLIGHT_MAX (EMBC_FRAMER_COUNT / 2 - 1)

struct embc_dl_config_s {
    uint32_t tx_link_buffer_size;  // in bytes
    uint32_t tx_window_size;  // in frames
    uint32_t tx_buffer_size;  // in bytes
    uint32_t rx_window_size;  // in frames
    uint32_t rx_buffer_size;  // in bytes
    uint32_t tx_timeout_ms;   // transmit timeout in milliseconds
};

struct embc_dl_tx_status_s {
    uint64_t bytes;
    uint64_t msg_bytes;
    uint64_t data_frames;
    uint64_t retransmissions;
};

struct embc_dl_rx_status_s {
    uint64_t msg_bytes;
    uint64_t data_frames;
};

/**
 * @brief The data link instance statistics.
 */
struct embc_dl_status_s {
    uint32_t version;
    uint32_t reserved;
    struct embc_dl_rx_status_s rx;
    struct embc_framer_status_s rx_framer;
    struct embc_dl_tx_status_s tx;
    uint64_t send_buffers_free;
};

/**
 * @brief The API event callbacks to the upper layer.
 */
struct embc_dl_api_s {
    /// The arbitrary user data.
    void *user_data;

    /**
     * @brief The function called when the remote host issues a reset.
     *
     * @param user_data The arbitrary user data.
     */
    void (*reset_fn)(void *user_data);

    /**
     * @brief The function called upon message receipt.
     *
     * @param user_data The arbitrary user data.
     * @param metadata The arbitrary 24-bit metadata associated with the message.
     * @param msg The buffer containing the message.
     *      This buffer is only valid for the duration of the callback.
     * @param msg_size The size of msg_buffer in bytes.
     */
    void (*recv_fn)(void *user_data, uint32_t metadata,
                    uint8_t *msg, uint32_t msg_size);
};

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
int32_t embc_dl_send(struct embc_dl_s * self, uint32_t metadata,
                     uint8_t const *msg, uint32_t msg_size);

/**
 * @brief Provide receive data to this data link instance.
 *
 * @param self The data link instance.
 * @param buffer The data received, which is only valid for the
 *      duration of the callback.
 * @param buffer_size The size of buffer in total_bytes.
 */
void embc_dl_ll_recv(struct embc_dl_s * self,
                     uint8_t const * buffer, uint32_t buffer_size);

/**
 * @brief The maximum time until the next embc_dl_process() call.
 *
 * @param self The instance.
 * @return The maximum time in milliseconds until the system must call
 *      embc_dl_process().  The system may call process sooner.
 */
uint32_t embc_dl_service_interval_ms(struct embc_dl_s * self);

/**
 * @brief Process to handle retransmission.
 *
 * @param self The instance.
 */
void embc_dl_process(struct embc_dl_s * self);

/**
 * @brief The low-level abstract driver implementation.
 */
struct embc_dl_ll_s {
    /**
     * @brief The low-level driver instance.
     *
     * This value is passed as the first variable to each
     * low-level driver callback.
     */
    void * user_data;

    /**
     * @brief Get the current time in milliseconds
     *
     * @param hal The HAL instance (user_data).
     * @return The current time in milliseconds.
     *      The data link module only uses relative time.  The HAL
     *      implementation is free to select any definition of 0.
     *      This value wraps every 49 days.
     */
    uint32_t (*time_get_ms)(void * user_data);

    /**
     * @brief Write data to the low-level driver instance.
     *
     * @param user_data The instance.
     * @param buffer The buffer containing the data to send which must
     *      remain valid until the send completes.  The low-level driver
     *      must call uart_ll_cbk_send_done() when complete.
     * @param buffer_size The size of buffer in total_bytes.
     *
     * This function must call ul_instance->send_done() when it no
     * longer needs buffer.  The send_done() may occur within this function.
     */
    void (*send)(void * user_data, uint8_t const * buffer, uint32_t buffer_size);

    /**
     * @brief The number of bytes currently available to send().
     *
     * @param user_data The instance.
     * @return The non-blocking free space available to send().
     */
    uint32_t (*send_available)(void * user_data);

    // note: recv is performed through the embc_dl_ll_recv with the dl instance.
};

/**
 * @brief Allocate, initialize, and start the data link layer.
 *
 * @param config The data link configuration.
 * @param ll_instance The lower-level driver instance.
 * @return The new data link instance.
 */
struct embc_dl_s * embc_dl_initialize(
        struct embc_dl_config_s const * config,
        struct embc_dl_ll_s const * ll_instance);

/**
 * @brief Register callbacks for the upper-layer.
 *
 * @param self The data link instance.
 * @param ul The upper layer
 */
void embc_dl_register_upper_layer(struct embc_dl_s * self, struct embc_dl_api_s const * ul);

/**
 * @brief Reset the data link state.
 *
 * @param self The data link instance.
 */
void embc_dl_reset(struct embc_dl_s * self);

/**
 * @brief Stop, finalize, and deallocate the data link instance.
 *
 * @param self The data link instance.
 * @return 0 or error code.
 *
 * While this method is provided for completeness, most embedded systems will
 * not use it.  Implementations without "free" may fail.
 */
int32_t embc_dl_finalize(struct embc_dl_s * self);

/**
 * @brief Get the status for the data link.
 *
 * @param self The data link instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
int32_t embc_dl_status_get(
        struct embc_dl_s * self,
        struct embc_dl_status_s * status);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_FRAMER_H__ */
