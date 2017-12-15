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

#include "embc/stream/framer.h"
#include "embc/stream/framer_util.h"
#include "embc/collections/list.h"
#include "embc/collections/bbuf.h"
#include "embc.h"
#include "embc/crc.h"

#define SOF ((uint8_t) EMBC_FRAMER_SOF)
#define HEADER_SIZE ((uint16_t) EMBC_FRAMER_HEADER_SIZE)
#define FOOTER_SIZE ((uint16_t) EMBC_FRAMER_FOOTER_SIZE)

enum embc_framer_state_e {
    ST_SOF_UNSYNC = 0x00,
    ST_HEADER_UNSYNC = 0x01,
    ST_SOF = 0x80,
    ST_HEADER = 0x81,
    ST_PAYLOAD_AND_FOOTER = 0x82,
};

struct embc_framer_rx_s {
    uint8_t frame_id;
    uint8_t b[EMBC_FRAMER_FRAME_MAX_SIZE];
    uint16_t size;
    struct embc_list_s item;
};

struct embc_framer_tx_s {
    uint8_t frame_id;
    uint8_t b[EMBC_FRAMER_FRAME_MAX_SIZE];
    uint16_t size;
    struct embc_list_s item;
};

struct embc_framer_ack_s {
    uint8_t b[EMBC_FRAMER_ACK_FRAME_SIZE];
};

struct embc_framer_s {
    struct embc_framer_event_callbacks_s event_cbk;
    struct embc_framer_hal_callbacks_s hal_cbk;

    uint8_t rx_frame_id;
    struct embc_framer_rx_s rx[EMBC_FRAMER_OUTSTANDING_FRAMES_MAX];
    uint16_t rx_offset;
    uint16_t rx_remaining;
    uint8_t rx_state;
    struct embc_framer_rx_s * rx_active;
    struct embc_list_s rx_free;
    struct embc_list_s rx_pending;

    struct embc_framer_tx_s tx[EMBC_FRAMER_OUTSTANDING_FRAMES_MAX];
    uint8_t tx_frame_id;
    struct embc_list_s tx_free;
    struct embc_list_s tx_pending;

    struct embc_framer_ack_s ack[EMBC_FRAMER_OUTSTANDING_FRAMES_MAX + 1];
    uint8_t ack_index;
};

uint32_t embc_framer_instance_size(void) {
    return sizeof(struct embc_framer_s);
}

void embc_framer_initialize(
        struct embc_framer_s * self,
        struct embc_framer_hal_callbacks_s * callbacks) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(callbacks);
    DBC_NOT_NULL(callbacks->tx_fn);
    DBC_NOT_NULL(callbacks->timer_set_fn);
    DBC_NOT_NULL(callbacks->timer_cancel_fn);
    EMBC_STRUCT_PTR_INIT(self);
    self->hal_cbk = *callbacks;

    // initialize the RX messages
    embc_list_initialize(&self->rx_free);
    embc_list_initialize(&self->rx_pending);
    self->rx_active =  &self->rx[0];
    for (embc_size_t i = 1; i < EMBC_ARRAY_SIZE(self->rx); ++i) {
        embc_list_add_tail(&self->rx_free, &self->rx[i].item);
    }

    // initialize the TX messages
    embc_list_initialize(&self->tx_free);
    embc_list_initialize(&self->tx_pending);
    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->tx); ++i) {
        embc_list_add_tail(&self->tx_free, &self->tx[i].item);
    }
}

void embc_framer_event_callbacks(
        struct embc_framer_s * self,
        struct embc_framer_event_callbacks_s * callbacks) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(callbacks);
    DBC_NOT_NULL(callbacks->rx_fn);
    DBC_NOT_NULL(callbacks->tx_done_fn);
    self->event_cbk = *callbacks;
}

void embc_framer_finalize(struct embc_framer_s * self) {
    (void) self;
}

