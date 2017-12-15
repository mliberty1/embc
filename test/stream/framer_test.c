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

const embc_size_t BUFFER_ALLOCATOR[] = {0, 0, 0, 8};

uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
const uint8_t FRAME1[] = {0xAA, 0x00, 0x02, 0x03, 0x08, 0x11, 0x22, 0x41,
                          1, 2, 3, 4, 5, 6, 7, 8,
                          0x63, 0x31, 0xb3, 0xbe, 0xAA};

struct test_s {
    struct embc_buffer_allocator_s * buffer_allocator;
    struct embc_framer_s * f1;
    struct embc_list_s tx;
    struct embc_list_s rx[8];
};

void hal_tx_cbk(void *user_data, struct embc_buffer_s * buffer) {
    struct test_s * self = (struct test_s *) user_data;
    embc_list_add_tail(&self->tx, &buffer->item);
}

int32_t hal_timer_set_cbk(void *user_data, uint64_t duration,
                          void (*cbk_fn)(void *, uint32_t), void *cbk_user_data,
                          uint32_t *timer_id) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(duration);
    // todo
    (void) cbk_fn;
    (void) cbk_user_data;
    *timer_id = 0;
    return 0;
}

int32_t hal_timer_cancel_cbk(void *user_data, uint32_t timer_id) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(timer_id);
    return 0;
}

void rx_cbk(void *user_data,
            uint8_t port, uint8_t message_id, uint16_t port_def,
            struct embc_buffer_s * buffer) {
    check_expected(port);
    check_expected(message_id);
    check_expected(port_def);
    struct test_s * self = (struct test_s *) user_data;
    embc_list_add_tail(&self->rx[port], &buffer->item);
}

void tx_done_cbk(void * user_data, uint8_t port, uint8_t message_id, int32_t status) {
    (void) user_data;
    check_expected(port);
    check_expected(message_id);
    check_expected(status);
}

static int setup(void ** state) {
    struct test_s * self =  0;
    uint32_t sz = embc_framer_instance_size();
    assert_int_equal(0x280, sz);
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->buffer_allocator = embc_buffer_initialize(BUFFER_ALLOCATOR, EMBC_ARRAY_SIZE(BUFFER_ALLOCATOR));
    self->f1 = test_calloc(1, sz);
    embc_list_initialize(&self->tx);
    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(self->rx); ++i) {
        embc_list_initialize(&self->rx[i]);
    }

    struct embc_framer_hal_callbacks_s hal_callbacks = {
            .tx_fn = hal_tx_cbk,
            .tx_user_data = self,
            .timer_set_fn = hal_timer_set_cbk,
            .timer_set_user_data = self,
            .timer_cancel_fn = hal_timer_cancel_cbk,
            .timer_cancel_user_data = self,
    };
    embc_framer_initialize(self->f1, self->buffer_allocator, &hal_callbacks);

    struct embc_framer_port_callbacks_s port2_callbacks = {
            .rx_fn = rx_cbk,
            .rx_user_data = self,
            .tx_done_fn = tx_done_cbk,
            .tx_done_user_data = self
    };
    embc_framer_register_port_callbacks(self->f1, 2, &port2_callbacks);

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

static void tx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_buffer_s * b = embc_framer_alloc(self->f1);
    embc_buffer_write(b, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_send(self->f1, 2, 3, 0x2211, b);
    assert_int_equal(1, embc_list_length(&self->tx));
    assert_ptr_equal(&b->item, embc_list_peek_head(&self->tx));
    assert_int_equal(sizeof(FRAME1), embc_buffer_length(b));
    assert_memory_equal(FRAME1, b->data, sizeof(FRAME1));
    embc_buffer_free(b);
}

static void tx_multiple(void **state) {
    struct test_s *self = (struct test_s *) *state;
    for (int i = 0; i < 10; ++i) {
        struct embc_buffer_s * b = embc_framer_alloc(self->f1);
        embc_buffer_write(b, PAYLOAD1, sizeof(PAYLOAD1));
        embc_framer_send(self->f1, 2, 3, 0x2211, b);
        assert_ptr_equal(&b->item, embc_list_remove_head(&self->tx));
        assert_int_equal(sizeof(FRAME1), embc_buffer_length(b));
        assert_memory_equal(FRAME1, b->data, sizeof(FRAME1));
        embc_buffer_free(b);
    }
}

static void send_payload1(struct test_s *self) {
    expect_value(rx_cbk, port, 2);
    expect_value(rx_cbk, message_id, 3);
    expect_value(rx_cbk, port_def, 0x2211);
    embc_framer_hal_rx_buffer(self->f1, FRAME1, sizeof(FRAME1));
    struct embc_list_s * x = embc_list_remove_head(&self->rx[2]);
    assert_non_null(x);
    struct embc_buffer_s * b = embc_list_entry(x, struct embc_buffer_s, item);
    assert_int_equal(sizeof(PAYLOAD1), embc_buffer_length(b));
    assert_memory_equal(PAYLOAD1, b->data, sizeof(PAYLOAD1));
}

static void rx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    send_payload1(self);
}

#if 0
static void rx_duplicate_sof(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    embc_framer_rx_byte(self->f1, 0xAA);
    send_payload1(self);
}

static void rx_garbage_before_sof(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    embc_framer_rx_byte(self->f1, 0x11);
    embc_framer_rx_byte(self->f1, 0x22);
    send_payload1(self);
}

static void header_fragment(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    embc_framer_rx_buffer(self->f1, FRAME1, 3);
    send_payload1(self);

    embc_framer_rx_buffer(self->f1, FRAME1, 3);
    expect_value(error_cbk, id, 2);
    expect_value(error_cbk, status, EMBC_ERROR_SYNCHRONIZATION);

    send_payload1(self);
}

static void header_crc_bad(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    send_payload1(self);
    embc_framer_rx_buffer(self->f1, FRAME1, 4);
    expect_value(error_cbk, id, 2);
    expect_value(error_cbk, status, EMBC_ERROR_SYNCHRONIZATION);
    embc_framer_rx_byte(self->f1, 0x00);
    send_payload1(self);
}

static void frame_crc_bad(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    embc_framer_rx_buffer(self->f1, FRAME1, sizeof(FRAME1) - 2);
    embc_framer_rx_byte(self->f1, 0x00);
    expect_value(error_cbk, id, 2);
    expect_value(error_cbk, status, EMBC_ERROR_MESSAGE_INTEGRITY);
    embc_framer_rx_byte(self->f1, 0xAA);
    send_payload1(self);
}

static void rx_multiple(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);

    for (int i = 0; i < 10; ++i) {
        expect_value(rx_cbk, id, 2);
        expect_value(rx_cbk, port, 3);
        expect_value(rx_cbk, length, sizeof(PAYLOAD1));
        expect_memory(rx_cbk, buffer, PAYLOAD1, sizeof(PAYLOAD1));
        embc_framer_rx_buffer(self->f1, FRAME1, sizeof(FRAME1));
        for (int k = 0; k < i; ++k) {
            embc_framer_rx_byte(self->f1, 0xAA);
        }
    }
}
#endif

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(validate, 0, 0),
            cmocka_unit_test_setup_teardown(tx_single, setup, teardown),
            cmocka_unit_test_setup_teardown(tx_multiple, setup, teardown),
            cmocka_unit_test_setup_teardown(rx_single, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_duplicate_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_garbage_before_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(header_fragment, setup, teardown),
            //cmocka_unit_test_setup_teardown(header_crc_bad, setup, teardown),
            //cmocka_unit_test_setup_teardown(frame_crc_bad, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_multiple, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
