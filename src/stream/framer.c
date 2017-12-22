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

//#define LOG_LEVEL LOG_LEVEL_ALL
#include "embc/stream/framer.h"
#include "embc/stream/framer_util.h"
#include "embc/collections/list.h"
#include "embc/memory/buffer.h"
#include "embc/bbuf.h"
#include "embc.h"
#include "embc/crc.h"
#include "embc/time.h"

#define SOF ((uint8_t) EMBC_FRAMER_SOF)
#define HEADER_SIZE ((uint16_t) EMBC_FRAMER_HEADER_SIZE)
#define FOOTER_SIZE ((uint16_t) EMBC_FRAMER_FOOTER_SIZE)
#define BITMAP_CURRENT ((uint16_t) 0x100)
#define EMBC_FRAMER_TIMEOUT EMBC_MILLISECONDS_TO_TIME(100)

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
    uint8_t retries;
};

static inline uint16_t hdr_port_def(struct embc_framer_header_s * hdr) {
    return ((((uint16_t) hdr->port_def1) & 0xff) << 8) |
            (((uint16_t) hdr->port_def0) & 0xff);
}

static void transmit_tx_buf(struct embc_framer_s * self, struct tx_buf_s * t);

struct embc_framer_s {
    struct embc_buffer_allocator_s * buffer_allocator;
    struct embc_framer_port_callbacks_s port_cbk[EMBC_FRAMER_PORTS];
    struct embc_framer_port_callbacks_s port0_cbk;
    struct embc_framer_hal_callbacks_s hal_cbk;
    struct embc_framer_status_s status;

    uint32_t timer_id;
    uint8_t rx_frame_id;
    uint16_t rx_frame_bitmask;
    uint16_t rx_frame_bitmask_outstanding;
    struct embc_list_s rx_pending;      // of embc_buffer_s, for in-order delivery
    struct embc_buffer_s * rx_buffer;
    uint16_t rx_remaining;
    uint8_t rx_state;
    embc_framer_rx_hook_fn rx_hook_fn;
    void * rx_hook_user_data;

    uint8_t tx_next_frame_id;
    struct tx_buf_s tx_buffers[EMBC_FRAMER_OUTSTANDING_FRAMES_MAX];
    struct embc_list_s tx_buffers_free;    // of embc_buffer_s, for retransmission
    struct embc_list_s tx_buffers_active;  // of embc_buffer_s, for retransmission
    struct embc_list_s tx_queue;           // of embc_buffer_s, awaiting transmission
};

embc_size_t embc_framer_instance_size(void) {
    return (embc_size_t) sizeof(struct embc_framer_s);
}

static void rx_default(void *user_data,
                       uint8_t port, uint8_t message_id, uint16_t port_def,
                       struct embc_buffer_s * buffer) {
    (void) user_data;
    (void) port;
    (void) message_id;
    (void) port_def;
    LOGF_DEBUG2("rx_default free %p", (void *) buffer);
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

static inline struct embc_framer_header_s * buffer_hdr(struct embc_buffer_s * buffer) {
    return (struct embc_framer_header_s *) (buffer->data);
}

static void port0_rx(void *user_data,
                     uint8_t port, uint8_t message_id, uint16_t port_def,
                     struct embc_buffer_s * buffer) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    EMBC_ASSERT(0 == port);
    uint8_t cmd = (uint8_t) (port_def & 0xff);
    struct embc_framer_header_s * hdr = buffer_hdr(buffer);
    switch (cmd) {
        case EMBC_FRAMER_PORT0_PING_REQ:
            LOGF_DEBUG2("port0_rx ping_req: frame_id=0x%02x, message_id=%d",
                      (int) hdr->frame_id, (int) hdr->message_id);
            embc_framer_send(self, port, message_id, EMBC_FRAMER_PORT0_PING_RSP, buffer);
            break;
        case EMBC_FRAMER_PORT0_STATUS_REQ:
            embc_buffer_write(buffer, &self->status, sizeof(self->status));
            embc_framer_send(self, port, message_id, EMBC_FRAMER_PORT0_STATUS_RSP, buffer);
            break;
        default:
            self->port0_cbk.rx_fn(self->port0_cbk.rx_user_data, port, message_id, port_def, buffer);
            break;
    }
}

