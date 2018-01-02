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

#include "../hal_test_impl.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "embc/stream/framer.h"
#include "embc/stream/framer_util.h"
#include "embc/memory/buffer.h"
#include "embc/collections/list.h"
#include "embc.h"

const embc_size_t BUFFER_ALLOCATOR[] = {4, 0, 0, 8};

uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
const uint8_t FRAME1[] = {0xAA, 0x00, 0x02, 0x03, 0x08, 0x11, 0x22, 0x41,
                          1, 2, 3, 4, 5, 6, 7, 8,
                          0x63, 0x31, 0xb3, 0xbe, 0xAA};
uint8_t PAYLOAD2[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

struct timer_s {
    uint64_t duration;
    void (*cbk_fn)(void *, uint32_t);
    void * cbk_user_data;
    uint32_t timer_id;
    struct embc_list_s item;
};

struct test_s {
    struct embc_buffer_allocator_s * buffer_allocator;
    struct embc_framer_s * f1;
    struct embc_list_s tx;

    struct timer_s timers[24];
    struct embc_list_s timers_free;
    struct embc_list_s timers_pending;
};

void hal_tx_cbk(void *user_data, struct embc_buffer_s * buffer) {
    struct test_s * self = (struct test_s *) user_data;
    embc_list_add_tail(&self->tx, &buffer->item);
}

int32_t hal_timer_set_cbk(void *user_data, uint64_t duration,
                          void (*cbk_fn)(void *, uint32_t), void *cbk_user_data,
                          uint32_t *timer_id) {
    struct test_s * self = (struct test_s *) user_data;
    struct embc_list_s * item = embc_list_remove_head(&self->timers_free);
    assert_non_null(item);
    struct timer_s * timer = embc_list_entry(item, struct timer_s, item);
    timer->duration = duration;
    timer->cbk_fn = cbk_fn;
    timer->cbk_user_data = cbk_user_data;
    embc_list_add_tail(&self->timers_pending, &timer->item);
    *timer_id = timer->timer_id;
    return 0;
}

int32_t hal_timer_cancel_cbk(void *user_data, uint32_t timer_id) {
    struct test_s * self = (struct test_s *) user_data;
    struct timer_s * timer = &self->timers[timer_id];
    embc_list_remove(&timer->item);
    timer->cbk_fn = 0;
    timer->cbk_user_data = 0;
    embc_list_add_tail(&self->timers_free, &timer->item);
    return 0;
}

void rx_hook_fn(
        void * user_data,
        struct embc_buffer_s * buffer) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    uint8_t length = buffer->length;
    uint8_t * frame = buffer->data;
    check_expected(length);
    check_expected_ptr(frame);
    embc_buffer_free(buffer);
}

void rx_cbk(void *user_data,
            uint8_t port, uint8_t message_id, uint16_t port_def,
            struct embc_buffer_s * buffer) {
    (void) user_data;
    uint8_t length = embc_buffer_read_remaining(buffer);
    uint8_t * payload = buffer->data + buffer->cursor;
    check_expected(port);
    check_expected(message_id);
    check_expected(port_def);
    check_expected(length);
    check_expected_ptr(payload);
    embc_buffer_free(buffer);
}

void tx_done_cbk(void * user_data, uint8_t port, uint8_t message_id, uint16_t port_def, int32_t status) {
    (void) user_data;
    check_expected(port);
    check_expected(message_id);
    check_expected(port_def),
    check_expected(status);
}

static void port_register(struct test_s * self, uint8_t port) {
    struct embc_framer_port_callbacks_s port_callbacks = {
            .rx_fn = rx_cbk,
            .rx_user_data = self,
            .tx_done_fn = tx_done_cbk,
            .tx_done_user_data = self
    };
    embc_framer_register_port_callbacks(self->f1, port, &port_callbacks);
}

