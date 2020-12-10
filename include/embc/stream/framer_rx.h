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
 * @brief Lower-level receive framer.
 */

#ifndef EMBC_FRAMER_RX_H__
#define EMBC_FRAMER_RX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "framer.h"
#include <stdint.h>

/**
 * @ingroup embc_framer
 * @defgroup embc_framer_rx Lower-level receive framer.
 *
 * @brief Implement the byte-level receive framer.
 *
 * This module segments a byte stream into received frames.
 * See embc_framer for the full frame implementation.
 * This module is separated to facilitate unit testing.
 */

/**
 * @brief The application interface for received frames.
 */
struct embc_framer_rx_api_s {
    /**
     * @brief The arbitrary application data for this instance.
     *
     * This value is passed as the first variable to each of the port callback
     * functions.
     */
    void * user_data;

    /**
     * @brief The function call for any framing errors.
     *
     * @param user_data The arbitrary user data.
     */
    void (*on_frame_error)(void * user_data);

    /**
     * @brief The function called for each acknowledgement (ACK)
     *
     * @param user_data The arbitrary user data.
     * @param frame_id The frame_id for the acknowledgement.
     */
    void (*on_ack)(void * user_data, uint16_t frame_id);

    /**
     * @brief The function called for each not acknowledgement (NACK).
     *
     * @param user_data The arbitrary user data.
     * @param frame_id The expected frame_id.
     * @param cause The embc_framer_nack_cause_e.
     * @param cause_frame_id For cause=EMBC_FRAMER_NACK_CAUSE_FRAME_ID,
     *      the frame_id which triggered this NACK.
     */
    void (*on_nack)(void * user_data, uint16_t frame_id, uint8_t cause, uint16_t cause_frame_id);

    /**
     * @brief The function called for each data frame.
     *
     * @param user_data The arbitrary user data.
     * @param frame_id The data frame_id.
     * @param seq The embc_framer_sequence_e (single, start, middle, end).
     * @param port_id The port number for this frame.
     * @param message_id The application message id for this frame.
     * @param buf The buffer containing the frame payload.
     * @param buf_size The size of buf.
     */
    void (*on_frame)(void * user_data, uint16_t frame_id, enum embc_framer_sequence_e seq,
                     uint8_t port_id, uint8_t message_id, uint8_t * buf, uint16_t buf_size);
};

/**
 * @brief The receive framer instance.
 */
struct embc_framer_rx_s {
    struct embc_framer_rx_api_s api;
    uint8_t buf[EMBC_FRAMER_PAYLOAD_MAX_SIZE + EMBC_FRAMER_OVERHEAD_SIZE];
    uint32_t buf_offset;  // offset into rx_buf
    uint8_t state;
    uint16_t length;
    uint8_t is_sync;
    struct embc_framer_rx_status_s status;
};

/**
 * @brief Initialize the receive framer.
 *
 * @param self The framer instance, allocated externally.
 */
void embc_framer_rx_initialize(struct embc_framer_rx_s * self);

/**
 * @brief Process received data through the framer.
 *
 * @param self The framer instance.
 * @param buffer The buffer containing the received data to process.
 * @param buffer_size The length of buffer in bytes.
 */
void embc_framer_rx_recv(struct embc_framer_rx_s * self, uint8_t const * buffer, uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_FRAMER_RX_H__ */