static void port0_tx_done(
        void * user_data,
        uint8_t port,
        uint8_t message_id,
        int32_t status) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    (void) self;
    EMBC_ASSERT(0 == port);
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

    LOGS_DEBUG3("initialize the RX messages");
    embc_list_initialize(&self->rx_pending);
    self->rx_buffer = embc_buffer_alloc(self->buffer_allocator, EMBC_FRAMER_FRAME_MAX_SIZE);
    self->rx_hook_fn = rx_hook_fn;
    self->rx_hook_user_data = self;

    LOGS_DEBUG3("initialize the TX messages");
    embc_list_initialize(&self->tx_buffers_free);
    embc_list_initialize(&self->tx_buffers_active);
    embc_list_initialize(&self->tx_queue);
    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->tx_buffers); ++i) {
        embc_list_add_tail(&self->tx_buffers_free, &self->tx_buffers[i].item);
    }

    self->port0_cbk.rx_fn = rx_default;
    self->port0_cbk.tx_done_fn = tx_done_default;
    self->port_cbk[0].rx_fn = port0_rx;
    self->port_cbk[0].rx_user_data = self;
    self->port_cbk[0].tx_done_fn = port0_tx_done;
    self->port_cbk[0].tx_done_user_data = self;
    for (embc_size_t i = 1; i < EMBC_ARRAY_SIZE(self->port_cbk); ++i) {
        self->port_cbk[i].rx_fn = rx_default;
        self->port_cbk[i].tx_done_fn = tx_done_default;
    }
}

void embc_framer_register_port_callbacks(
        struct embc_framer_s * self,
        uint8_t port,
        struct embc_framer_port_callbacks_s const * callbacks) {
    DBC_NOT_NULL(self);
    DBC_RANGE_INT(port, 0, EMBC_FRAMER_PORTS - 1);
    DBC_NOT_NULL(callbacks);
    struct embc_framer_port_callbacks_s * c;

    if (0 == port) {
        self->port0_cbk = *callbacks;
        c = &self->port0_cbk;
    } else {
        self->port_cbk[port] = *callbacks;
        c = &self->port_cbk[port];
    }

    if (0 == c->rx_fn) {
        c->rx_fn = rx_default;
    }
    if (0 == c->tx_done_fn) {
        c->tx_done_fn = tx_done_default;
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

static inline uint8_t frame_payload_length(uint8_t const * buffer) {
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) buffer;
    return hdr->length;
}

static void rx_insert(struct embc_framer_s *self, struct embc_buffer_s * frame) {
    struct embc_list_s * item;
    struct embc_framer_header_s * h = buffer_hdr(frame);
    embc_list_foreach(&self->rx_pending, item) {
        struct embc_buffer_s * f = embc_list_entry(item, struct embc_buffer_s, item);
        struct embc_framer_header_s * h1 = buffer_hdr(f);
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
    frame->reserve = EMBC_FRAMER_FOOTER_SIZE;  // to allow for loopback
    struct embc_framer_header_s *hdr = buffer_hdr(frame);
    ++self->status.rx_data_count;
    uint16_t port_def = hdr_port_def(hdr);
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
        LOGF_DEBUG2("rx_purge_queued %p", (void *) b);
        embc_buffer_free(b);
    }
}

struct embc_buffer_s * embc_framer_construct_ack_to_frame(
        struct embc_framer_s *self,
        struct embc_buffer_s * data_frame,
        uint16_t mask,
        uint8_t status) {
    struct embc_framer_header_s *hdr = buffer_hdr(data_frame);
    uint8_t frame_id = hdr->frame_id & EMBC_FRAMER_ID_MASK;

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
    struct embc_framer_header_s *hdr = buffer_hdr(data_frame);
    LOGF_DEBUG1("send_frame_ack frame_id=0x%02x status=%d",
              (int) hdr->frame_id, (int) status);
    struct embc_buffer_s * ack = embc_framer_construct_ack_to_frame(self, data_frame, mask, status);
    self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, ack);
}

static uint8_t expected_tx_frame_id(struct embc_framer_s * self) {
    struct embc_list_s * item;
    if (embc_list_is_empty(&self->tx_buffers_active)) {
        return 255;
    }
    item = embc_list_peek_head(&self->tx_buffers_active);
    struct tx_buf_s *t = embc_list_entry(item, struct tx_buf_s, item);
    struct embc_framer_header_s *hdr = buffer_hdr(t->b);
    return hdr->frame_id;
}

static struct tx_buf_s * find_tx(struct embc_framer_s * self, uint8_t frame_id) {
    struct embc_list_s * item;
    embc_list_foreach(&self->tx_buffers_active, item) {
        struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
        struct embc_framer_header_s *hdr = buffer_hdr(t->b);
        if (hdr->frame_id == frame_id) {
            return t;
        }
    }
    return 0;
}

