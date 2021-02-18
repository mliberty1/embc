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
#include "embc/stream/port0.h"
#include "embc/stream/transport.h"
#include "embc/stream/pubsub.h"
#include "embc/platform.h"


struct embc_transport_s {
    struct embc_port0_s * p;
    struct embc_pubsub_s * pubsub;
};

static int32_t ll_send(struct embc_transport_s * t,
                       uint8_t port_id,
                       enum embc_transport_seq_e seq,
                       uint16_t port_data,
                       uint8_t const *msg, uint32_t msg_size) {
    (void) t;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
    return 0;
}

#define expect_send(_port_id, _seq, _port_data, _msg_data, _msg_size)  \
    expect_value(ll_send, port_id, _port_id);                          \
    expect_value(ll_send, seq, _seq);                                  \
    expect_value(ll_send, port_data, _port_data);                      \
    expect_value(ll_send, msg_size, _msg_size );                       \
    expect_memory(ll_send, msg, _msg_data, _msg_size);

static int setup(void ** state) {
    (void) state;
    struct embc_transport_s * self = embc_alloc_clr(sizeof(struct embc_transport_s));
    self->pubsub = embc_pubsub_initialize(10000);
    self->p = embc_port0_initialize(EMBC_PORT0_MODE_CLIENT, self, ll_send, self->pubsub, "s/");
    assert_non_null(self->p);
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct embc_transport_s * self = (struct embc_transport_s *) *state;
    embc_port0_finalize(self->p);
    embc_pubsub_finalize(self->pubsub);
    embc_free(self);
    return 0;
}

#define pack_req(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x00) | (((uint16_t) cmd_meta) << 8))

#define pack_rsp(op, cmd_meta) \
     ((EMBC_PORT0_OP_##op & 0x07) | (0x08) | (((uint16_t) cmd_meta) << 8))

static void test_echo_req(void ** state) {
    struct embc_transport_s * self = (struct embc_transport_s *) *state;
    static uint8_t payload[] = {0, 1, 2, 3, 4, 5, 6, 7};
    expect_send(0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(ECHO, 0), payload, sizeof(payload));
    embc_port0_on_recv_cbk(self->p, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_req(ECHO, 0), payload, sizeof(payload));
}

static void test_meta(void ** state) {
    struct embc_transport_s * self = (struct embc_transport_s *) *state;
    static uint8_t req_msg[] = {0};
    static const char * port0 = "{\"type\":\"oam\"}";
    static const char * meta = "{\"type\":\"pubsub\"}";
    assert_int_equal(0, embc_port0_meta_set(self->p, 1, meta));
    assert_ptr_equal(meta, embc_port0_meta_get(self->p, 1));

    // port0
    expect_send(0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(META, 0), port0, strlen(port0) + 1);
    embc_port0_on_recv_cbk(self->p, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_req(META, 0), req_msg, 1);

    // registered port
    expect_send(0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(META, 1), meta, strlen(meta) + 1);
    embc_port0_on_recv_cbk(self->p, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_req(META, 1), req_msg, 1);

    // unregistered port
    expect_send(0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(META, 2), req_msg, 1);
    embc_port0_on_recv_cbk(self->p, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_req(META, 2), req_msg, 1);

    // invalid port
    expect_send(0, EMBC_TRANSPORT_SEQ_SINGLE, pack_rsp(META, 255), req_msg, 1);
    embc_port0_on_recv_cbk(self->p, 0, EMBC_TRANSPORT_SEQ_SINGLE, pack_req(META, 255), req_msg, 1);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_echo_req, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
