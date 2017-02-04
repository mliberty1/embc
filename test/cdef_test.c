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
#include <stdint.h>
#include <cmocka.h>
#include "embc/cdef.h"
#include "embc.h"


EMBC_STATIC_ASSERT(1 == 1, identity_holds);
// failing EMBC_STATIC_ASSERT in cdef_static_assert_test.c


static int32_t return_on_error(int32_t x) {
    EMBC_RETURN_ON_ERROR(x);
    return 0;
}

static int32_t return_on_error_msg(int32_t x) {
    EMBC_RETURN_ON_ERROR_MSG(x, "message");
    return 0;
}

static int32_t exit_on_error(int32_t x) {
    EMBC_EXIT_ON_ERROR(x);
    return 0;
exit:
    return 3;
}

static void test_array_size(void **state) {
    (void) state;
    int8_t i8[9];
    int16_t i16[7];
    int32_t i32[5];
    int64_t i64[3];
    assert_int_equal(9, EMBC_ARRAY_SIZE(i8));
    assert_int_equal(7, EMBC_ARRAY_SIZE(i16));
    assert_int_equal(5, EMBC_ARRAY_SIZE(i32));
    assert_int_equal(3, EMBC_ARRAY_SIZE(i64));
}

static void test_return_on_error(void **state) {
    (void) state;
    assert_int_equal(0, return_on_error(0));
    assert_int_equal(1, return_on_error(1));
}

static void test_return_on_error_msg(void **state) {
    (void) state;
    assert_int_equal(0, return_on_error_msg(0));
    assert_int_equal(1, return_on_error_msg(1));
}

static void test_exit_on_error(void **state) {
    (void) state;
    assert_int_equal(0, exit_on_error(0));
    assert_int_equal(3, exit_on_error(1));
}

static void test_restrict_to_range(void **state) {
    (void) state;
    assert_int_equal(1, EMBC_RESTRICT_TO_RANGE(-10, 1, 5));
    assert_int_equal(1, EMBC_RESTRICT_TO_RANGE(0, 1, 5));
    assert_int_equal(1, EMBC_RESTRICT_TO_RANGE(1, 1, 5));
    assert_int_equal(5, EMBC_RESTRICT_TO_RANGE(5, 1, 5));
    assert_int_equal(5, EMBC_RESTRICT_TO_RANGE(6, 1, 5));
    assert_int_equal(5, EMBC_RESTRICT_TO_RANGE(100, 1, 5));
}

static void test_signum(void **state) {
    (void) state;
    assert_int_equal(0, EMBC_SIGNUM(0));
    assert_int_equal(-1, EMBC_SIGNUM(-10));
    assert_int_equal(1, EMBC_SIGNUM(10));
}

static void test_round_up_to_multiple(void **state) {
    (void) state;
    assert_int_equal(0, EMBC_ROUND_UP_TO_MULTIPLE(0, 5));
    assert_int_equal(5, EMBC_ROUND_UP_TO_MULTIPLE(1, 5));
    assert_int_equal(5, EMBC_ROUND_UP_TO_MULTIPLE(4, 5));
    assert_int_equal(5, EMBC_ROUND_UP_TO_MULTIPLE(5, 5));
    assert_int_equal(10, EMBC_ROUND_UP_TO_MULTIPLE(6, 5));
    assert_int_equal(-5, EMBC_ROUND_UP_TO_MULTIPLE(-1, 5));
    assert_int_equal(-5, EMBC_ROUND_UP_TO_MULTIPLE(-4, 5));
    assert_int_equal(-5, EMBC_ROUND_UP_TO_MULTIPLE(-5, 5));
    assert_int_equal(-10, EMBC_ROUND_UP_TO_MULTIPLE(-6, 5));
    assert_int_equal(-10, EMBC_ROUND_UP_TO_MULTIPLE(-10, 5));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_array_size),
            cmocka_unit_test(test_return_on_error),
            cmocka_unit_test(test_return_on_error_msg),
            cmocka_unit_test(test_exit_on_error),
            cmocka_unit_test(test_restrict_to_range),
            cmocka_unit_test(test_signum),
            cmocka_unit_test(test_round_up_to_multiple),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
