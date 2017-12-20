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
#define BITMAP_CURRENT ((uint16_t) 0x100)

EMBC_STATIC_ASSERT(EMBC_FRAMER_FRAME_MAX_SIZE < 256, frame_size_too_big);

enum embc_framer_state_e {
    ST_SOF_UNSYNC = 0x00,
    ST_HEADER_UNSYNC = 0x01,
    ST_SOF = 0x80,
    ST_HEADER = 0x81,
    ST_PAYLOAD_AND_FOOTER = 0x82,
};

enum tx_status_e {
    TX_STATUS_EMPTY = 0,
    TX_STATUS_TRANSMITTING = 1,
    TX_STATUS_AWAIT_ACK = 2,
    TX_STATUS_ACKED = 3,
};

struct tx_buf_s {
    struct embc_buffer_s * b;
    struct embc_list_s item;
    uint8_t status;
};

struct embc_framer_s {
    struct embc_buffer_allocator_s * buffer_allocator;
    struct embc_framer_port_callbacks_s port_cbk[EMBC_FRAMER_PORTS];
    struct embc_framer_hal_callbacks_s hal_cbk;
    struct embc_framer_status_s status;

    uint8_t rx_frame_id;
    uint16_t rx_frame_bitmask;
    uint16_t rx_frame_bitmask_outstanding;
    struct embc_list_s rx_pending;      // of embc_buffer_s, for in-order delivery
    struct embc_buffer_s * rx_buffer;
    uint16_t rx_remaining;
    uint8_t rx_state;
    embc_framer_rx_hook_fn rx_hook_fn;
    void * rx_hook_user_data;

    uint8_t tx_frame_id;
    struct tx_buf_s tx_buffers[EMBC_FRAMER_OUTSTANDING_FRAMES_MAX];
    struct embc_list_s tx_buffers_free;    // of embc_buffer_s, for retransmission
    struct embc_list_s tx_buffers_active;  // of embc_buffer_s, for retransmission
    struct embc_list_s tx_queue;           // of embc_buffer_s, awaiting transmission
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

static void rx_hook_fn(
        void * user_data,
        struct embc_buffer_s * frame);

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
    embc_memset(self, 0, sizeof(*self));
    self->buffer_allocator = buffer_allocator;
    self->hal_cbk = *callbacks;

    for (int i = 0; i < EMBC_FRAMER_OUTSTANDING_FRAMES_MAX; ++i) {
        self->rx_frame_bitmask_outstanding = (BITMAP_CURRENT | self->rx_frame_bitmask_outstanding) >> 1;
    }

    // initialize the RX messages
    embc_list_initialize(&self->rx_pending);
    self->rx_buffer = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
    self->rx_hook_fn = rx_hook_fn;
    self->rx_hook_user_data = self;

    // initialize the TX messages
    embc_list_initialize(&self->tx_buffers_free);
    embc_list_initialize(&self->tx_buffers_active);
    embc_list_initialize(&self->tx_queue);
    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->tx_buffers); ++i) {
        embc_list_add_tail(&self->tx_buffers_free, &self->tx_buffers[i].item);
    }

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

