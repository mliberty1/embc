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
#include "embc/memory/buffer.h"
#include "embc/bbuf.h"
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

struct embc_framer_s {
    struct embc_buffer_allocator_s * buffer_allocator;
    struct embc_framer_port_callbacks_s port_cbk[EMBC_FRAMER_PORTS];
    struct embc_framer_hal_callbacks_s hal_cbk;

    uint8_t rx_frame_id;
    struct embc_list_s rx_pending;      // of embc_buffer_s, for in-order delivery
    struct embc_buffer_s * rx_buffer;
    uint16_t rx_remaining;
    uint8_t rx_state;

    uint8_t tx_frame_id;
    struct embc_list_s tx_pending;       // of embc_buffer_s, for retransmission
    struct embc_list_s tx_buffer_queue;  // of embc_buffer_s, awaiting transmission
};

uint32_t embc_framer_instance_size(void) {
    return sizeof(struct embc_framer_s);
}

static void rx_default(void *user_data,
                       uint8_t port, uint8_t message_id, uint16_t port_def,
                       struct embc_buffer_s * buffer) {
    (void) user_data;
    (void) port;
    (void) message_id;
    (void) port_def;
    embc_buffer_free(buffer);
}

static void tx_done_default(
        void * user_data,
        uint8_t port,
        uint8_t message_id,
        int32_t status) {
    (void) user_data;
    (void) port;
    (void) message_id;
    (void) status;
}

void embc_framer_initialize(
        struct embc_framer_s * self,
        struct embc_buffer_allocator_s * buffer_allocator,
        struct embc_framer_hal_callbacks_s * callbacks) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(callbacks);
    DBC_NOT_NULL(callbacks->tx_fn);
    DBC_NOT_NULL(callbacks->timer_set_fn);
    DBC_NOT_NULL(callbacks->timer_cancel_fn);
    EMBC_STRUCT_PTR_INIT(self);
    self->buffer_allocator = buffer_allocator;
    self->hal_cbk = *callbacks;

    // initialize the RX messages
    embc_list_initialize(&self->rx_pending);
    self->rx_buffer = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);

    // initialize the TX messages
    embc_list_initialize(&self->tx_pending);
    embc_list_initialize(&self->tx_buffer_queue);

    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->port_cbk); ++i) {
        self->port_cbk[i].rx_fn = rx_default;
        self->port_cbk[i].tx_done_fn = tx_done_default;
    }
}

void embc_framer_register_port_callbacks(
        struct embc_framer_s * self,
        uint8_t port,
        struct embc_framer_port_callbacks_s * callbacks) {
    DBC_NOT_NULL(self);
    DBC_RANGE_INT(port, 0, EMBC_FRAMER_PORTS - 1);
    DBC_NOT_NULL(callbacks);
    DBC_NOT_NULL(callbacks->rx_fn);
    DBC_NOT_NULL(callbacks->tx_done_fn);
    self->port_cbk[port] = *callbacks;
    if (0 == callbacks->rx_fn) {
        self->port_cbk[port].rx_fn = rx_default;
    }
    if (0 == callbacks->tx_done_fn) {
        self->port_cbk[port].tx_done_fn = tx_done_default;
    }
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

static void signal_error(struct embc_framer_s * self, uint8_t * buffer, uint8_t error) {
    // todo
    (void) self;
    (void) buffer;
    (void) error;
    /*
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
     */
}

static void handle_rx(struct embc_framer_s * self, uint16_t offset) {
    struct embc_framer_header_s * hdr =
            (struct embc_framer_header_s *) (self->rx_buffer->data + offset);
    if (hdr->port >= EMBC_FRAMER_PORTS) {
        LOGF_WARN("invalid port %d", (int) hdr->port);
        return;
    }
    embc_size_t length = EMBC_FRAMER_HEADER_SIZE + hdr->length + EMBC_FRAMER_FOOTER_SIZE;
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
    self->rx_buffer->cursor = offset + EMBC_FRAMER_HEADER_SIZE;
    embc_buffer_copy(b, self->rx_buffer, hdr->length);
    // todo force in order delivery
    uint16_t port_def = (((uint16_t) hdr->port_def1) << 8) | hdr->port_def0;
    self->port_cbk[hdr->port].rx_fn(
            self->port_cbk[hdr->port].rx_user_data,
            hdr->port,
            hdr->message_id,
            port_def,
            b);
    embc_buffer_erase(self->rx_buffer, 0, length + offset);
}

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
    uint8_t const * crc_value = &buffer[sz - FOOTER_SIZE];
    uint32_t crc_rx = EMBC_BBUF_DECODE_U32_LE(crc_value);
    uint32_t crc_calc = crc32(0, buffer, sz - FOOTER_SIZE);
    if (crc_rx != crc_calc) {
        return EMBC_ERROR_MESSAGE_INTEGRITY;
    }
    return 0;
}

