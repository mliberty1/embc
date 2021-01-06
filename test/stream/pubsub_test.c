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

const struct embc_pubsub_meta_s meta_hello_world = {
        .topic = "s/hello/world",
        .brief = "Hello World",
        .detail = "Yes, it really says Hello World",
        .type = EMBC_PUBSUB_TYPE_CSTR,
};

const struct embc_pubsub_meta_s meta_u32 = {
        .topic = "s/hello/u32",
        .brief = "Hello, this is a u32 value",
        .detail = "Yes, it really is a u32 value",
        .type = EMBC_PUBSUB_TYPE_U32,
};

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

static void on_pub(void * user_data, const char * topic, const struct embc_pubsub_value_s * value) {
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
}

static void expect_pub_cstr(const char * topic, const char * value) {
    expect_string(on_pub, topic, topic);
    expect_value(on_pub, type, EMBC_PUBSUB_TYPE_CSTR);
    expect_string(on_pub, cstr, value);
}

static void expect_pub_u32(const char * topic, uint32_t value) {
    expect_string(on_pub, topic, topic);
    expect_value(on_pub, type, EMBC_PUBSUB_TYPE_U32);
    expect_value(on_pub, u32, value);
}

static void test_cstr(void ** state) {
    (void) state;

    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_non_null(ps);
    assert_int_equal(0, embc_pubsub_register_cstr(ps, &meta_hello_world, "hello world"));

    // subscribe directly
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello/world", on_pub, NULL));

    // subscribe to parent
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    embc_pubsub_finalize(ps);
}

static void test_u32(void ** state) {
    (void) state;
    struct embc_pubsub_s * ps = embc_pubsub_initialize();
    assert_int_equal(0, embc_pubsub_register_u32(ps, &meta_u32, 42));

    // subscribe to parent
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, embc_pubsub_subscribe(ps, "s/hello", on_pub, NULL));

    assert_int_equal(0, embc_pubsub_publish_u32(ps, "s/hello/u32", 7));
    expect_pub_u32("s/hello/u32", 7);
    embc_pubsub_process(ps);

    embc_pubsub_finalize(ps);
}


int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_cstr, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