static void tx_complete(struct embc_framer_s * self, struct tx_buf_s * t, uint8_t status) {
    struct embc_framer_header_s *hdr = buffer_hdr(t->b);
    ++self->status.tx_count;
    embc_list_remove(&t->item);
    embc_list_add_tail(&self->tx_buffers_free, &t->item);
    if (hdr->port < EMBC_FRAMER_PORTS) {
        self->port_cbk[hdr->port].tx_done_fn(
                self->port_cbk[hdr->port].tx_done_user_data,
                hdr->port,
                hdr->message_id,
                status
        );
    } else {
        LOGF_WARN("ack invalid port: %d", (int) hdr->port);
    }
    LOGF_DEBUG2("tx_complete %p", (void *) t->b);
    embc_buffer_free(t->b);
    t->b = 0;
    t->status = TX_STATUS_EMPTY;
}

static void tx_error(struct embc_framer_s * self, struct tx_buf_s * t, uint8_t status) {
    if (t->retries >= EMBC_FRAMER_MAX_RETRIES) {
        tx_complete(self, t, status);
    } else {
        ++t->retries;
        ++self->status.tx_retransmit_count;
        transmit_tx_buf(self, t);
    }
}

static void tx_complete_in_order(struct embc_framer_s * self, struct tx_buf_s * t_current) {
    // Complete only when guaranteed in order!
    bool match_current = false;
    struct embc_list_s * item;
    uint8_t tx_ack_frame_id = expected_tx_frame_id(self);
    embc_list_foreach(&self->tx_buffers_active, item) {
        struct tx_buf_s *t = embc_list_entry(item, struct tx_buf_s, item);
        match_current |= (t == t_current);
        struct embc_framer_header_s *hdr = buffer_hdr(t->b);
        if ((TX_STATUS_ACKED == t->status) && (hdr->frame_id == tx_ack_frame_id)) {
            tx_complete(self, t, 0);
        } else if (!match_current && (t->status == TX_STATUS_AWAIT_ACK)) {
            ++self->status.tx_retransmit_count;
            transmit_tx_buf(self, t);
            break;
        } else {
            break;
        }
        tx_ack_frame_id = (tx_ack_frame_id + 1) & EMBC_FRAMER_ID_MASK;
    }
}

static void timer_callback(void * user_data, uint32_t timer_id) {
    struct embc_framer_s * self = (struct embc_framer_s *) user_data;
    (void) timer_id;
    struct embc_list_s * item;
    struct tx_buf_s * t;
    embc_list_foreach(&self->tx_buffers_active, item) {
        t = embc_list_entry(item, struct tx_buf_s, item);
        if (t->status == TX_STATUS_AWAIT_ACK) {
            tx_error(self, t, EMBC_ERROR_TIMED_OUT);
        }
    }
}

static void timer_start(struct embc_framer_s * self) {
    self->hal_cbk.timer_set_fn(
            self->hal_cbk.timer_set_user_data,
            EMBC_FRAMER_TIMEOUT,
            timer_callback, self,
            &self->timer_id);
}

static void timer_cancel(struct embc_framer_s * self) {
    self->hal_cbk.timer_cancel_fn(
            self->hal_cbk.timer_cancel_user_data,
            self->timer_id);
}

static void transmit_tx_buf(struct embc_framer_s * self, struct tx_buf_s * t) {
    timer_cancel(self);
    struct embc_framer_header_s * hdr = buffer_hdr(t->b);
    LOGF_DEBUG2("tx 0x%02x, 0x%04x", hdr->frame_id, hdr_port_def(hdr));
    t->status = TX_STATUS_TRANSMITTING;
    self->hal_cbk.tx_fn(self->hal_cbk.tx_user_data, t->b);
    timer_start(self);
}

static void transmit_queued(struct embc_framer_s * self) {
    while (!embc_list_is_empty(&self->tx_buffers_free) && !embc_list_is_empty(&self->tx_queue)) {
        struct embc_list_s * item =  embc_list_remove_head(&self->tx_queue);
        struct embc_buffer_s * b = embc_list_entry(item, struct embc_buffer_s, item);
        item = embc_list_remove_head(&self->tx_buffers_free);
        struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
        embc_list_add_tail(&self->tx_buffers_active, item);
        t->b = b;
        t->retries = 0;
        transmit_tx_buf(self, t);
    }
}

