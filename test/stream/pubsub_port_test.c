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
#include "embc/platform.h"
#include "embc/stream/pubsub_port.h"

struct embc_transport_s {
    int32_t dummy;
};

struct embc_pubsub_s {
    int32_t dummy;
};

struct test_s {
    struct embc_pubsubp_s * s;
    struct embc_transport_s t;
    struct embc_pubsub_s p;
};

int32_t embc_pubsub_subscribe(struct embc_pubsub_s * self, const char * topic,
                              embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    (void) self;
    check_expected_ptr(topic);
    (void) cbk_fn;
    (void) cbk_user_data;
    return 0;
}

#define expect_subscribe(_topic)                            \
    expect_string(embc_pubsub_subscribe, topic, _topic);

int32_t embc_pubsub_unsubscribe(struct embc_pubsub_s * self, const char * topic,
                                embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    (void) self;
    check_expected_ptr(topic);
    (void) cbk_fn;
    (void) cbk_user_data;
    return 0;
}

#define expect_unsubscribe(_topic)                            \
    expect_string(embc_pubsub_unsubscribe, topic, _topic);

int32_t embc_pubsub_meta(struct embc_pubsub_s * self, const char * topic, const char * meta_json) {
    (void) self;
    check_expected_ptr(topic);
    (void) meta_json;
    return 0;
}

#define expect_meta(_topic)                            \
    expect_string(embc_pubsub_meta, topic, _topic);

int32_t embc_pubsub_publish(
        struct embc_pubsub_s * self,
        const char * topic, const struct embc_pubsub_value_s * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data) {
    (void) self;
    (void) src_fn;
    (void) src_user_data;
    int type = value->type;
    int dtype = type & EMBC_PUBSUB_DTYPE_MASK;
    check_expected_ptr(topic);
    check_expected(dtype);
    const char * cstr = value->value.str;
    uint32_t u32 = value->value.u32;
    switch (dtype) {
        case EMBC_PUBSUB_DTYPE_STR: check_expected_ptr(cstr); break;
        case EMBC_PUBSUB_DTYPE_U32: check_expected(u32); break;
        default: break;
    }
    return 0;
}

#define expect_publish_cstr(_topic, _cstr)                  \
    expect_string(embc_pubsub_publish, topic, _topic);               \
    expect_value(embc_pubsub_publish, dtype, EMBC_PUBSUB_DTYPE_STR);  \
    expect_string(embc_pubsub_publish, cstr, _cstr);

#define expect_publish_u32(_topic, _u32)                    \
    expect_string(embc_pubsub_publish, topic, _topic);               \
    expect_value(embc_pubsub_publish, dtype, EMBC_PUBSUB_DTYPE_U32);  \
    expect_value(embc_pubsub_publish, u32, _u32);

int32_t embc_transport_send(struct embc_transport_s * self,
                uint8_t port_id,
                enum embc_transport_seq_e seq,
                uint16_t port_data,
                uint8_t const *msg, uint32_t msg_size) {
    (void) self;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
    return 0;
}

#define expect_send(_port_id, _port_data, _msg, _msg_size)          \
    expect_value(embc_transport_send, port_id, _port_id);                    \
    expect_value(embc_transport_send, seq, EMBC_TRANSPORT_SEQ_SINGLE);       \
    expect_value(embc_transport_send, port_data, _port_data);                \
    expect_value(embc_transport_send, msg_size, _msg_size);                  \
    expect_memory(embc_transport_send, msg, _msg, _msg_size);

int32_t embc_transport_port_register(struct embc_transport_s * self, uint8_t port_id,
                                     embc_transport_event_fn event_fn,
                                     embc_transport_recv_fn recv_fn,
                                     void * user_data) {
    (void) self;
    check_expected_ptr(port_id);
    (void) event_fn;
    (void) recv_fn;
    (void) user_data;
    return 0;
}

static int setup(void ** state) {
    struct test_s * self = embc_alloc_clr(sizeof(struct test_s));
    assert_non_null(self);
    self->s = embc_pubsubp_initialize("s", "s/c/");
    expect_meta("s/c/ev");
    expect_meta("s/c/tx");
    embc_pubsubp_pubsub_register(self->s, &self->p);
    expect_value(embc_transport_port_register, port_id, 2);
    expect_subscribe("s");
    embc_pubsubp_transport_register(self->s, 2, &self->t);

    expect_publish_u32("s/c/ev", EMBC_DL_EV_TX_CONNECTED);
    expect_publish_u32("s/c/tx", 1);
    embc_pubsubp_on_event(self->s, EMBC_DL_EV_TX_CONNECTED);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s * self = (struct test_s *) *state;
    embc_pubsubp_finalize(self->s);
    embc_free(self);
    return 0;
}

static void test_pubsub_to_transport(void ** state) {
    struct test_s * self = (struct test_s *) *state;
    struct embc_pubsub_value_s value;
    value.type = EMBC_PUBSUB_DTYPE_U32;
    value.value.u32 = 42;
    uint8_t msg[] = {3, 'h', '/', 'w', 0, 4, 42, 0, 0, 0};
    expect_send(2, ((uint16_t) value.type) << 8, msg, sizeof(msg));
    assert_int_equal(0, embc_pubsubp_on_update(self->s, "h/w", &value));
}

static void test_transport_to_pubsub(void ** state) {
    struct test_s * self = (struct test_s *) *state;
    struct embc_pubsub_value_s value;
    value.type = EMBC_PUBSUB_DTYPE_U32;
    value.value.u32 = 42;
    uint8_t msg[] = {3 , 'h', '/', 'w', 0, 4, 42, 0, 0, 0};
    expect_publish_u32("h/w", 42);
    embc_pubsubp_on_recv(self->s, 2, EMBC_TRANSPORT_SEQ_SINGLE,  ((uint16_t) value.type) << 8, msg, sizeof(msg));
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_pubsub_to_transport, setup, teardown),
            cmocka_unit_test_setup_teardown(test_transport_to_pubsub, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
