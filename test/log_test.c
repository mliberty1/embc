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

#ifndef LOG_GLOBAL_LEVEL
#define LOG_GLOBAL_LEVEL LOG_LEVEL_ALL
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ALL
#endif

#define LOG_PRINTF(level, format, ...) \
    log_printf_("%c " format "\n", log_level_char[level], __VA_ARGS__);

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdarg.h>
#include <stdio.h>
#include "embc/log.h"


void my_printf(const char * format, ...) {
    char str[256];
    va_list arg;
    va_start(arg, format);
    vsnprintf(str, sizeof(str), format, arg);
    va_end(arg);
    check_expected(str);
}

static void test_logf(void **state) {
    (void) state;
    log_initialize(my_printf);
    expect_string(my_printf, str, "C hello world\n");
    LOGF_CRITICAL("%s %s", "hello", "world");
    expect_string(my_printf, str, "E hello world\n");
    LOGF_ERROR("%s %s", "hello", "world");
    expect_string(my_printf, str, "W hello world\n");
    LOGF_WARNING("%s %s", "hello", "world");
    expect_string(my_printf, str, "N hello world\n");
    LOGF_NOTICE("%s %s", "hello", "world");
    expect_string(my_printf, str, "I hello world\n");
    LOGF_INFO("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    LOGF_DEBUG1("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    LOGF_DEBUG2("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    LOGF_DEBUG3("%s %s", "hello", "world");
}

static void test_logs(void **state) {
    (void) state;
    log_initialize(my_printf);
    expect_string(my_printf, str, "C hello\n");
    LOGS_CRITICAL("hello");
    expect_string(my_printf, str, "E hello\n");
    LOGS_ERROR("hello");
    expect_string(my_printf, str, "W hello\n");
    LOGS_WARNING("hello");
    expect_string(my_printf, str, "N hello\n");
    LOGS_NOTICE("hello");
    expect_string(my_printf, str, "I hello\n");
    LOGS_INFO("hello");
    expect_string(my_printf, str, "D hello\n");
    LOGS_DEBUG1("hello");
    expect_string(my_printf, str, "D hello\n");
    LOGS_DEBUG2("hello");
    expect_string(my_printf, str, "D hello\n");
    LOGS_DEBUG3("hello");
}

static void test_logc(void **state) {
    (void) state;
    log_initialize(my_printf);
    expect_string(my_printf, str, "C h\n");
    LOGC_CRITICAL('h');
    expect_string(my_printf, str, "E h\n");
    LOGC_ERROR('h');
    expect_string(my_printf, str, "W h\n");
    LOGC_WARNING('h');
    expect_string(my_printf, str, "N h\n");
    LOGC_NOTICE('h');
    expect_string(my_printf, str, "I h\n");
    LOGC_INFO('h');
    expect_string(my_printf, str, "D h\n");
    LOGC_DEBUG1('h');
    expect_string(my_printf, str, "D h\n");
    LOGC_DEBUG2('h');
    expect_string(my_printf, str, "D h\n");
    LOGC_DEBUG3('h');
}

static void test_local_levels(void **state) {
    (void) state;
#undef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
    log_initialize(my_printf);
    expect_string(my_printf, str, "D hello\n");
    LOGF_DEBUG("%s", "hello");
    LOGF_DEBUG2("%s", "hello");
    expect_string(my_printf, str, "D hello\n");
    LOGS_DEBUG("hello");
    LOGS_DEBUG2("hello");
    expect_string(my_printf, str, "D h\n");
    LOGC_DEBUG('h');
    LOGC_DEBUG2('h');
#undef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ALL
}

static void test_global_levels(void **state) {
    (void) state;
#undef LOG_GLOBAL_LEVEL
#define LOG_GLOBAL_LEVEL LOG_LEVEL_DEBUG
    log_initialize(my_printf);
    expect_string(my_printf, str, "D hello\n");
    LOGF_DEBUG("%s", "hello");
    LOGF_DEBUG2("%s", "hello");
    expect_string(my_printf, str, "D hello\n");
    LOGS_DEBUG("hello");
    LOGS_DEBUG2("hello");
    expect_string(my_printf, str, "D h\n");
    LOGC_DEBUG('h');
    LOGC_DEBUG2('h');
#undef LOG_GLOBAL_LEVEL
#define LOG_GLOBAL_LEVEL LOG_LEVEL_ALL
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_logf),
            cmocka_unit_test(test_logs),
            cmocka_unit_test(test_logc),
            cmocka_unit_test(test_local_levels),
            cmocka_unit_test(test_global_levels),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
