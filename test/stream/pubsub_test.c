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
#include "embc/ec.h"
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
    int dtype = type & EMBC_PUBSUB_DTYPE_MASK;
    check_expected(dtype);
    const char * cstr = value->value.str;
    uint32_t u32 = value->value.u32;
    switch (dtype) {
        case EMBC_PUBSUB_DTYPE_NULL:
            break;
        case EMBC_PUBSUB_DTYPE_STR:     // intentional fall-through
        case EMBC_PUBSUB_DTYPE_JSON:    // intentional fall-through
        case EMBC_PUBSUB_DTYPE_BIN:
            check_expected_ptr(cstr);
            break;
        case EMBC_PUBSUB_DTYPE_U32:
            check_expected(u32);
            break;
        default:
            assert_false(1);
            break;
    }
    return 0;
}

#define expect_pub_null(topic_str)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, dtype, EMBC_PUBSUB_DTYPE_NULL);

#define expect_pub_cstr(topic_str, value)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, dtype, EMBC_PUBSUB_DTYPE_STR); \
    expect_string(on_pub, cstr, value)

#define expect_pub_json(topic_str, value)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, dtype, EMBC_PUBSUB_DTYPE_JSON); \
    expect_string(on_pub, cstr, value)

#define expect_pub_u32(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, dtype, EMBC_PUBSUB_DTYPE_U32); \
    expect_value(on_pub, u32, value)

static void test_cstr(void ** state) {
    (void) state;

    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_cstr_r("hello world"), NULL, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_process(ps);

    // subscribe directly, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));

    // subscribe to parent, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_cstr_r("there"), NULL, NULL));
    expect_pub_cstr("s/hello/world", "there"); // first subscription
    expect_pub_cstr("s/hello/world", "there"); // second subscription
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void test_str(void ** state) {
    (void) state;
    char msg[16] = "hello world";

    struct embc_pubsub_s * ps = embc_pubsub_initialize(128);
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_str(msg), NULL, NULL));
    msg[0] = '!';  // overwrite local data, ensure copy occurred.
    msg[1] = 0;
    expect_pub_cstr("s/hello/world", "hello world");
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}

static void test_str_but_too_big(void ** state) {
    (void) state;
    char msg[] = "hello world, this is a very long message that will exceed the buffer size";

    struct embc_pubsub_s * ps = embc_pubsub_initialize(32);
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));
    assert_int_equal(EMBC_ERROR_PARAMETER_INVALID, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_str(msg), NULL, NULL));
    embc_pubsub_finalize(ps);
}

static void test_str_full_buffer(void ** state) {
    (void) state;
    char msg[] = "0123456789abcde";

    struct embc_pubsub_s * ps = embc_pubsub_initialize(32);
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_str(msg), NULL, NULL));
    assert_int_equal(EMBC_ERROR_NOT_ENOUGH_MEMORY, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_str(msg), NULL, NULL));

    expect_pub_cstr("s/hello/world", msg);
    embc_pubsub_process(ps);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/world", &embc_pubsub_str(msg), NULL, NULL));
    expect_pub_cstr("s/hello/world", msg);
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void test_u32(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), NULL, NULL));
    embc_pubsub_process(ps);

    // subscribe to parent, get retained value
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(7), NULL, NULL));
    expect_pub_u32("s/hello/u32", 7);
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static void test_subscribe_first(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);

    // subscribe to parent, get retained value
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    // publish
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), NULL, NULL));
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
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);

    // publish
    embc_pubsub_register_on_publish(ps, on_publish, NULL);
    expect_function_call(on_publish);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), NULL, NULL));

    embc_pubsub_finalize(ps);
}

static void test_retained_value_query(void ** state) {
    (void) state;
    struct embc_pubsub_value_s value;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_not_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), NULL, NULL));
    embc_pubsub_process(ps);
    assert_int_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(42, value.value.u32);
    embc_pubsub_finalize(ps);
}

static void test_do_not_update_same(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), on_pub, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}

static void test_unsubscribe(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_unsubscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32_r(42), NULL, NULL));
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}

static void test_unretained(void ** state) {
    (void) state;
    struct embc_pubsub_value_s value;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_not_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/hello/u32", &embc_pubsub_u32(42), NULL, NULL));
    embc_pubsub_process(ps);
    assert_int_not_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));

    // no callback, since not retained.
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));
    embc_pubsub_finalize(ps);
}

const char * META1 =
    "{"
        "\"dtype\": \"u32\","
        "\"brief\": \"value1\","
        "\"default\": \"42\","
        "\"options\": [[42, \"v1\"], [43, \"v2\"]],"
        "\"flags\": []"
    "}";

const char * META2 =
    "{"
         "\"dtype\": \"u32\","
         "\"brief\": \"value1\","
         "\"default\": \"42\","
         "\"options\": [[42, \"v1\"], [43, \"v2\"]],"
         "\"flags\": []"
     "}";

const char * META_EMPTY = "";


static void test_meta(void ** state) {
    (void) state;
    struct embc_pubsub_value_s value;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_not_equal(0, embc_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/v1$", &embc_pubsub_cjson_r(META1), NULL, NULL));
    assert_int_equal(0, embc_pubsub_publish(ps, "s/v2$", &embc_pubsub_cjson_r(META2), NULL, NULL));
    embc_pubsub_process(ps);

    // no callback, since metadata not automatically published on subscribe.
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s", on_pub, NULL));

    expect_pub_json("s/v1$", META1);
    expect_pub_json("s/v2$", META2);
    assert_int_equal(0, embc_pubsub_publish(ps, "$", &embc_pubsub_null(), NULL, NULL));
    embc_pubsub_process(ps);

    expect_pub_json("s/v1$", META1);
    expect_pub_json("s/v2$", META2);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/$", &embc_pubsub_null(), NULL, NULL));
    embc_pubsub_process(ps);

    // unknown topic, no expected publish
    assert_int_equal(0, embc_pubsub_publish(ps, "other/$", &embc_pubsub_null(), NULL, NULL));
    embc_pubsub_process(ps);

    // Update metadata (not a normal operation)
    expect_pub_json("s/v2$", META_EMPTY);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/v2$", &embc_pubsub_cjson_r(META_EMPTY), NULL, NULL));
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}

static uint8_t on_pub2(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
    (void) user_data;
    (void) value;
    check_expected_ptr(topic);
    return 0;
}

static void test_meta_request_callback(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize(0);
    assert_int_equal(0, embc_pubsub_publish(ps, "s/v1$", &embc_pubsub_cjson_r(META1), NULL, NULL));
    embc_pubsub_process(ps);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s", on_pub, NULL));
    // expect callbacks to on_pub2, but not on_pub which is subscribed.
    assert_int_equal(0, embc_pubsub_publish(ps, "s/$", &embc_pubsub_null(), on_pub2, NULL));
    expect_string(on_pub2, topic, "s/v1$");
    embc_pubsub_process(ps);
    embc_pubsub_finalize(ps);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_cstr, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str_but_too_big, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str_full_buffer, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32, setup, teardown),
            cmocka_unit_test_setup_teardown(test_subscribe_first, setup, teardown),
            cmocka_unit_test_setup_teardown(test_on_publish_cbk, setup, teardown),
            cmocka_unit_test_setup_teardown(test_retained_value_query, setup, teardown),
            cmocka_unit_test_setup_teardown(test_do_not_update_same, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unsubscribe, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unretained, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_request_callback, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
