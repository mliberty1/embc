/*
 * Copyright 2014-2020 Jetperch LLC
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
#include "embc/stream/transport.h"
#include "embc/stream/data_link.h"
#include "embc/platform.h"


uint8_t DATA1[] = {1, 2, 3, 4, 5, 6, 7, 8};


struct embc_dl_s {
    struct embc_dl_api_s api;
    struct embc_transport_s * t;
};

void embc_dl_register_upper_layer(struct embc_dl_s * self, struct embc_dl_api_s const * ul) {
    function_called();
    self->api = *ul;
}

int32_t embc_dl_send(struct embc_dl_s * self, uint32_t metadata,
                     uint8_t const *msg, uint32_t msg_size) {
    (void) self;
    check_expected(metadata);
    check_expected(msg_size);
    check_expected_ptr(msg);
    return 0;
}

#define expect_send(_metadata, _msg_data, _msg_size)    \
    expect_value(embc_dl_send, metadata, _metadata);    \
    expect_value(embc_dl_send, msg_size, _msg_size );   \
    expect_memory(embc_dl_send, msg, _msg_data, _msg_size);

static int setup(void ** state) {
    (void) state;
    struct embc_dl_s * self = embc_alloc_clr(sizeof(struct embc_dl_s));
    expect_function_call(embc_dl_register_upper_layer);
    self->t = embc_transport_initialize(self);
    assert_non_null(self->t);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct embc_dl_s * self = (struct embc_dl_s *) *state;
    embc_transport_finalize(self->t);
    embc_free(self);
    return 0;
}

static void test_send(void ** state) {
    struct embc_dl_s * self = (struct embc_dl_s *) *state;
    expect_send(0x123400, DATA1, sizeof(DATA1));
    assert_int_equal(0, embc_transport_send(self->t, 0, 0x1234, DATA1, sizeof(DATA1)));
    expect_send(0x12341f, DATA1, sizeof(DATA1));
    assert_int_equal(0, embc_transport_send(self->t, 0x1f, 0x1234, DATA1, sizeof(DATA1)));
    assert_int_not_equal(0, embc_transport_send(self->t, EMBC_TRANSPORT_PORT_MAX + 1, 0, DATA1, sizeof(DATA1)));
}

void on_event(void *user_data, enum embc_dl_event_e event) {
    (void) user_data;
    check_expected(event);
}

#define expect_event(_event)    \
    expect_value(on_event, event, _event);

void on_recv(void *user_data, uint8_t port_id,
             enum embc_transport_seq_e seq, uint16_t port_data,
             uint8_t *msg, uint32_t msg_size) {
    (void) user_data;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
}

#define expect_recv(_port_id, _seq, _port_data, _msg_data, _msg_size) \
    expect_value(on_recv, port_id, _port_id);                         \
    expect_value(on_recv, seq, _seq);                                 \
    expect_value(on_recv, port_data, _port_data);                     \
    expect_value(on_recv, msg_size, _msg_size);                       \
    expect_memory(on_recv, msg, _msg_data, _msg_size)

static void test_event(void ** state) {
    struct embc_dl_s * self = (struct embc_dl_s *) *state;
    assert_int_equal(0, embc_transport_port_register(self->t, 1, on_event, on_recv, self));
    expect_event(EMBC_DL_EV_RECEIVED_RESET);
    self->api.event_fn(self->api.user_data, EMBC_DL_EV_RECEIVED_RESET);
}

static void test_recv(void ** state) {
    struct embc_dl_s * self = (struct embc_dl_s *) *state;
    assert_int_equal(0, embc_transport_port_register(self->t, 1, on_event, on_recv, self));

    expect_recv(1, EMBC_TRANSPORT_SEQ_SINGLE, 0x1234, DATA1, sizeof(DATA1));
    self->api.recv_fn(self->api.user_data, 0x1234C1, DATA1, sizeof(DATA1));

    expect_recv(1, EMBC_TRANSPORT_SEQ_START, 0xABCD, DATA1, sizeof(DATA1));
    self->api.recv_fn(self->api.user_data, 0xABCD81, DATA1, sizeof(DATA1));

    expect_recv(1, EMBC_TRANSPORT_SEQ_MIDDLE, 0, DATA1, sizeof(DATA1));
    self->api.recv_fn(self->api.user_data, 0x01, DATA1, sizeof(DATA1));

    expect_recv(1, EMBC_TRANSPORT_SEQ_STOP, 0, DATA1, sizeof(DATA1));
    self->api.recv_fn(self->api.user_data, 0x41, DATA1, sizeof(DATA1));

    // no registered handler, will be dropped
    self->api.recv_fn(self->api.user_data, 0x7, DATA1, sizeof(DATA1));
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_send, setup, teardown),
            cmocka_unit_test_setup_teardown(test_event, setup, teardown),
            cmocka_unit_test_setup_teardown(test_recv, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
