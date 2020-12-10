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
#include "embc/memory/buffer.h"
#include "embc/collections/list.h"
#include "embc/crc.h"
#include "embc/time.h"
#include "embc.h"

static uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};

struct test_s {
    struct embc_framer_ul_s ul;
    struct embc_framer_s * f;
    uint8_t * send_buffer;
    uint8_t send_buffer_size;
};

int32_t ll_open(void * ll_user_data,
                struct embc_framer_ul_s * ul_instance) {
    struct test_s * self = (struct test_s *) ll_user_data;
    self->ul = *ul_instance;
    return 0;
}

int32_t ll_close(void * ll_user_data) {
    struct test_s * self = (struct test_s *) ll_user_data;
    memset(&self->ul, 0, sizeof(self->ul));
    return 0;
}

void ll_send(void * ll_user_data, uint8_t * buffer, uint32_t buffer_size) {
    struct test_s * self = (struct test_s *) ll_user_data;
    check_expected(buffer_size);
    check_expected_ptr(buffer);
    self->send_buffer = buffer;
    self->send_buffer_size = buffer_size;
}

void send_done_01(void *user_data, uint8_t port_id, uint8_t message_id) {
    (void) user_data;
    check_expected(port_id);
    check_expected(message_id);
}

static void expect_send_done(
        uint8_t port_id, uint8_t message_id) {
    expect_value(send_done_01, port_id, port_id);
    expect_value(send_done_01, message_id, message_id);
}

void recv_01(void *user_data, uint8_t port_id, uint8_t message_id,
             uint8_t *msg_buffer, uint32_t msg_size) {
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
        .ports = 8,
        .send_frames = 8,
        .event_cbk = event_cbk_01,
        .event_user_data = self
    };

    struct embc_framer_hal_s hal;  // todo

    struct embc_framer_ll_s ll = {
            .ll_user_data = self,
            .open = ll_open,
            .close = ll_close,
            .send = ll_send
    };

    struct embc_framer_port_s frame_port = {
            .user_data = state,
            .send_done_cbk = send_done_01,
            .recv_cbk = recv_01,
    };

    self->f = embc_framer_initialize(&config, &hal, &ll);
    assert_non_null(self->f);
    embc_framer_port_register(self->f, 2, &frame_port);

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
    assert_non_null(self->ul.send_done);
    assert_non_null(self->ul.recv);
    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(1, status.version);
    assert_int_equal(0, status.rx.total_bytes);
    assert_int_equal(0, status.rx.invalid_bytes);
    assert_int_equal(0, status.rx.data_frames);
    assert_int_equal(0, status.rx.crc_errors);
    assert_int_equal(0, status.rx.frame_id_errors);
    assert_int_equal(0, status.rx.frames_missing);
    assert_int_equal(0, status.rx.resync);
    assert_int_equal(0, status.rx.frame_too_big);
    assert_int_equal(0, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);
}

static void send(struct test_s *self,
                 uint16_t frame_id,
                 uint8_t priority, uint8_t port_id, uint8_t message_id,
                 uint8_t *msg_buffer, uint32_t msg_size) {
    uint8_t *msg_buffer_orig = msg_buffer;
    uint32_t msg_size_orig = msg_size;
    uint8_t start = 1;
    uint8_t stop = 0;
    uint16_t sz;
    uint16_t frame_sz;
    uint32_t crc;
    uint8_t b[EMBC_FRAMER_FRAME_MAX_SIZE];

    while (msg_size) {
        b[0] = EMBC_FRAMER_SOF;
        if (msg_size <= EMBC_FRAMER_PAYLOAD_MAX_SIZE) {
            stop = 1;
            sz = msg_size;
        } else {
            sz = EMBC_FRAMER_PAYLOAD_MAX_SIZE;
        }
        b[1] = (start << 4) | (stop << 3) | ((frame_id >> 8) & 0x7);
        b[2] = (uint8_t) (frame_id & 0xff);
        b[3] = sz - 1;
        b[4] = port_id & 0x1f;
        b[5] = message_id;
        memcpy(b + 6, msg_buffer, sz);
        crc = embc_crc32(0, b + 1, sz + 6 - 1);
        b[6 + sz + 0] = crc & 0xff;
        b[6 + sz + 1] = (crc >> 8) & 0xff;
        b[6 + sz + 2] = (crc >> 16) & 0xff;
        b[6 + sz + 3] = (crc >> 24) & 0xff;
        frame_sz = 6 + sz + 4;
        expect_value(ll_send, buffer_size, frame_sz);
        expect_memory(ll_send, buffer, b, frame_sz);
        start = 0;
        msg_buffer += sz;
        msg_size -= sz;
    }
    assert_int_equal(0, embc_framer_send(self->f, priority, port_id, message_id, msg_buffer_orig, msg_size_orig));
}