void embc_framer_register_rx_hook(
        struct embc_framer_s * self,
        embc_framer_rx_hook_fn rx_fn,
        void * rx_user_data) {
    DBC_NOT_NULL(self);
    if (0 == rx_fn) {
        self->rx_hook_fn = rx_hook_fn;
        self->rx_hook_user_data = self;
    } else {
        self->rx_hook_fn = rx_fn;
        self->rx_hook_user_data = rx_user_data;
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

static inline uint8_t frame_payload_length(uint8_t const * buffer) {
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) buffer;
    return hdr->length;
}

static void rx_insert(struct embc_framer_s *self, struct embc_buffer_s * frame) {
    struct embc_list_s * item;
    struct embc_framer_header_s * h = (struct embc_framer_header_s *) frame->data;
    embc_list_foreach(&self->rx_pending, item) {
        struct embc_buffer_s * f = embc_list_entry(item, struct embc_buffer_s, item);
        struct embc_framer_header_s * h1 = (struct embc_framer_header_s *) f->data;
        uint8_t delta = h->frame_id - h1->frame_id;
        if (0 == delta) { // duplicate
            return;
        } else if (delta > 128) { // before current item
            embc_list_insert_before(item, &frame->item);
            return;
        }
    }
    embc_list_add_tail(&self->rx_pending, &frame->item);
}

static void rx_complete(struct embc_framer_s *self, struct embc_buffer_s * frame) {
    frame->cursor = EMBC_FRAMER_HEADER_SIZE;
    frame->length -= EMBC_FRAMER_FOOTER_SIZE;
    struct embc_framer_header_s *hdr = (struct embc_framer_header_s *) (frame->data);
    uint16_t port_def = (((uint16_t) hdr->port_def1) << 8) | hdr->port_def0;
    self->port_cbk[hdr->port].rx_fn(
            self->port_cbk[hdr->port].rx_user_data,
            hdr->port,
            hdr->message_id,
            port_def,
            frame);
}

static void rx_complete_queued(struct embc_framer_s *self) {
    if ((self->rx_frame_bitmask & self->rx_frame_bitmask_outstanding) == self->rx_frame_bitmask_outstanding) {
        struct embc_list_s *item;
        struct embc_buffer_s *b;
        embc_list_foreach(&self->rx_pending, item) {
            b = embc_list_entry(item, struct embc_buffer_s, item);
            embc_list_remove(&b->item);
            rx_complete(self, b);
        }
    }
}

static void rx_purge_queued(struct embc_framer_s *self) {
    struct embc_list_s *item;
    struct embc_buffer_s *b;
    embc_list_foreach(&self->rx_pending, item) {
        b = embc_list_entry(item, struct embc_buffer_s, item);
        embc_list_remove(&b->item);
        embc_buffer_free(b);
    }
}

struct embc_buffer_s * embc_framer_construct_ack_to_frame(
        struct embc_framer_s *self,
        struct embc_buffer_s * data_frame,
        uint16_t mask,
        uint8_t status) {
    struct embc_framer_header_s *hdr = (struct embc_framer_header_s *) (data_frame->data);
    uint8_t frame_id = hdr->frame_id & 0x0f;

    if (0 == mask) {
        uint8_t frame_delta_new = frame_id - self->rx_frame_id;
        uint8_t frame_delta_old = self->rx_frame_id - frame_id;
        if (frame_id == self->rx_frame_id) {
            mask = self->rx_frame_bitmask;
        } else if (frame_delta_new < EMBC_FRAMER_OUTSTANDING_FRAMES_MAX) {
            mask = self->rx_frame_bitmask >> frame_delta_new;
        } else if (frame_delta_old <= EMBC_FRAMER_OUTSTANDING_FRAMES_MAX) {
            mask = self->rx_frame_bitmask << frame_delta_old;
        }
        mask |= BITMAP_CURRENT;
    }

    struct embc_buffer_s * ack = embc_framer_construct_ack(
            self, frame_id, hdr->port, hdr->message_id, mask,status);
    return ack;
}

static inline struct embc_buffer_s * construct_nack(
        struct embc_framer_s *self,
        uint8_t status) {
    return embc_framer_construct_ack(self, 0, 0, 0, 0, status);
}

static inline void send_frame_ack(
        struct embc_framer_s *self,
        struct embc_buffer_s * data_frame,
        uint16_t mask,
        uint8_t status) {
    struct embc_buffer_s * ack = embc_framer_construct_ack_to_frame(self, data_frame, mask, status);
    self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, ack);
}

static void handle_ack(struct embc_framer_s *self, struct embc_buffer_s * ack) {
    struct embc_framer_header_s *hdr = (struct embc_framer_header_s *) (ack->data);
    if (1 != hdr->length) {
        LOGF_WARN("ack invalid length: %d", (int) hdr->length);
        return;
    }
    uint8_t status = ack->data[EMBC_FRAMER_HEADER_SIZE];
    uint16_t port_def = (((uint16_t) hdr->port_def1) << 8) | hdr->port_def0;
    if (0 == status) {
        // success!
        if ((hdr->port <= 0) || (hdr->port >= EMBC_FRAMER_PORTS)) {
            LOGF_WARN("ack invalid port: %d", (int) hdr->port);
            return;
        }
        struct embc_list_s * item;
        embc_list_foreach(&self->tx_buffers_active, item) {
            struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
            embc_list_remove(&t->item);
            embc_list_add_tail(&self->tx_buffers_free, item);
            self->port_cbk[hdr->port].tx_done_fn(
                    self->port_cbk[hdr->port].tx_done_user_data,
                    hdr->port,
                    hdr->message_id,
                    status
            );
            embc_buffer_free(t->b);
            t->status = TX_STATUS_EMPTY;
        }
    } else if (0 == port_def) {
        // general nack (not for a specific frame)
    } else {
        // specific nack for a frame (usually message integrity error).
    }
}