/**
 * @brief Check if the frame header is valid.
 * @param buffer The buffer pointing to the start of the frame.
 *      The buffer must be at least HEADER_SIZE bytes long.
 * @return 1 if value, 0 if invalid.
 */
static int is_header_valid(uint8_t const * buffer) {
    return (
            (buffer[0] == SOF) &&
            (buffer[HEADER_SIZE - 1] == crc_ccitt_8(0, buffer, HEADER_SIZE - 1))
    );
}

static uint16_t frame_payload_length(uint8_t const * buffer) {
    uint16_t length = buffer[4];
    return (length == 0) ? 256 : length;
}

static uint16_t frame_length(uint8_t const * buffer) {
    return frame_payload_length(buffer) + HEADER_SIZE + FOOTER_SIZE;
}

#if 0
static void signal_error(struct embc_framer_s * self, uint8_t * buffer, uint8_t error) {
    uint8_t id = 0;
    if (0 == (self->rx_state & 0x80)) { // no errors until synchronized
        return;
    }
    if (!buffer) {
        buffer = self->rx_buffer;
    }
    uint16_t sz = self->rx_offset - (buffer - self->rx_buffer);
    if (sz >= 2) {
        id = buffer[1] & 0x0f;
    }
    self->error_fn(self->error_user_data, id, error);
}

static void signal_rx(struct embc_framer_s * self, uint8_t * buffer) {
    if (!buffer) {
        buffer = self->rx_buffer;
    }
    uint8_t id = buffer[1] & 0x0f;
    uint8_t port = buffer[2];
    self->rx_fn(self->rx_user_data, id, port,
                buffer + HEADER_SIZE,
                frame_payload_length(buffer));
}
#endif

int32_t embc_framer_validate(uint8_t const * buffer,
                                    uint16_t buffer_length,
                                    uint16_t * length) {
    if (buffer_length < HEADER_SIZE) {
        return EMBC_ERROR_EMPTY;
    }
    if (!is_header_valid(buffer)) {
        return EMBC_ERROR_IO;
    }
    uint16_t sz = frame_length(buffer);
    if (length) {
        *length = sz;
    }
    if (buffer_length < sz) {
        return EMBC_ERROR_TOO_SMALL;
    }
    uint8_t const * crc_value = &buffer[sz - 5];
    uint32_t crc_rx = BBUF_DECODE_U32_LE(crc_value);
    uint32_t crc_calc = crc32(0, buffer, sz - FOOTER_SIZE);
    if (crc_rx != crc_calc) {
        return EMBC_ERROR_MESSAGE_INTEGRITY;
    }
    return 0;
}

#if 0
static void framer_resync(struct embc_framer_s * self) {
    uint16_t count = self->rx_offset;
    uint16_t sz = 0;
    uint16_t i = 1;
    uint16_t length;
    self->rx_state = ST_SOF_UNSYNC;
    for (; i < count; ++i) {
        uint8_t * b = self->rx_buffer + i;
        if (*b != SOF) {
            continue;
        }
        sz = count - i;
        switch (embc_framer_validate(b, sz, &length)) {
            case EMBC_ERROR_EMPTY:
                self->rx_remaining = HEADER_SIZE - sz;
                self->rx_state = ST_HEADER_UNSYNC;
                goto exit;
            case EMBC_ERROR_TOO_SMALL:
                self->rx_remaining = length - sz;
                self->rx_state = ST_PAYLOAD_AND_FOOTER;
                goto exit;
            case EMBC_ERROR_MESSAGE_INTEGRITY:
                signal_error(self, b, EMBC_ERROR_MESSAGE_INTEGRITY);
                break;
            case EMBC_SUCCESS:
                self->rx_state = ST_SOF;
                signal_rx(self, b);
                i += length - 1;
                break;
            case EMBC_ERROR_IO:
                break;
            default:
                break;
        }
    }
exit:
    sz = count - i;
    for (uint16_t j = 0; j < sz; ++j) {
        self->rx_buffer[j] = self->rx_buffer[i + j];
    }
    self->rx_offset = sz;
}