static int setup(void ** state) {
    struct test_s * self =  0;
    uint32_t sz = embc_framer_instance_size();
    //assert_int_equal(0x330, sz);
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->buffer_allocator = embc_buffer_initialize(BUFFER_ALLOCATOR, EMBC_ARRAY_SIZE(BUFFER_ALLOCATOR));
    self->f1 = test_calloc(1, sz);
    embc_list_initialize(&self->tx);

    struct embc_framer_hal_callbacks_s hal_callbacks = {
            .tx_fn = hal_tx_cbk,
            .tx_user_data = self,
            .timer_set_fn = hal_timer_set_cbk,
            .timer_set_user_data = self,
            .timer_cancel_fn = hal_timer_cancel_cbk,
            .timer_cancel_user_data = self,
    };
    embc_framer_initialize(self->f1, self->buffer_allocator, &hal_callbacks);
    port_register(self, 1);

    embc_list_initialize(&self->timers_free);
    embc_list_initialize(&self->timers_pending);
    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->timers); ++i) {
        self->timers[i].timer_id = i;
        embc_list_add_tail(&self->timers_free, &self->timers[i].item);
    }

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_finalize(self->f1);
    test_free(self->f1);
    embc_buffer_finalize(self->buffer_allocator);
    test_free(self);
    return 0;
}

static void validate(void ** state) {
    (void) state;
    uint16_t length = 0;
    assert_int_equal(0, embc_framer_validate(FRAME1, sizeof(FRAME1), &length));
    assert_int_equal(sizeof(FRAME1), length);
}

static void construct_frame(void **state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_buffer_s *b = embc_framer_construct_frame(
            self->f1, 0, 2, 3, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    assert_int_equal(sizeof(FRAME1), b->length);
    assert_memory_equal(b->data, FRAME1, b->length);
    embc_buffer_free(b);
}

static void tx_validate_and_confirm(
        struct test_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t const * payload, uint8_t length) {
    struct embc_buffer_s * c = embc_framer_construct_frame(
            self->f1, frame_id, port, message_id, port_def, payload, length);
    assert_false(embc_list_is_empty(&self->tx));
    struct embc_buffer_s * b = embc_list_entry(embc_list_remove_head(&self->tx), struct embc_buffer_s, item);
    assert_int_equal(b->length, c->length);
    assert_memory_equal(b->data, c->data, b->length);
    embc_framer_hal_tx_done(self->f1, b);
    embc_buffer_free(c);
}

#define expect_tx_done(port_, message_id_, port_def_, status_) \
    expect_value(tx_done_cbk, port, port_); \
    expect_value(tx_done_cbk, message_id, message_id_); \
    expect_value(tx_done_cbk, port_def, port_def_); \
    expect_value(tx_done_cbk, status, status_)

static void perform_ack(
        struct test_s *self,
        uint8_t frame_id, uint8_t port, uint8_t message_id, uint16_t port_def,
        uint8_t status) {
    struct embc_buffer_s * ack = embc_framer_construct_ack(
            self->f1, frame_id, port, message_id, port_def, status);
    embc_framer_hal_rx_buffer(self->f1, ack->data, ack->length);
    embc_buffer_free(ack);
}

static void tx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    assert_int_equal(0, embc_framer_status_get(self->f1).tx_count);
    embc_framer_send_payload(self->f1, 1, 3, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 0, 1, 3, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 3, 0x2211, 0);
    perform_ack(self, 0, 1, 3, EMBC_FRAMER_ACK_MASK_CURRENT, 0);
    assert_int_equal(1, embc_framer_status_get(self->f1).tx_count);
}

static void tx_multiple(void **state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t frame_id = 0;
    for (int i = 0; i < 10; ++i) {
        uint8_t message_id = (uint8_t) (i & 0xff);
        embc_framer_send_payload(self->f1, 1, message_id, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        tx_validate_and_confirm(self, frame_id, 1, message_id, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        expect_tx_done(1, message_id, 0x2211, 0);
        perform_ack(self, frame_id, 1, message_id, EMBC_FRAMER_ACK_MASK_CURRENT, 0);
        frame_id = (frame_id + 1) & EMBC_FRAMER_ID_MASK;
    }
    assert_int_equal(10, embc_framer_status_get(self->f1).tx_count);
    assert_int_equal(0, embc_framer_status_get(self->f1).tx_retransmit_count);
}

static void send_payload1(struct test_s *self) {
    struct embc_framer_status_s s1 = embc_framer_status_get(self->f1);
    expect_value(rx_hook_fn, length, sizeof(FRAME1));
    expect_memory(rx_hook_fn, frame, FRAME1, sizeof(FRAME1));
    embc_framer_hal_rx_buffer(self->f1, FRAME1, sizeof(FRAME1));
    struct embc_framer_status_s s2 = embc_framer_status_get(self->f1);
    assert_int_equal(1, s2.rx_count - s1.rx_count);
}

static void rx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);
    send_payload1(self);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_count);
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_synchronization_error);
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_deduplicate_count);
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_mic_error);
}