static void tx_confirmed(struct embc_framer_s * self, struct embc_framer_header_s *ack_hdr) {
    uint8_t frame_id = ack_hdr->frame_id & EMBC_FRAMER_ID_MASK;
    struct tx_buf_s * t_match = find_tx(self, frame_id);
    if (0 == t_match) {
        LOGF_WARN("ack for unknown frame_id %d", (int) frame_id);
        return;
    }

    // update current item status.
    t_match->status = TX_STATUS_ACKED;
    uint16_t mask = hdr_port_def(ack_hdr);

    // update status based upon mask.
    struct embc_list_s * item = &t_match->item;
    while (item->prev != &self->tx_buffers_active) {
        item = item->prev;
        mask = mask << 1;
        struct tx_buf_s * t = embc_list_entry(item, struct tx_buf_s, item);
        struct embc_framer_header_s *hdr = buffer_hdr(t->b);
        frame_id = (frame_id - 1) & EMBC_FRAMER_ID_MASK;
        EMBC_ASSERT(frame_id == hdr->frame_id);
        if ((TX_STATUS_AWAIT_ACK == t->status) && (mask & EMBC_FRAMER_ACK_MASK_CURRENT)) {
            t->status = TX_STATUS_ACKED;
        }
    }

    tx_complete_in_order(self, t_match);
    transmit_queued(self);
    if (embc_list_is_empty(&self->tx_buffers_active)) {
        timer_cancel(self);
    }
}

static void handle_ack(struct embc_framer_s *self, struct embc_buffer_s * ack) {
    struct embc_framer_header_s *hdr = buffer_hdr(ack);
    if (1 != hdr->length) {
        LOGF_WARN("ack invalid length: %d", (int) hdr->length);
        return;
    }
    uint8_t status = ack->data[EMBC_FRAMER_HEADER_SIZE];
    uint16_t port_def = hdr_port_def(hdr);
    if (0 == status) { // success!
        tx_confirmed(self, hdr);
    } else if (0 == port_def) {
        // general nack (not for a specific frame)
    } else {
        // specific nack for a frame (usually message integrity error).
        uint8_t frame_id = hdr->frame_id & EMBC_FRAMER_ID_MASK;
        struct tx_buf_s * t = find_tx(self, frame_id);
        if (t) {
            tx_error(self, t, EMBC_ERROR_MESSAGE_INTEGRITY);
        } else {
            LOGF_WARN("NACK: frame_id %d not found", (int) frame_id);
        }
    }
}

static void rx_hook_fn(
        void * user_data,
        struct embc_buffer_s * frame) {
    DBC_NOT_NULL(user_data);
    DBC_NOT_NULL(frame);
    DBC_NOT_NULL(frame->data);
    struct embc_framer_s *self = (struct embc_framer_s *) user_data;
    struct embc_framer_header_s *hdr = buffer_hdr(frame);
    if (hdr->port >= EMBC_FRAMER_PORTS) {
        LOGF_WARN("invalid port %d", (int) hdr->port);
        return;
    }
    EMBC_ASSERT(frame->length == (hdr->length + EMBC_FRAMER_HEADER_SIZE + EMBC_FRAMER_FOOTER_SIZE));

    uint8_t frame_id = hdr->frame_id & EMBC_FRAMER_ID_MASK;
    uint16_t ack_frame_bitmask = 0;
    bool discard = false;

    if (EMBC_FRAMER_TYPE_ACK == (hdr->frame_id & EMBC_FRAMER_TYPE_MASK)) { // ack frame
        LOGF_DEBUG3("rx_hook_fn ack  0x%02x 0x%04x %p",
                    (int) hdr->frame_id, (int) hdr_port_def(hdr), (void *) frame);
        ++self->status.rx_ack_count;
        handle_ack(self, frame);
        embc_buffer_free(frame);
        return;
    }

    LOGF_DEBUG3("rx_hook_fn data 0x%02x 0x%04x",
                (int) hdr->frame_id, (int) hdr_port_def(hdr));
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
                discard = true;
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
    if (discard) {
        LOGF_DEBUG1("rx_hook_fn discard duplicate %p", (void *) frame);
        embc_buffer_free(frame);
    }
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
            LOGS_DEBUG1("framer_resync MIC");
            send_frame_ack(self, self->rx_buffer, EMBC_FRAMER_ACK_MASK_CURRENT, status);
        } else {
            LOGS_DEBUG1("framer_resync sync");
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
                LOGS_DEBUG3("rx_buffer may contain more data than just the frame, copy");
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
                    LOGS_DEBUG3("transfer rx_buffer ownership, create new rx_buffer");
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
        uint8_t const * buffer, embc_size_t length) {
    for (uint32_t i = 0; i < length; ++i) {
        embc_framer_hal_rx_byte(self, buffer[i]);
    }
}