static void rx_hook_fn(
        void * user_data,
        struct embc_buffer_s * frame) {
    struct embc_framer_s *self = (struct embc_framer_s *) user_data;
    struct embc_framer_header_s *hdr = (struct embc_framer_header_s *) (frame->data);
    if (hdr->port >= EMBC_FRAMER_PORTS) {
                LOGF_WARN("invalid port %d", (int) hdr->port);
        return;
    }
    EMBC_ASSERT(frame->length == (hdr->length + EMBC_FRAMER_HEADER_SIZE + EMBC_FRAMER_FOOTER_SIZE));

    uint8_t frame_id = hdr->frame_id & 0x0f;
    uint16_t ack_frame_bitmask = 0;

    if (hdr->frame_id & 0x80) { // ack frame
        handle_ack(self, frame);
        return;
    }

    if (frame_id == self->rx_frame_id) {
        // expected frame!
        ack_frame_bitmask = BITMAP_CURRENT | self->rx_frame_bitmask;
        ++self->rx_frame_id;
        self->rx_frame_bitmask = (BITMAP_CURRENT | self->rx_frame_bitmask) >> 1;
        embc_list_add_tail(&self->rx_pending, &frame->item);
    } else {
        uint8_t frame_delta_new = frame_id - self->rx_frame_id;
        uint8_t frame_delta_old = self->rx_frame_id - frame_id;

        if (frame_delta_new < EMBC_FRAMER_OUTSTANDING_FRAMES_MAX) {
            // skipped frame(s), store this frame.
            // will receive skipped frames on retransmit
            ack_frame_bitmask = BITMAP_CURRENT | (self->rx_frame_bitmask >> frame_delta_new);
            self->rx_frame_id += frame_delta_new + 1;
            self->rx_frame_bitmask = (ack_frame_bitmask >> 1);
            embc_list_add_tail(&self->rx_pending, &frame->item);
        } else if (frame_delta_old <= EMBC_FRAMER_OUTSTANDING_FRAMES_MAX) {
            // older frame, dedup as necessary
            uint16_t mask = BITMAP_CURRENT >> frame_delta_old;
            if (self->rx_frame_bitmask & mask) {
                // duplicate, discard
                embc_buffer_free(frame);
                ++self->status.rx_deduplicate_count;
            } else {
                self->rx_frame_bitmask |= mask;
                rx_insert(self, frame);
            }
            ack_frame_bitmask = self->rx_frame_bitmask << frame_delta_old;
        } else {
            // completely out of order: assume resync required.
            rx_purge_queued(self);
            ack_frame_bitmask = BITMAP_CURRENT;
            self->rx_frame_id = frame_id + 1;
            self->rx_frame_bitmask = BITMAP_CURRENT >> 1;
            embc_list_add_tail(&self->rx_pending, &frame->item);
            ++self->status.rx_frame_id_error;
        }
    }
    rx_complete_queued(self);
    send_frame_ack(self, frame, ack_frame_bitmask, 0);
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

    uint8_t payload_length = frame_payload_length(buffer);
    if (payload_length > EMBC_FRAMER_PAYLOAD_MAX_SIZE) {
        return EMBC_ERROR_TOO_BIG;
    }
    uint16_t sz = payload_length + HEADER_SIZE + FOOTER_SIZE;
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

static void framer_resync(struct embc_framer_s * self, int32_t status) {
    (void) status;
    uint16_t count = embc_buffer_length(self->rx_buffer);
    uint16_t sz = count;
    uint16_t i = 1;
    uint16_t length;
    if (0 == (self->rx_state & 0x80)) {
        // not synchronized, no additional error
    } else {
        // create ack
        if (EMBC_ERROR_MESSAGE_INTEGRITY == status) {
            ++self->status.rx_mic_error;
            send_frame_ack(self, self->rx_buffer, 0, status);
        } else {
            ++self->status.rx_synchronization_error;
            struct embc_buffer_s * ack = construct_nack(self, status);
            self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, ack);
        }
    }
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
                break;  // consume & keep trying
            case EMBC_SUCCESS: {
                self->rx_state = ST_SOF;
                // rx_buffer may contain more data than just the frame, copy
                struct embc_buffer_s * f = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
                embc_buffer_write(f, b, length);
                embc_buffer_cursor_set(f, 0);
                ++self->status.rx_count;
                self->rx_hook_fn(self->rx_hook_user_data, f);
                embc_buffer_erase(self->rx_buffer, 0, i + length);
                i = 0;
                sz = embc_buffer_length(self->rx_buffer);

                break;
            }
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
                embc_buffer_write_u8(self->rx_buffer, SOF); // rewrite
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
                    uint8_t length = frame_payload_length(self->rx_buffer->data);
                    if (length > EMBC_FRAMER_PAYLOAD_MAX_SIZE) {
                        framer_resync(self, EMBC_ERROR_TOO_BIG);
                    } else {
                        self->rx_remaining = length + FOOTER_SIZE;
                        self->rx_state = ST_PAYLOAD_AND_FOOTER;
                    }
                } else {
                    framer_resync(self, EMBC_ERROR_SYNCHRONIZATION);
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
                    framer_resync(self, rc);
                } else {
                    ++self->status.rx_count;
                    embc_buffer_cursor_set(self->rx_buffer, 0);
                    self->rx_hook_fn(self->rx_hook_user_data, self->rx_buffer);
                    self->rx_buffer = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
                    self->rx_state = ST_SOF;
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

void embc_framer_hal_tx_done(
        struct embc_framer_s * self,
        struct embc_buffer_s * buffer) {
    struct embc_list_s * item;
    embc_list_foreach(&self->tx_buffers_active, item) {
        struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
        if (t->b == buffer) {
            t->status = TX_STATUS_AWAIT_ACK;
        }
    }
}

struct embc_buffer_s * embc_framer_construct_frame(
        struct embc_framer_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const *payload, uint8_t length) {
    DBC_RANGE_INT(port, 1, (EMBC_FRAMER_PORTS - 1));
    uint32_t frame_crc = 0;
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, length + HEADER_SIZE + FOOTER_SIZE);
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) b->data;
    hdr->sof = SOF;
    hdr->frame_id = frame_id & 0x0f;
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = length;
    hdr->port_def0 = (uint8_t) (port_def & 0xff);
    hdr->port_def1 = (uint8_t) ((port_def >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, b->data, HEADER_SIZE - 1);
    b->cursor = HEADER_SIZE;
    b->length = HEADER_SIZE;
    embc_buffer_write(b, payload, length);
    frame_crc = crc32(frame_crc, b->data, b->length);
    embc_buffer_write_u32_le(b, frame_crc);
    embc_buffer_write_u8(b, SOF);
    return b;
}