static void respond_with_ack(struct test_s *self, uint16_t frame_id) {
    uint8_t buffer[EMBC_FRAMER_ACK_SIZE];
    buffer[0] = EMBC_FRAMER_SOF;
    buffer[1] = 0x98 | (uint8_t) ((frame_id >> 8) & 0x7);
    buffer[2] = (uint8_t) (frame_id & 0xff);
    buffer[3] = (uint8_t) (embc_crc32(0, buffer + 1, 2) & 0xff);
    self->ul.recv(self->ul.ul_user_data, buffer, sizeof(buffer));
}

static void respond_with_nack(struct test_s *self, uint16_t frame_id, uint8_t cause, uint16_t cause_frame_id) {
    uint8_t buffer[EMBC_FRAMER_NACK_SIZE];
    buffer[0] = EMBC_FRAMER_SOF;
    buffer[1] = 0xD8 | (uint8_t) ((frame_id >> 8) & 0x7);
    buffer[2] = (uint8_t) (frame_id & 0xff);
    buffer[3] = ((cause & 1) << 7) | (uint8_t) ((cause_frame_id >> 8) & 0x7);
    buffer[4] = (uint8_t) (cause_frame_id & 0xff);
    buffer[5] = (uint8_t) (embc_crc32(0, buffer + 1, 4) & 0xff);
    self->ul.recv(self->ul.ul_user_data, buffer, sizeof(buffer));
}

static void respond_with_send_done(struct test_s *self) {
    uint8_t * send_buffer = self->send_buffer;
    uint32_t send_buffer_size = self->send_buffer_size;
    self->send_buffer = NULL;
    self->send_buffer_size = 0;
    assert_non_null(send_buffer);
    self->ul.send_done(self->ul.ul_user_data, send_buffer, send_buffer_size);
}

static void test_send_one_with_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_framer_status_s status;

    send(self, 0, 1, 2, 3, PAYLOAD1, sizeof(PAYLOAD1));
    respond_with_send_done(self);

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    expect_send_done(2, 3);
    respond_with_ack(self, 0);

    assert_int_equal(0, embc_framer_status_get(self->f, &status));
    assert_int_equal(sizeof(PAYLOAD1) + EMBC_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
}

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


#if 0
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
    uint8_t sof[] = {EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF};
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
        sof[i] = EMBC_FRAMER_SOF;
    }

    // initial garbage contains SOF with long frame length
    uint8_t garbage[] = {0x12, 0x13, EMBC_FRAMER_SOF, 0xff, EMBC_FRAMER_SOF, 0x00, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF};
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
    uint8_t sof[] = {EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF, EMBC_FRAMER_SOF};

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
            cmocka_unit_test_setup_teardown(test_send_one_with_nack, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one_many_sof, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_one_initial_garbage, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_two, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_valid_invalid_valid, setup, teardown),
            //cmocka_unit_test_setup_teardown(test_receive_valid_invalid_valid_extra_sofs, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