void embc_framer_hal_tx_done(
        struct embc_framer_s * self,
        struct embc_buffer_s * buffer) {
    struct embc_list_s * item;
    struct embc_framer_header_s * hdr = buffer_hdr(buffer);
    if (EMBC_FRAMER_TYPE_ACK == (hdr->frame_id & EMBC_FRAMER_TYPE_MASK)) {
        LOGF_DEBUG3("embc_framer_hal_tx_done ack %d %p", hdr->frame_id, (void *) buffer);
        embc_buffer_free(buffer);
    } else {
        LOGF_DEBUG3("embc_framer_hal_tx_done data %d %p", hdr->frame_id, (void *) buffer);
        embc_list_foreach(&self->tx_buffers_active, item) {
            struct tx_buf_s *t = embc_list_entry(item, struct tx_buf_s, item);
            if (t->b == buffer) {
                t->status = TX_STATUS_AWAIT_ACK;
            }
        }
    }
}

struct embc_buffer_s * embc_framer_construct_frame(
        struct embc_framer_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const *payload, uint8_t length) {
    DBC_RANGE_INT(port, 0, (EMBC_FRAMER_PORTS - 1));
    uint32_t frame_crc = 0;
    LOGS_DEBUG3("embc_framer_construct_frame");
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, length + HEADER_SIZE + FOOTER_SIZE);
    struct embc_framer_header_s * hdr = buffer_hdr(b);
    hdr->sof = SOF;
    hdr->frame_id = frame_id & EMBC_FRAMER_ID_MASK;
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
    LOGS_DEBUG3("embc_framer_construct_ack");
    struct embc_buffer_s * b = embc_buffer_alloc(self->buffer_allocator, 1 + HEADER_SIZE + FOOTER_SIZE);
    struct embc_framer_header_s * hdr = buffer_hdr(b);
    hdr->sof = SOF;
    hdr->frame_id = EMBC_FRAMER_TYPE_ACK | (frame_id & EMBC_FRAMER_ID_MASK);
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
    DBC_RANGE_INT(port, 0, EMBC_FRAMER_PORTS - 1);
    DBC_NOT_NULL(buffer);
    uint32_t frame_crc = 0;
    embc_size_t length = embc_buffer_length(buffer);

    if (buffer->reserve < EMBC_FRAMER_FOOTER_SIZE) {
        LOGF_DEBUG1("embc_framer_send buffer copy %p", (void *) buffer);
        EMBC_ASSERT(length < EMBC_FRAMER_PAYLOAD_MAX_SIZE);
        struct embc_buffer_s * b = embc_framer_alloc(self);
        embc_buffer_write(b, buffer->data, length);
        embc_buffer_free(buffer);
        buffer = b;
        length = embc_buffer_length(buffer);
    }
    EMBC_ASSERT(buffer->reserve == EMBC_FRAMER_FOOTER_SIZE);

    buffer->reserve = 0;
    struct embc_framer_header_s * hdr = buffer_hdr(buffer);

    hdr->sof = SOF;
    hdr->frame_id = self->tx_next_frame_id;
    hdr->port = port;
    hdr->message_id = message_id;
    hdr->length = (uint8_t) ((length - HEADER_SIZE) & 0xff);
    hdr->port_def0 = (uint8_t) (port_def & 0xff);
    hdr->port_def1 = (uint8_t) ((port_def >> 8) & 0xff);
    hdr->crc8 = crc_ccitt_8(0, buffer->data, HEADER_SIZE - 1);
    frame_crc = crc32(frame_crc, buffer->data, length);
    embc_buffer_cursor_set(buffer, length);
    embc_buffer_write_u32_le(buffer, frame_crc);
    embc_buffer_write_u8(buffer, SOF);
    self->tx_next_frame_id = (self->tx_next_frame_id + 1) & EMBC_FRAMER_ID_MASK;
    embc_list_add_tail(&self->tx_queue, &buffer->item);
    transmit_queued(self);
}

EMBC_API void embc_framer_send_payload(
        struct embc_framer_s * self,
        uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * data, uint8_t length) {
    DBC_RANGE_INT(length, 0, EMBC_FRAMER_PAYLOAD_MAX_SIZE - 1);
    LOGS_DEBUG3("embc_framer_send_payload");
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