void embc_framer_rx_byte(struct embc_framer_s * self, uint8_t byte) {
    DBC_NOT_NULL(self);
    self->rx_buffer[self->rx_offset++] = byte;

    switch (self->rx_state) {
        case ST_SOF_UNSYNC: /* intentional fall-through. */
        case ST_SOF:
            if (byte == SOF) {
                self->rx_buffer[0] = SOF;
                self->rx_offset = 1;
            } else {
                self->rx_remaining = HEADER_SIZE - 2;
                self->rx_state = ST_HEADER_UNSYNC | (self->rx_state & 0x80);
            }
            break;

        case ST_HEADER_UNSYNC: /* intentional fall-through. */
        case ST_HEADER:
            --self->rx_remaining;
            if (self->rx_remaining == 0) {
                if (is_header_valid(self->rx_buffer)) {
                    self->rx_remaining = frame_payload_length(self->rx_buffer)
                                         + FOOTER_SIZE;
                    self->rx_state = ST_PAYLOAD_AND_FOOTER;
                    // todo: set timeout timer
                } else {
                    signal_error(self, self->rx_buffer, EMBC_ERROR_SYNCHRONIZATION);
                    framer_resync(self);
                    break;
                }
            }
            break;

        case ST_PAYLOAD_AND_FOOTER:
            --self->rx_remaining;
            if (self->rx_remaining == 0) {
                uint16_t length = 0;
                int32_t rc = embc_framer_validate(self->rx_buffer, self->rx_offset, &length);
                if (rc) {
                    signal_error(self, self->rx_buffer, rc);
                    framer_resync(self);
                } else {
                    signal_rx(self, self->rx_buffer);
                    self->rx_offset = 0;
                    self->rx_state = ST_SOF;
                }
            }
            break;
        default:
            break;
    }
}

void embc_framer_rx_buffer(struct embc_framer_s * self,
        uint8_t const * buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i) {
        embc_framer_rx_byte(self, buffer[i]);
    }
}

void embc_framer_rx_callback(
        struct embc_framer_s * self,
        embc_framer_rx_fn fn, void * user_data) {
    DBC_NOT_NULL(self);
    if (fn) {
        self->rx_fn = fn;
        self->rx_user_data = user_data;
    } else {
        self->rx_fn = rx_null;
        self->rx_user_data = 0;
    }
}

void embc_framer_error_callback(
        struct embc_framer_s * self,
        embc_framer_error_fn fn, void * user_data) {
    DBC_NOT_NULL(self);
    if (fn) {
        self->error_fn = fn;
        self->error_user_data = user_data;
    } else {
        self->error_fn = error_null;
        self->error_user_data = 0;
    }
}

#endif

void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * buffer, uint32_t length) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(buffer);
    DBC_RANGE_INT(length, 1, 256);
    uint32_t frame_crc = 0;

    struct embc_list_s * item = embc_list_remove_head(&self->tx_free);
    EMBC_ASSERT(item);
    struct embc_framer_tx_s * tx = embc_list_entry(item, struct embc_framer_tx_s, item);
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) tx->b;
    hdr->sof = SOF;
    hdr->frame_id = self->tx_frame_id;
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = ((length == 256) ? 0 : length) & 0xff;
    hdr->port_def0 = (uint8_t) (port_def & 0xff);
    hdr->port_def1 = (uint8_t) ((port_def >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, tx->b, 7);
    frame_crc = crc32(frame_crc, tx->b, 5);
#if 0
    frame_crc = crc_ccitt_16(frame_crc, buffer, length);
    self->tx_fn(self->tx_user_data, b, 5);
    self->tx_fn(self->tx_user_data, buffer, length);
    b[0] = frame_crc & 0xff;
    b[1] = (frame_crc >> 8) & 0xff;
    b[2] = SOF;
    self->tx_fn(self->tx_user_data, b, 3);
#endif
}
