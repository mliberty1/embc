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
#include "embc/stream/pubsub.h"

#include <stdio.h>
#include <stdlib.h>


static int setup(void ** state) {
    (void) state;
    return 0;
}

static int teardown(void ** state) {
    (void) state;
    fflush(stdout);
    fflush(stderr);
    return 0;
}

static uint8_t on_pub(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
    (void) user_data;
    check_expected_ptr(topic);
    int type = value->type;
    check_expected(type);
    const char * cstr = value->value.cstr;
    uint32_t u32 = value->value.u32;
    switch (type) {
        case EMBC_PUBSUB_TYPE_CSTR:
            check_expected_ptr(cstr);
            break;
        case EMBC_PUBSUB_TYPE_U32:
            check_expected(u32);
            break;
        default:
            assert_false(1);
            break;
    }
    return 0;
}

#define expect_pub_cstr(topic_str, value)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, EMBC_PUBSUB_TYPE_CSTR); \
    expect_string(on_pub, cstr, value)

#define expect_pub_u32(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, EMBC_PUBSUB_TYPE_U32); \
    expect_value(on_pub, u32, value)

static void on_complete(void * user_data,
                        const char * topic, const struct embc_pubsub_value_s * value,
                        uint8_t status) {
    (void) user_data;
    check_expected_ptr(topic);
    assert_int_equal(EMBC_PUBSUB_TYPE_U32, value->type); // only support u32 for this test
    uint32_t u32 = value->value.u32;
    check_expected(u32);
    check_expected(status);
}

#define expect_u32_complete(topic_str, value, status_) \
    expect_string(on_complete, topic, topic_str);           \
    expect_value(on_complete, u32, value);                  \
    expect_value(on_complete, status, status_);

static void test_cstr(void ** state) {
    (void) state;

    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_publish_cstr(ps, "s/hello/world", "hello world", NULL, NULL, NULL, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_process(ps);

    // subscribe directly, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));

    // subscribe to parent, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish_cstr(ps, "s/hello/world", "there", NULL, NULL, NULL, NULL));
    expect_pub_cstr("s/hello/world", "there"); // first subscription
    expect_pub_cstr("s/hello/world", "there"); // second subscription
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void test_u32(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, NULL, NULL, NULL, NULL));
    embc_pubsub_process(ps);

    // subscribe to parent, get retained value
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 7, NULL, NULL, NULL, NULL));
    expect_pub_u32("s/hello/u32", 7);
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void test_subscribe_first(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();

    // subscribe to parent, get retained value
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, NULL, NULL, NULL, NULL));
    expect_pub_u32("s/hello/u32", 42);
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void on_publish(void * user_data) {
    (void) user_data;
    function_called();
}

static void test_on_publish_cbk(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();

    // publish
    embc_pubsub_register_on_publish(ps, on_publish, NULL);
    expect_function_call(on_publish);
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, NULL, NULL, NULL, NULL));

    embc_pubsub_finalize(ps);
}

static void test_retained_value_query(void ** state) {
    (void) state;
    struct embc_pubsub_value_s value;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_int_not_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, NULL, NULL, NULL, NULL));
    embc_pubsub_process(ps);
    assert_int_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(42, value.value.u32);
    embc_pubsub_finalize(ps);
}

static void test_do_not_update_same(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));
    expect_u32_complete("s/hello/u32", 42, 0);
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, on_pub, NULL, on_complete, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}

static void test_unsubscribe(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_unsubscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 42, NULL, NULL, NULL, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}


int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_cstr, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32, setup, teardown),
            cmocka_unit_test_setup_teardown(test_subscribe_first, setup, teardown),
            cmocka_unit_test_setup_teardown(test_on_publish_cbk, setup, teardown),
            cmocka_unit_test_setup_teardown(test_retained_value_query, setup, teardown),
            cmocka_unit_test_setup_teardown(test_do_not_update_same, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unsubscribe, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
