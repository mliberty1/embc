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
#include "embc/stream/data_link.h"
#include "embc/memory/buffer.h"
#include "embc/collections/list.h"
#include "embc/crc.h"
#include "embc/time.h"
#include "embc.h"

#define SEND_BUFFER_SIZE (1 << 13)
static uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};

struct test_s {
    struct embc_framer_s * f;
    uint8_t send_buffer[SEND_BUFFER_SIZE];
    uint8_t send_buffer_size;
    uint32_t time_ms;
};

static uint32_t ll_time_get_ms(void * ll_user_data) {
    struct test_s * self = (struct test_s *) ll_user_data;
    return self->time_ms;
}

static void ll_send(void * ll_user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct test_s * self = (struct test_s *) ll_user_data;
    check_expected(buffer_size);
    check_expected_ptr(buffer);
    memcpy(self->send_buffer + self->send_buffer_size, buffer, buffer_size);
    self->send_buffer_size += buffer_size;
}

static uint32_t ll_send_available(void * ll_user_data) {
    struct test_s * self = (struct test_s *) ll_user_data;
    (void) self;
    return EMBC_FRAMER_MAX_SIZE; // todo
}

void recv_01(void *user_data, uint8_t port_id,
             uint16_t message_id, uint8_t *msg_buffer, uint32_t msg_size) {
    (void) user_data;
    check_expected(port_id);
    check_expected(message_id);
    check_expected(msg_size);
    check_expected_ptr(msg_buffer);
}

void event_cbk_01(void * user_data,
               struct embc_framer_s * instance,
               enum embc_framer_event_s event) {
    struct test_s * self = (struct test_s *) user_data;
    assert_ptr_equal(self->f, instance);
    check_expected(event);
}

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));

    struct embc_framer_config_s config = {
        .event_cbk = event_cbk_01,
        .event_user_data = self
    };

    struct embc_framer_ll_s ll = {
            .ll_user_data = self,
            .time_get_ms = ll_time_get_ms,
            .send = ll_send,
            .send_available = ll_send_available,
    };

    self->f = embc_framer_initialize(&config, &ll);
    assert_non_null(self->f);
    embc_framer_port_register(self->f, 2, recv_01, state);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_framer_finalize(self->f);
    test_free(self);
    return 0;
}

static void test_initial_state(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_framer_status_s status;

    assert_non_null(self->f);
    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(1, status.version);
    assert_int_equal(0, status.rx.total_bytes);
    assert_int_equal(0, status.rx.invalid_bytes);
    assert_int_equal(0, status.rx.data_frames);
    assert_int_equal(0, status.rx.crc_errors);
    assert_int_equal(0, status.rx.ack);
    assert_int_equal(0, status.rx.nack_frame_id);
    assert_int_equal(0, status.rx.nack_frame_error);
    assert_int_equal(0, status.rx.resync);
    assert_int_equal(0, status.rx.frame_too_big);
    assert_int_equal(0, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);
}

static void send(struct test_s *self,
                 uint16_t frame_id, uint8_t port_id, uint16_t message_id,
                 uint8_t *msg_buffer, uint32_t msg_size) {
    uint8_t b[EMBC_FRAMER_MAX_SIZE];
    embc_framer_construct_data(b, frame_id, port_id, message_id, msg_buffer, msg_size);
    uint16_t frame_sz = msg_size + EMBC_FRAMER_OVERHEAD_SIZE;
    expect_value(ll_send, buffer_size, frame_sz);
    expect_memory(ll_send, buffer, b, frame_sz);
    assert_int_equal(0, embc_framer_send(self->f, port_id, message_id, msg_buffer, msg_size));
}

#if 0  // todo
static void ll_recv(struct test_s *self,
                    uint16_t frame_id, uint8_t port_id, uint16_t message_id,
                    uint8_t *msg_buffer, uint32_t msg_size) {
    uint8_t b[EMBC_FRAMER_FRAME_MAX_SIZE];
    embc_framer_construct_data(b, frame_id, port_id, message_id, msg_buffer, msg_size);
    uint16_t frame_sz = msg_size + EMBC_FRAMER_OVERHEAD_SIZE;
    embc_framer_ll_recv(self->f, b, frame_sz);
}
#endif

static void ll_recv_link(struct test_s *self, enum embc_framer_frame_type_e frame_type, uint16_t frame_id) {
    uint8_t b[EMBC_FRAMER_LINK_SIZE];
    embc_framer_construct_link(b, frame_type, frame_id);
    embc_framer_ll_recv(self->f, b, EMBC_FRAMER_LINK_SIZE);
}

static void test_send_one_with_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_framer_status_s status;

    send(self, 0, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    ll_recv_link(self, EMBC_FRAMER_FT_ACK_ALL, 0);
    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
}

#if 0

