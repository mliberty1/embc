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
#include <cmocka.h>
#include "embc/time.h"


#define ABS(x) ( (x) < 0 ? -x : x)
#define CLOSE(x, t) ( ABS(x) < (t) )


static void test_constants(void **state) {
    (void) state;
    assert_int_equal(1 << 30, EMBC_TIME_SECOND);
    assert_int_equal((EMBC_TIME_SECOND + 500) / 1000, EMBC_TIME_MILLISECOND);
    assert_int_equal((EMBC_TIME_SECOND + 500000) / 1000000, EMBC_TIME_MICROSECOND);
    assert_int_equal(1, EMBC_TIME_NANOSECOND);
    assert_int_equal(EMBC_TIME_SECOND * 60, EMBC_TIME_MINUTE);
    assert_int_equal(EMBC_TIME_SECOND * 60 * 60, EMBC_TIME_HOUR);
    assert_int_equal(EMBC_TIME_SECOND * 60 * 60 * 24, EMBC_TIME_DAY);
}

static void test_f64(void **state) {
    (void) state;
    assert_true(CLOSE(1.0 - EMBC_TIME_TO_F64(EMBC_TIME_SECOND), 1e-9));
    assert_true(EMBC_TIME_SECOND == EMBC_F64_TO_TIME(1.0));
    assert_true(CLOSE(0.001 - EMBC_TIME_TO_F64(EMBC_TIME_MILLISECOND), 1e-9));
    assert_true(EMBC_TIME_MILLISECOND == EMBC_F64_TO_TIME(0.001));
}

static void test_f32(void **state) {
    (void) state;
    assert_true(CLOSE(1.0f - EMBC_TIME_TO_F32(EMBC_TIME_SECOND), 1e-9));
    assert_true(EMBC_TIME_SECOND == EMBC_F32_TO_TIME(1.0f));
    assert_true(CLOSE(0.001f - EMBC_TIME_TO_F32(EMBC_TIME_MILLISECOND), 1e-9));
    assert_true(EMBC_TIME_MILLISECOND == EMBC_F32_TO_TIME(0.001f));
}

static void test_convert_time_to(void **state) {
    (void) state;
    assert_int_equal(1, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND));
    assert_int_equal(1, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND + 1));
    assert_int_equal(1, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND - 1));
    assert_int_equal(2, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND + EMBC_TIME_SECOND / 2));
    assert_int_equal(1, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND - EMBC_TIME_SECOND / 2));
    assert_int_equal(0, EMBC_TIME_TO_SECONDS(EMBC_TIME_SECOND - EMBC_TIME_SECOND / 2 - 1));
    assert_int_equal(1000, EMBC_TIME_TO_MILLISECONDS(EMBC_TIME_SECOND));
    assert_int_equal(1000000, EMBC_TIME_TO_MICROSECONDS(EMBC_TIME_SECOND));
    assert_int_equal(1000000000, EMBC_TIME_TO_NANOSECONDS(EMBC_TIME_SECOND));
}

static void test_convert_to_time(void **state) {
    (void) state;
    assert_int_equal(EMBC_TIME_SECOND, EMBC_SECONDS_TO_TIME(1));
    assert_int_equal(EMBC_TIME_SECOND, EMBC_MILLISECONDS_TO_TIME(1000));
    assert_int_equal(EMBC_TIME_SECOND, EMBC_MICROSECONDS_TO_TIME(1000000));
    assert_int_equal(EMBC_TIME_SECOND, EMBC_NANOSECONDS_TO_TIME(1000000000));
}

static void test_abs(void **state) {
    (void) state;
    assert_int_equal(EMBC_TIME_SECOND, EMBC_TIME_ABS(EMBC_TIME_SECOND));
    assert_int_equal(EMBC_TIME_SECOND, EMBC_TIME_ABS(-EMBC_TIME_SECOND));
}

static void test_round_nearest(void **state) {
    (void) state;
    assert_int_equal(1, EMBC_TIME_TO_COUNTER(EMBC_TIME_SECOND, 1));
    assert_int_equal(1, EMBC_TIME_TO_COUNTER(EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(1, EMBC_TIME_TO_COUNTER(EMBC_TIME_SECOND - 1, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER(-EMBC_TIME_SECOND, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER(-EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER(-EMBC_TIME_SECOND - 1, 1));
}

static void test_round_zero(void **state) {
    (void) state;
    assert_int_equal(1, EMBC_TIME_TO_COUNTER_RZERO(EMBC_TIME_SECOND, 1));
    assert_int_equal(1, EMBC_TIME_TO_COUNTER_RZERO(EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(0, EMBC_TIME_TO_COUNTER_RZERO(EMBC_TIME_SECOND - 1, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER_RZERO(-EMBC_TIME_SECOND, 1));
    assert_int_equal(0, EMBC_TIME_TO_COUNTER_RZERO(-EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER_RZERO(-EMBC_TIME_SECOND - 1, 1));
}

static void test_round_inf(void **state) {
    (void) state;
    assert_int_equal(1, EMBC_TIME_TO_COUNTER_RINF(EMBC_TIME_SECOND, 1));
    assert_int_equal(2, EMBC_TIME_TO_COUNTER_RINF(EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(1, EMBC_TIME_TO_COUNTER_RINF(EMBC_TIME_SECOND - 1, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER_RINF(-EMBC_TIME_SECOND, 1));
    assert_int_equal(-1, EMBC_TIME_TO_COUNTER_RINF(-EMBC_TIME_SECOND + 1, 1));
    assert_int_equal(-2, EMBC_TIME_TO_COUNTER_RINF(-EMBC_TIME_SECOND - 1, 1));
}

static void test_counter(void **state) {
    (void) state;
    struct embc_time_counter_s counter = embc_time_counter();
    assert_true(counter.frequency >= 1000LL);  // recommendation
    assert_true(counter.frequency < 10000000000LL);  // requirement
}

static void test_utc(void **state) {
    (void) state;
    int64_t t = embc_time_utc();
    // Set to check for 2021, update on 2022 Jan 1.
    int64_t year_offset = 2021 - 2018;
    assert_true(t > (EMBC_TIME_YEAR * year_offset));
    assert_true(t < (EMBC_TIME_YEAR * (year_offset + 1)));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_constants),
            cmocka_unit_test(test_f64),
            cmocka_unit_test(test_f32),
            cmocka_unit_test(test_convert_time_to),
            cmocka_unit_test(test_convert_to_time),
            cmocka_unit_test(test_abs),
            cmocka_unit_test(test_round_nearest),
            cmocka_unit_test(test_round_zero),
            cmocka_unit_test(test_round_inf),
            cmocka_unit_test(test_counter),
            cmocka_unit_test(test_utc),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