static void rx_duplicate_sof(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    send_payload1(self);
}

static void rx_garbage_before_sof(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    embc_framer_hal_rx_byte(self->f1, 0x11);
    embc_framer_hal_rx_byte(self->f1, 0x22);
    send_payload1(self);
}

static void header_fragment(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    embc_framer_hal_rx_buffer(self->f1, FRAME1, 3);
    send_payload1(self);

    embc_framer_hal_rx_buffer(self->f1, FRAME1, 3);
    //expect_value(error_cbk, id, 2);
    //expect_value(error_cbk, status, EMBC_ERROR_SYNCHRONIZATION);

    send_payload1(self);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_synchronization_error);
}

static void header_crc_bad(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    send_payload1(self);
    embc_framer_hal_rx_buffer(self->f1, FRAME1, 7);
    embc_framer_hal_rx_byte(self->f1, 0x00);
    send_payload1(self);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_synchronization_error);
}

static void frame_crc_bad(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    embc_framer_hal_rx_buffer(self->f1, FRAME1, sizeof(FRAME1) - 2);
    embc_framer_hal_rx_byte(self->f1, 0x00);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_mic_error);
}

static void rx_multiple(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_register_rx_hook(self->f1, rx_hook_fn, self);

    for (int i = 0; i < 10; ++i) {
        send_payload1(self);
        for (int k = 0; k < i; ++k) {
            embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
        }
    }
}

static void expect_frame(uint8_t port, uint8_t message_id, uint16_t port_def,
                         uint8_t const * payload, uint8_t length) {
    expect_value(rx_cbk, port, port);
    expect_value(rx_cbk, message_id, message_id);
    expect_value(rx_cbk, port_def, port_def);
    expect_value(rx_cbk, length, length);
    expect_memory(rx_cbk, payload, payload, length);
}

static void check_ack(struct test_s *self, uint8_t frame_id, uint8_t port, uint8_t message_id, uint8_t status, uint16_t bitmask) {
    assert_true(embc_list_length(&self->tx) > 0);
    struct embc_buffer_s * b = embc_list_entry(embc_list_remove_head(&self->tx), struct embc_buffer_s, item);
    uint16_t length = 0;
    assert_int_equal(0, embc_framer_validate(b->data, b->length, &length));
    assert_int_equal(length, 1 + EMBC_FRAMER_HEADER_SIZE + EMBC_FRAMER_FOOTER_SIZE);
    struct embc_framer_header_s * hdr = (struct embc_framer_header_s *) b->data;
    assert_int_equal(EMBC_FRAMER_SOF, hdr->sof);
    assert_int_equal(frame_id | 0x80, hdr->frame_id);
    assert_int_equal(port, hdr->port);
    assert_int_equal(message_id, hdr->message_id);
    uint16_t port_def = (((uint16_t) hdr->port_def1) << 8) | hdr->port_def0;
    assert_int_equal(bitmask, port_def);
    assert_int_equal(status, b->data[EMBC_FRAMER_HEADER_SIZE]);
    embc_buffer_free(b);
}