static void test_send_one_with_nack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_framer_status_s status;

    send(self, 0, 1, 2, 3, PAYLOAD1, sizeof(PAYLOAD1));
    respond_with_send_done(self);

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    // NACK causes retransmission
    expect_any(ll_send, buffer_size);
    expect_any(ll_send, buffer);
    respond_with_nack(self, 0, 0, 0);
    respond_with_send_done(self);

    // ACK causes send_done
    expect_send_done(2, 3);
    respond_with_ack(self, 0);

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(2 * (sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE), status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
}


static void test_receive_one(void ** state) {
    struct embc_framer_status_s status;
    struct test_s *self = (struct test_s *) *state;
    assert_non_null(self->ul.recv);

    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(FRAME1), status.rx.total_bytes);
    assert_int_equal(1, status.rx.frames);
    assert_int_equal(0, status.rx.crc_errors);
    assert_int_equal(0, status.rx.resync);
    assert_int_equal(0, status.rx.frame_id_errors);
    assert_int_equal(0, status.rx.frames_missing);
    assert_int_equal(0, status.rx.frame_too_big);
}

static void test_receive_one_many_sof(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t sof[] = {EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1};
    self->ul.recv(self->ul.ul_user_data, sof, sizeof(sof));
    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));
}

static void test_receive_one_initial_garbage(void ** state) {
    struct embc_framer_status_s status;
    struct test_s *self = (struct test_s *) *state;
    uint8_t sof[1024];
    for (uint32_t i = 0; i < sizeof(sof); ++i) {
        sof[i] = EMBC_FRAMER_SOF1;
    }

    // initial garbage contains SOF with long frame length
    uint8_t garbage[] = {0x12, 0x13, EMBC_FRAMER_SOF1, 0xff, EMBC_FRAMER_SOF1, 0x00, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1};
    self->ul.recv(self->ul.ul_user_data, garbage, sizeof(garbage));

    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(garbage) + sizeof(FRAME1), status.rx.total_bytes);
    assert_int_equal(1, status.rx.frames);
    assert_int_equal(1, status.rx.frame_too_big);
}

static void test_receive_two(void ** state) {
    struct embc_framer_status_s status;
    struct test_s *self = (struct test_s *) *state;

    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));

    expect_value(recv_01, msg_size, sizeof(FRAME2_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME2_PAYLOAD, sizeof(FRAME2_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME2, sizeof(FRAME2));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(FRAME1) + sizeof(FRAME2), status.rx.total_bytes);
    assert_int_equal(2, status.rx.frames);
}

static void test_receive_valid_invalid_valid(void ** state) {
    struct embc_framer_status_s status;
    struct test_s *self = (struct test_s *) *state;

    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));

    self->ul.recv(self->ul.ul_user_data, FRAME1_BAD_CRC, sizeof(FRAME1_BAD_CRC));

    expect_value(recv_01, msg_size, sizeof(FRAME2_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME2_PAYLOAD, sizeof(FRAME2_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME2, sizeof(FRAME2));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(FRAME1) + sizeof(FRAME1_BAD_CRC) + sizeof(FRAME2), status.rx.total_bytes);
    assert_int_equal(2, status.rx.frames);
    assert_int_equal(1, status.rx.crc_errors);
    assert_int_equal(1, status.rx.resync);
}

static void test_receive_valid_invalid_valid_extra_sofs(void ** state) {
    struct embc_framer_status_s status;
    struct test_s *self = (struct test_s *) *state;
    uint8_t sof[] = {EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1, EMBC_FRAMER_SOF1};

    self->ul.recv(self->ul.ul_user_data, sof, sizeof(sof));
    expect_value(recv_01, msg_size, sizeof(FRAME1_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME1_PAYLOAD, sizeof(FRAME1_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, FRAME1, sizeof(FRAME1));

    self->ul.recv(self->ul.ul_user_data, sof, sizeof(sof));
    self->ul.recv(self->ul.ul_user_data, FRAME1_BAD_CRC, sizeof(FRAME1_BAD_CRC));

    expect_value(recv_01, msg_size, sizeof(FRAME2_PAYLOAD));
    expect_memory(recv_01, msg_buffer, FRAME2_PAYLOAD, sizeof(FRAME2_PAYLOAD));
    self->ul.recv(self->ul.ul_user_data, sof, sizeof(sof));
    self->ul.recv(self->ul.ul_user_data, FRAME2, sizeof(FRAME2));
    self->ul.recv(self->ul.ul_user_data, sof, sizeof(sof));

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(FRAME1) + sizeof(FRAME1_BAD_CRC) + sizeof(FRAME2) + sizeof(sof) * 4, status.rx.total_bytes);
    assert_int_equal(2, status.rx.frames);
    assert_int_equal(1, status.rx.crc_errors);
    assert_int_equal(1, status.rx.resync);
}
#endif

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initial_state, setup, teardown),
            cmocka_unit_test_setup_teardown(test_send_one_with_ack, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_send_one_with_timeout, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_send_one_with_nack, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one_many_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one_initial_garbage, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_two, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_valid_invalid_valid, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_valid_invalid_valid_extra_sofs, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
