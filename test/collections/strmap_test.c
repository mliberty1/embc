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
#include <stdbool.h>
#include "embc/collections/strmap.h"
#include "embc.h"
#include <stdio.h>

void app_printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

/* A test case that does nothing and succeeds. */
static void strmap_empty(void **state) {
    (void) state; /* unused */
    embc_size_t value = 42;
    struct embc_strmap_s * h = embc_strmap_new();
    assert_int_equal(0, embc_strmap_length(h));
    assert_int_equal(1, embc_strmap_get(h, "hello", (void **) &value));
    assert_int_equal(1, embc_strmap_remove(h, "hello", (void **) &value));
    assert_int_equal(0, value);
    embc_strmap_free(h);
}

static void strmap_put_get_remove_get(void **state) {
    (void) state; /* unused */
    embc_size_t value;
    struct embc_strmap_s * h = embc_strmap_new();
    assert_int_equal(0, embc_strmap_put(h, "hello", (void *) 20, (void **) &value));
    assert_int_equal(1, embc_strmap_length(h));
    assert_int_equal(0, embc_strmap_get(h, "hello", (void **) &value));
    assert_int_equal(20, value);
    assert_int_equal(0, embc_strmap_remove(h, "hello", (void **) &value));
    assert_int_equal(20, value);
    assert_int_equal(0, embc_strmap_length(h));
    assert_int_equal(1, embc_strmap_get(h, "hello", 0));
    embc_strmap_free(h);
}

static void strmap_overwrite(void **state) {
    (void) state; /* unused */
    embc_size_t value;
    struct embc_strmap_s * h = embc_strmap_new();
    assert_int_equal(0, embc_strmap_put(h, "hello", (void *) 20, (void **) &value));
    assert_int_equal(1, embc_strmap_length(h));
    assert_int_equal(0, embc_strmap_put(h, "hello", (void *) 40, (void **) &value));
    assert_int_equal(1, embc_strmap_length(h));
    assert_int_equal(20, value);
    assert_int_equal(0, embc_strmap_put(h, "hello", (void *) 60, (void **) &value));
    assert_int_equal(1, embc_strmap_length(h));
    assert_int_equal(40, value);
    embc_strmap_free(h);
}

int main(void) {
    embc_allocator_set((embc_alloc_fn) malloc, free);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(strmap_empty),
        cmocka_unit_test(strmap_put_get_remove_get),
        cmocka_unit_test(strmap_overwrite),
    };

    log_initialize(app_printf);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