struct embc_buffer_s * embc_framer_construct_ack(
        struct embc_framer_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t ack_mask,
        uint8_t status) {
    uint32_t frame_crc = 0;
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, 1 + HEADER_SIZE + FOOTER_SIZE);
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) b->data;
    hdr->sof = SOF;
    hdr->frame_id = 0x80 | (frame_id & 0x0f);
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = 1;
    hdr->port_def0 = (uint8_t) (ack_mask & 0xff);
    hdr->port_def1 = (uint8_t) ((ack_mask >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, b->data, HEADER_SIZE - 1);
    b->cursor = HEADER_SIZE;
    b->length = HEADER_SIZE;
    embc_buffer_write_u8(b, status);
    frame_crc = crc32(frame_crc, b->data, b->length);
    embc_buffer_write_u32_le(b, frame_crc);
    embc_buffer_write_u8(b, SOF);
    return b;
}

void embc_framer_send(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        struct embc_buffer_s * buffer) {
    DBC_NOT_NULL(self);
    DBC_GT_ZERO(port);
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
    hdr->frame_id = (self->tx_frame_id)++;
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = (uint8_t) ((length - HEADER_SIZE) & 0xff);
    hdr->port_def0 = (uint8_t) (port_def & 0xff);
    hdr->port_def1 = (uint8_t) ((port_def >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, buffer->data, HEADER_SIZE - 1);
    frame_crc = crc32(frame_crc, buffer->data, length);
    embc_buffer_write_u32_le(buffer, frame_crc);
    embc_buffer_write_u8(buffer, SOF);

    if (embc_list_is_empty(&self->tx_buffers_free)) {
        embc_list_add_tail(&self->tx_queue, &buffer->item);
    } else {
        struct embc_list_s * item = embc_list_remove_head(&self->tx_buffers_free);
        struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
        embc_list_add_tail(&self->tx_buffers_active, item);
        t->b = buffer;
        t->status = TX_STATUS_TRANSMITTING;
        self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, t->b);
    }
}

EMBC_API void embc_framer_send_payload(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * data, uint8_t length) {
    DBC_RANGE_INT(length, 0, EMBC_FRAMER_PAYLOAD_MAX_SIZE - 1);
    struct embc_buffer_s * b = embc_framer_alloc(self);
    embc_buffer_write(b, data, length);
    embc_framer_send(self, port, message_id, port_def, b);
}

struct embc_buffer_s * embc_framer_alloc(
        struct embc_framer_s * self) {
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
    b->cursor = EMBC_FRAMER_HEADER_SIZE;
    b->length = EMBC_FRAMER_HEADER_SIZE;
    b->reserve = EMBC_FRAMER_FOOTER_SIZE;
    return b;
}

struct embc_framer_status_s embc_framer_status_get(
        struct embc_framer_s * self) {
    return self->status;
}