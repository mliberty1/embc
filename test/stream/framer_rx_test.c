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
#include "embc/stream/framer_rx.h"


#define SOF (EMBC_FRAMER_SOF)


static uint8_t SOF_5[] = {SOF, SOF, SOF, SOF, SOF};
static uint8_t SOF_64[] = {SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF,
                           SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF,
                           SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF,
                           SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF, SOF,};

static uint8_t ACK1[] = {0x55, 0x98, 0x01, 0x7B};
static uint8_t ACK2[] = {0x55, 0x9C, 0x56, 0x28};
static uint8_t NACK1[] = {0x55, 0xD8, 1, 0, 2, 0x71};
static uint8_t NACK2[] = {0x55, 0xDC, 0x56, 0x81, 0x23, 0x47};
static uint8_t GARGABE1[] = {0x00, 0xff, 0x00, 0xff, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
static uint8_t FRAME1[] = {0x55, 0x18, 0x01, 0x07, 0x02, 0x11,
                           1, 2, 3, 4, 5, 6, 7, 8,
                           0x7a, 0xbb, 0x48, 0xb6};
static uint8_t FRAME2[] = {0x55, 0x18, 0x01, 0x09, 0x02, 0x11,
                           0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
                           0x33, 0xc9, 0xf3, 0xdf};

static uint8_t FRAME_MIN[] = {0x55, 0x18, 0x01, 0x00, 0x02, 0x11,
                              0xff,
                              0x16, 0x10, 0x78, 0x1e};
static uint8_t FRAME_MAX[] = {0x55, 0x18, 0x01, 0xFF, 0x02, 0x11,
                              0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                              0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                              0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
                              0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
                              0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
                              0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
                              0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
                              0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
                              0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
                              0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
                              0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
                              0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
                              0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
                              0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
                              0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
                              0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
                              0xfe, 0x84, 0xb2, 0x16};

struct test_s {
    struct embc_framer_rx_s rx;
};

static void on_frame_error(void * user_data) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected_ptr(self);
}

static void expect_frame_error() {
    expect_any(on_frame_error, self);
}

static void on_ack(void * user_data, uint16_t frame_id) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(frame_id);
}

static void expect_ack(uint16_t frame_id) {
    expect_value(on_ack, frame_id, frame_id);
}

static void on_nack(void * user_data, uint16_t frame_id, uint8_t cause, uint16_t cause_frame_id) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(frame_id);
    check_expected(cause);
    check_expected(cause_frame_id);
}

static void expect_nack(uint16_t frame_id, uint8_t cause, uint16_t cause_frame_id) {
    expect_value(on_nack, frame_id, frame_id);
    expect_value(on_nack, cause, cause);
    expect_value(on_nack, cause_frame_id, cause_frame_id);
}

static void on_frame(void * user_data, uint16_t frame_id, enum embc_framer_sequence_e seq,
              uint8_t port_id, uint8_t message_id, uint8_t * buf, uint16_t buf_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(frame_id);
    check_expected(seq);
    check_expected(port_id);
    check_expected(message_id);
    check_expected(buf_size);
    check_expected_ptr(buf);
}

// static void expect_frame(uint8_t const * x) {
#define expect_frame(x) { \
    uint32_t len__ = 1 + ((uint16_t) x[3]); \
    expect_value(on_frame, frame_id, (((uint16_t) (x[1] & 0x07)) << 8) | x[2]); \
    expect_value(on_frame, seq, (x[1] >> 3) & 0x03); \
    expect_value(on_frame, port_id, x[4] & 0x1f); \
    expect_value(on_frame, message_id, x[5]); \
    expect_value(on_frame, buf_size, len__); \
    expect_memory(on_frame, buf, x + EMBC_FRAMER_HEADER_SIZE, len__); \
}

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->rx.api.user_data = self;
    self->rx.api.on_frame_error = on_frame_error;
    self->rx.api.on_ack = on_ack;
    self->rx.api.on_nack = on_nack;
    self->rx.api.on_frame = on_frame;
    embc_framer_rx_initialize(&self->rx);
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void test_receive_ack1(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;

    expect_ack(1);
    embc_framer_rx_recv(&self->rx, ACK1, sizeof(ACK1));
}

static void test_receive_ack2(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    expect_ack(0x456);
    embc_framer_rx_recv(&self->rx, ACK2, sizeof(ACK2));
}

static void test_receive_nack1(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    expect_nack(1, 0, 2);
    embc_framer_rx_recv(&self->rx, NACK1, sizeof(NACK1));
}

static void test_receive_nack2(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    expect_nack(0x456, 1, 0x123);
    embc_framer_rx_recv(&self->rx, NACK2, sizeof(NACK2));
}

static void test_receive_garbage(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    embc_framer_rx_recv(&self->rx, GARGABE1, sizeof(GARGABE1));
}

static void test_receive_garbage_then_ack1(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    embc_framer_rx_recv(&self->rx, GARGABE1, sizeof(GARGABE1));
    expect_ack(1);
    embc_framer_rx_recv(&self->rx, ACK1, sizeof(ACK1));
}

static void test_receive_one(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_receive_one_many_sof(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_rx_recv(&self->rx, SOF_5, sizeof(SOF_5));
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_receive_one_many_sof_split(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t sof[] = {EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF};
    embc_framer_rx_recv(&self->rx, sof, sizeof(sof));
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1 + 1, sizeof(FRAME1) - 1);
}

static void test_receive_one_initial_garbage(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_rx_recv(&self->rx, GARGABE1, sizeof(GARGABE1));
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_receive_three(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
    expect_frame(FRAME2);
    embc_framer_rx_recv(&self->rx, FRAME2, sizeof(FRAME2));
    embc_framer_rx_recv(&self->rx, SOF_5, sizeof(SOF_5));
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_truncated(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_rx_recv(&self->rx, FRAME1, 4);
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_min_length(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    expect_frame(FRAME_MIN);
    embc_framer_rx_recv(&self->rx, FRAME_MIN, sizeof(FRAME_MIN));
}

static void test_max_length(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    expect_frame(FRAME_MAX);
    embc_framer_rx_recv(&self->rx, FRAME_MAX, sizeof(FRAME_MAX));
}

static void test_frame_error(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
    expect_frame_error();
    embc_framer_rx_recv(&self->rx, GARGABE1, sizeof(GARGABE1));
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
}

static void test_truncated_flush_with_sof(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
    embc_framer_rx_recv(&self->rx, FRAME_MAX, 16);
    expect_frame_error();
    expect_frame(FRAME1);
    embc_framer_rx_recv(&self->rx, FRAME1, sizeof(FRAME1));
    for (int i = 0; i < 5; ++i) {
        embc_framer_rx_recv(&self->rx, SOF_64, sizeof(SOF_64));
    }
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_receive_ack1, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_ack2, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_nack1, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_nack2, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_garbage, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_garbage_then_ack1, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_one, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_one_many_sof, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_one_many_sof_split, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_one_initial_garbage, setup, teardown),
            cmocka_unit_test_setup_teardown(test_receive_three, setup, teardown),
            cmocka_unit_test_setup_teardown(test_truncated, setup, teardown),
            cmocka_unit_test_setup_teardown(test_min_length, setup, teardown),
            cmocka_unit_test_setup_teardown(test_max_length, setup, teardown),
            cmocka_unit_test_setup_teardown(test_frame_error, setup, teardown),
            cmocka_unit_test_setup_teardown(test_truncated_flush_with_sof, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