static void send_frame(struct test_s *self, int id, uint16_t mask, bool expect) {
    uint8_t frame_id = (uint8_t) (id & EMBC_FRAMER_ID_MASK);
    uint8_t message_id = (uint8_t) (id & 0xff);
    uint16_t port_def = (uint16_t) (id * 3);
    struct embc_buffer_s * b = embc_framer_construct_frame(
            self->f1, frame_id, 1, message_id, port_def, PAYLOAD1, sizeof(PAYLOAD1));
    if (expect) {
        expect_frame(1, message_id, port_def, PAYLOAD1, sizeof(PAYLOAD1));
    }
    embc_framer_hal_rx_buffer(self->f1, b->data, b->length);
    check_ack(self, frame_id, 1, message_id, 0, mask);
    embc_buffer_free(b);
}

static void rx_and_ack(void **state) {
    struct test_s *self = (struct test_s *) *state;
    uint16_t mask = 0x0100;
    for (int i = 0; i < EMBC_FRAMER_OUTSTANDING_FRAMES_MAX; ++i) {
        send_frame(self, i, mask, true);
        mask = 0x0100 | (mask >> 1);
    }
}

static void rx_dedup_on_lost_ack(void **state) {
    struct test_s *self = (struct test_s *) *state;
    send_frame(self, 0, 0x100, true);
    send_frame(self, 1, 0x180, true);
    send_frame(self, 2, 0x1C0, true);
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_deduplicate_count);
    send_frame(self, 1, 0x380, false);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_deduplicate_count);
}

static void rx_expect_ordered(struct test_s *self) {
    (void) self;
    expect_frame(1, 0, 0 * 3, PAYLOAD1, sizeof(PAYLOAD1));
    expect_frame(1, 1, 1 * 3, PAYLOAD1, sizeof(PAYLOAD1));
    expect_frame(1, 2, 2 * 3, PAYLOAD1, sizeof(PAYLOAD1));
}

static void rx_out_of_order_1(void **state) {
    struct test_s *self = (struct test_s *) *state;
    rx_expect_ordered(self);
    send_frame(self, 2, 0x100, false);
    send_frame(self, 0, 0x500, false);
    send_frame(self, 1, 0x380, false);
}

static void rx_out_of_order_2(void **state) {
    struct test_s *self = (struct test_s *) *state;
    rx_expect_ordered(self);
    send_frame(self, 2, 0x100, false);
    send_frame(self, 1, 0x300, false);
    send_frame(self, 0, 0x700, false);
}

static void rx_out_of_order_3(void **state) {
    struct test_s *self = (struct test_s *) *state;
    rx_expect_ordered(self);
    send_frame(self, 1, 0x100, false);
    send_frame(self, 0, 0x300, false);
    send_frame(self, 2, 0x1C0, false);
}

static void rx_out_of_order_4(void **state) {
    struct test_s *self = (struct test_s *) *state;
    rx_expect_ordered(self);
    send_frame(self, 1, 0x100, false);
    send_frame(self, 2, 0x180, false);
    send_frame(self, 0, 0x700, false);
}

static void rx_sequence(struct test_s *self, uint8_t offset, int count) {
    uint16_t mask = 0;
    for (int i = 0; i <= count; ++i) {
        uint8_t k = (uint8_t) (i + offset);
        uint8_t frame_id = k & EMBC_FRAMER_ID_MASK;
        struct embc_buffer_s * b = embc_framer_construct_frame(
                self->f1, frame_id, 1, k, 0x1122, PAYLOAD1, sizeof(PAYLOAD1));
        expect_frame(1, k, 0x1122, PAYLOAD1, sizeof(PAYLOAD1));
        embc_framer_hal_rx_buffer(self->f1, b->data, b->length);
        mask = 0x100 | (mask >> 1);
        check_ack(self, frame_id, 1, k, 0, mask);
        embc_buffer_free(b);
    }
}

static void rx_frame_id_resync(void **state) {
    struct test_s *self = (struct test_s *) *state;
    rx_sequence(self, 0, EMBC_FRAMER_OUTSTANDING_FRAMES_MAX);
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_frame_id_error);
    rx_sequence(self, 3 * EMBC_FRAMER_OUTSTANDING_FRAMES_MAX, EMBC_FRAMER_OUTSTANDING_FRAMES_MAX);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_frame_id_error);
}

