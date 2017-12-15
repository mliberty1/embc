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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "embc/stream/framer.h"
#include "embc/stream/framer_util.h"
#include "embc.h"


uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
const uint8_t FRAME1[] = {0xAA, 0x02, 0x03, 0x04, 0x08, 0x11, 0x22, 0xe0,
                          1, 2, 3, 4, 5, 6, 7, 8,
                          0x73, 0xf1, 0x96, 0x05, 0xAA};

struct test_s {
    uint8_t buffer_[512];
    uint16_t offset_;
    struct embc_framer_s * f1;
};

void tx_cbk(
        void * user_data,
        uint8_t const *buffer, uint16_t length) {
    assert_int_equal(11, (intptr_t) user_data);
    // todo chack memory
    (void) buffer;
    (void) length;
}

int32_t timer_set_cbk(void * user_data, uint64_t duration,
                     void (*cbk_fn)(void *, uint32_t), void * cbk_user_data,
                     uint32_t * timer_id) {
    assert_int_equal(12, (intptr_t) user_data);
    check_expected(duration);
    // todo
    (void) cbk_fn;
    (void) cbk_user_data;
    *timer_id = 0;
    return 0;
}

int32_t timer_cancel_cbk(void * user_data, uint32_t timer_id) {
    assert_int_equal(13, (intptr_t) user_data);
    check_expected(timer_id);
    return 0;
}

static int setup(void ** state) {
    struct test_s *self =  0;
    uint32_t sz = embc_framer_instance_size();
    assert_int_equal(0x560, sz);
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->f1 = test_calloc(1, sz);

    struct embc_framer_hal_callbacks_s hal_callbacks = {
            .tx_fn = tx_cbk,
            .tx_user_data = (void *) 11,
            .timer_set_fn = timer_set_cbk,
            .timer_set_user_data = (void *) 12,
            .timer_cancel_fn = timer_cancel_cbk,
            .timer_cancel_user_data = (void *) 13,
    };
    embc_framer_initialize(self->f1, &hal_callbacks);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_finalize(self->f1);
    test_free(self->f1);
    test_free(self);
    return 0;
}

#if 0

static void tx_fn(void * user_data, uint8_t const *buffer, uint16_t length) {
    struct test_s *self = (struct test_s *) user_data;
    assert_true((embc_size_t) (self->offset_ + length) <= sizeof(self->buffer_));
    memcpy(self->buffer_ + self->offset_, buffer, length);
    self->offset_ += length;
}

static void rx_cbk(void *user_data,
                   uint8_t id, uint8_t port,
                   uint8_t const * buffer, uint32_t length) {
    (void) user_data;
    check_expected(id);
    check_expected(port);
    check_expected(length);
    check_expected_ptr(buffer);
}

static void error_cbk(void *user_data, uint8_t id, uint8_t status) {
    (void) user_data;
    check_expected(id);
    check_expected(status);
}
#endif

static void validate(void ** state) {
    (void) state;
    uint16_t length = 0;
    assert_int_equal(0, embc_framer_validate(FRAME1, sizeof(FRAME1), &length));
    assert_int_equal(sizeof(FRAME1), length);
}

static void tx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    //embc_framer_send(self->f1, 2, 3, PAYLOAD1, sizeof(PAYLOAD1));
    //assert_int_equal(sizeof(FRAME1), self->offset_);
    //assert_memory_equal(FRAME1, self->buffer_, sizeof(FRAME1));
}

#if 0
static void tx_multiple(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);
    for (int i = 0; i < 10; ++i) {
        embc_framer_send(self->f1, 2, 3, PAYLOAD1, sizeof(PAYLOAD1));
        assert_int_equal(sizeof(FRAME1), self->offset_);
        assert_memory_equal(FRAME1, self->buffer_, sizeof(FRAME1));
        self->offset_ = 0;
        memset(self->buffer_, 0, sizeof(self->buffer_));
    }
}

static void send_payload1(struct test_s *self) {
    expect_value(rx_cbk, id, 2);
    expect_value(rx_cbk, port, 3);
    expect_value(rx_cbk, length, sizeof(PAYLOAD1));
    expect_memory(rx_cbk, buffer, PAYLOAD1, sizeof(PAYLOAD1));
    embc_framer_rx_buffer(self->f1, FRAME1, sizeof(FRAME1));
}

static void rx_single(void **state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_initialize(self->f1, tx_fn, self);
    embc_framer_rx_callback(self->f1, rx_cbk, self);
    embc_framer_error_callback(self->f1, error_cbk, self);
    send_payload1(self);
}

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
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(validate, 0, 0),
            cmocka_unit_test_setup_teardown(tx_single, setup, teardown),
            //cmocka_unit_test_setup_teardown(tx_multiple, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_single, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_duplicate_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_garbage_before_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(header_fragment, setup, teardown),
            //cmocka_unit_test_setup_teardown(header_crc_bad, setup, teardown),
            //cmocka_unit_test_setup_teardown(frame_crc_bad, setup, teardown),
            //cmocka_unit_test_setup_teardown(rx_multiple, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