static void framer_resync(struct embc_framer_s * self) {
    uint16_t count = embc_buffer_length(self->rx_buffer);
    uint16_t sz = count;
    uint16_t i = 1;
    uint16_t length;
    self->rx_state = ST_SOF_UNSYNC;
    for (; i < count; ++i) {
        uint8_t * b = self->rx_buffer->data + i;
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
                handle_rx(self, i);
                count = embc_buffer_length(self->rx_buffer);
                sz = count;
                break;
            case EMBC_ERROR_IO:
                break;
            default:
                break;
        }
    }
exit:
    for (uint16_t j = 0; j < sz; ++j) {
        self->rx_buffer->data[j] = self->rx_buffer->data[i + j];
    }
    self->rx_buffer->length -= i;
    self->rx_buffer->cursor -= i;
}

void embc_framer_hal_rx_byte(struct embc_framer_s * self, uint8_t byte) {
    DBC_NOT_NULL(self);
    embc_buffer_write_u8(self->rx_buffer, byte);

    switch (self->rx_state) {
        case ST_SOF_UNSYNC: /* intentional fall-through. */
        case ST_SOF:
            if (byte == SOF) {
                embc_buffer_reset(self->rx_buffer);
                embc_buffer_write_u8(self->rx_buffer, SOF);
            } else {
                self->rx_remaining = HEADER_SIZE - 2;
                self->rx_state = ST_HEADER_UNSYNC | (self->rx_state & 0x80);
            }
            break;

        case ST_HEADER_UNSYNC: /* intentional fall-through. */
        case ST_HEADER:
            --self->rx_remaining;
            if (self->rx_remaining == 0) {
                if (is_header_valid(self->rx_buffer->data)) {
                    self->rx_remaining = frame_payload_length(self->rx_buffer->data)
                                         + FOOTER_SIZE;
                    self->rx_state = ST_PAYLOAD_AND_FOOTER;
                    // todo: set timeout timer
                } else {
                    signal_error(self, self->rx_buffer->data, EMBC_ERROR_SYNCHRONIZATION);
                    framer_resync(self);
                    break;
                }
            }
            break;

        case ST_PAYLOAD_AND_FOOTER:
            --self->rx_remaining;
            if (self->rx_remaining == 0) {
                uint16_t length = embc_buffer_length(self->rx_buffer);
                int32_t rc = embc_framer_validate(self->rx_buffer->data, length, &length);
                if (rc) {
                    signal_error(self, self->rx_buffer->data, rc);
                    framer_resync(self);
                } else {
                    handle_rx(self, 0);
                }
            }
            break;
        default:
            break;
    }
}

void embc_framer_hal_rx_buffer(struct embc_framer_s * self,
        uint8_t const * buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i) {
        embc_framer_hal_rx_byte(self, buffer[i]);
    }
}

void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        struct embc_buffer_s * buffer) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(buffer);
    uint32_t frame_crc = 0;
    embc_size_t length = embc_buffer_length(buffer);

    if (buffer->reserve == 0) {
        // copy necessary, leave room for header & footer
        EMBC_ASSERT((buffer->reserve == 0) && (length < EMBC_FRAMER_PAYLOAD_MAX_SIZE));
        struct embc_buffer_s * b = embc_framer_alloc(self);
        embc_buffer_write(b, buffer->data, length);
        embc_buffer_free(buffer);
        buffer = b;
        length = embc_buffer_length(buffer);
    }
    EMBC_ASSERT(buffer->reserve == EMBC_FRAMER_FOOTER_SIZE);

    buffer->reserve = 0;
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) buffer->data;

    hdr->sof = SOF;
    hdr->frame_id = self->tx_frame_id;
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = (uint8_t) ((length - HEADER_SIZE) & 0xff);
    hdr->port_def0 = (uint8_t) (port_def & 0xff);
    hdr->port_def1 = (uint8_t) ((port_def >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, buffer->data, HEADER_SIZE - 1);
    frame_crc = crc32(frame_crc, buffer->data, length);
    embc_buffer_write_u32_le(buffer, frame_crc);
    embc_buffer_write_u8(buffer, SOF);
    self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, buffer);
}

struct embc_buffer_s * embc_framer_alloc(
        struct embc_framer_s * self) {
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
    b->cursor = EMBC_FRAMER_HEADER_SIZE;
    b->length = EMBC_FRAMER_HEADER_SIZE;
    b->reserve = EMBC_FRAMER_FOOTER_SIZE;
    return b;
}