static void rx_mic_error(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_frame(self, 0, 0x100, true);
    send_frame(self, 1, 0x180, true);
    struct embc_buffer_s * b = embc_framer_construct_frame(
            self->f1, 2, 1, 7, 0, PAYLOAD1, sizeof(PAYLOAD1));
    b->data[EMBC_FRAMER_HEADER_SIZE] = 0x9e;
    assert_int_equal(0, embc_framer_status_get(self->f1).rx_mic_error);
    embc_framer_hal_rx_buffer(self->f1, b->data, b->length);
    assert_int_equal(1, embc_framer_status_get(self->f1).rx_mic_error);
    check_ack(self, 2, 1, 7, EMBC_ERROR_MESSAGE_INTEGRITY, 0x0100);
    send_frame(self, 2, 0x1C0, true);
}

static void rx_resync_error(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_frame(self, 0, 0x100, true);
    send_frame(self, 1, 0x180, true);

    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, 0x00);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    embc_framer_hal_rx_byte(self->f1, EMBC_FRAMER_SOF);
    check_ack(self, 0, 0, 0, EMBC_ERROR_SYNCHRONIZATION, 0);
    send_frame(self, 2, 0x1C0, true);
}

static void tx_lost_acks(void **state) {
    struct test_s *self = (struct test_s *) *state;

    // frame 0 - no ack
    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    // frame 1 - no ack
    embc_framer_send_payload(self->f1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    expect_tx_done(1, 6, 0x2211, 0);
    expect_tx_done(1, 7, 0x2211, 0);
    expect_tx_done(1, 8, 0x2211, 0);

    // frame 2 - ack
    embc_framer_send_payload(self->f1, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    perform_ack(self, 2, 1, 8, 0x01C0, 0);
}

static void tx_lost_frame_and_retransmit_with_ack(void **state) {
    struct test_s *self = (struct test_s *) *state;

    // frame 0 - ack
    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 6, 0x2211, 0);
    perform_ack(self, 0, 1, 6, 0x0100, 0);

    // frame 1 - lost, no ack
    embc_framer_send_payload(self->f1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    // frame 2 - ack, still missing 1
    embc_framer_send_payload(self->f1, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    perform_ack(self, 2, 1, 8, 0x0140, 0);

    expect_tx_done(1, 7, 0x2211, 0);
    expect_tx_done(1, 8, 0x2211, 0);
    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    perform_ack(self, 1, 1, 7, 0x0380, 0);

    assert_int_equal(1, embc_framer_status_get(self->f1).tx_retransmit_count);
}

static void tx_retransmit_on_ack_mic_error(void **state) {
    struct test_s *self = (struct test_s *) *state;

    // frame 0 - ack
    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    // nack causes retransmit
    perform_ack(self, 0, 1, 6, 0x0100, EMBC_ERROR_MESSAGE_INTEGRITY);
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 6, 0x2211, 0);
    perform_ack(self, 0, 1, 6, 0, 0);
}

static void tx_queue_then_transmit(void **state) {
    struct test_s *self = (struct test_s *) *state;

    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 9, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 10, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    assert_int_equal(3, embc_list_length(&self->tx));

    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    assert_true(embc_list_is_empty(&self->tx));

    expect_tx_done(1, 6, 0x2211, 0);
    perform_ack(self, 0, 1, 6, 0x0100, 0);
    tx_validate_and_confirm(self, 3, 1, 9, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    //tx_validate_and_confirm(self, 4, 1, 10, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
}

static void signal_timeout(struct test_s * self) {
    struct embc_list_s * item = embc_list_remove_head(&self->timers_pending);
    struct timer_s * timer = embc_list_entry(item, struct timer_s, item);
    timer->cbk_fn(timer->cbk_user_data, timer->timer_id);
}

static void tx_retransmit_on_timeout(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    assert_int_equal(1, embc_list_length(&self->timers_pending));
    signal_timeout(self);
    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 6, 0x2211, 0);
    perform_ack(self, 0, 1, 6, 0x0100, 0);
}

static void tx_too_many_timeout_retransmits(void **state) {
    struct test_s *self = (struct test_s *) *state;

    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    for (int i = 0; i < EMBC_FRAMER_MAX_RETRIES; ++i) {
        tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        assert_int_equal(1, embc_list_length(&self->timers_pending));
        signal_timeout(self);
    }

    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 6, 0x2211, EMBC_ERROR_TIMED_OUT);
    signal_timeout(self);
}

static void tx_recover_after_error(void **state) {
    struct test_s *self = (struct test_s *) *state;

    embc_framer_send_payload(self->f1, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send_payload(self->f1, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    signal_timeout(self);
    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    signal_timeout(self);

    for (int i = 1; i < EMBC_FRAMER_MAX_RETRIES; ++i) {
        tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
        signal_timeout(self);
    }

    tx_validate_and_confirm(self, 0, 1, 6, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    expect_tx_done(1, 6, 0x2211, EMBC_ERROR_TIMED_OUT);
    signal_timeout(self);

    tx_validate_and_confirm(self, 1, 1, 7, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));
    tx_validate_and_confirm(self, 2, 1, 8, 0x2211, PAYLOAD1, sizeof(PAYLOAD1));

    perform_ack(self, 2, 1, 8, 0x0100, 0);
    expect_tx_done(1, 7, 0x2211, 0);
    expect_tx_done(1, 8, 0x2211, 0);
    perform_ack(self, 1, 1, 7, 0x0300, 0);
}

static void send_ping_req(struct test_s *self, int id, uint16_t mask) {
    uint8_t frame_id = (uint8_t) (id & EMBC_FRAMER_ID_MASK);
    uint8_t message_id = (uint8_t) (id & 0xff);
    struct embc_buffer_s * b = embc_framer_construct_frame(
            self->f1, frame_id, 0, message_id, EMBC_FRAMER_PORT0_PING_REQ, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_hal_rx_buffer(self->f1, b->data, b->length);
    if (id == 2) {
        tx_validate_and_confirm(self, 0, 0, 0, EMBC_FRAMER_PORT0_PING_RSP, PAYLOAD1, sizeof(PAYLOAD1));
        tx_validate_and_confirm(self, 1, 0, 1, EMBC_FRAMER_PORT0_PING_RSP, PAYLOAD1, sizeof(PAYLOAD1));
        perform_ack(self, 0, 0, 0, 0x0100, 0);
        perform_ack(self, 1, 0, 1, 0x0180, 0);
    }
    if (id >= 2) {
        tx_validate_and_confirm(self, frame_id, 0, message_id, EMBC_FRAMER_PORT0_PING_RSP, PAYLOAD1, sizeof(PAYLOAD1));
        perform_ack(self, frame_id, 0, message_id, 0x01C0, 0);
    }
    check_ack(self, frame_id, 0, message_id, 0, mask);
    embc_buffer_free(b);
}

static void ping(void **state) {
    struct test_s *self = (struct test_s *) *state;
    send_ping_req(self, 0, 0x0100);
    send_ping_req(self, 1, 0x0180);
    send_ping_req(self, 2, 0x01C0);
    send_ping_req(self, 3, 0x01E0);
    send_ping_req(self, 4, 0x01F0);
    send_ping_req(self, 5, 0x01F8);
}


int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(validate, 0, 0),
            cmocka_unit_test_setup_teardown(construct_frame, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_single, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_multiple, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_single, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_duplicate_sof, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_garbage_before_sof, setup, teardown),
            cmocka_unit_test_setup_teardown(header_fragment, setup, teardown),
            cmocka_unit_test_setup_teardown(header_crc_bad, setup, teardown),
            cmocka_unit_test_setup_teardown(frame_crc_bad, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_multiple, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_and_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_dedup_on_lost_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_out_of_order_1, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_out_of_order_2, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_out_of_order_3, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_out_of_order_4, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_frame_id_resync, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_mic_error, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_resync_error, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_lost_acks, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_lost_frame_and_retransmit_with_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_retransmit_on_ack_mic_error, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_queue_then_transmit, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_retransmit_on_timeout, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_too_many_timeout_retransmits, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_recover_after_error, setup, teardown),
            cmocka_unit_test_setup_teardown(ping, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
