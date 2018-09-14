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
#include "embc/collections/intmap.h"
#include "embc.h"
#include <stdio.h>

void app_printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

void dbc_assert(char const *file, unsigned line, const char * msg) {
    (void) file;
    (void) line;
    (void) msg;
}

/* A test case that does nothing and succeeds. */
static void embc_intmap_empty(void **state) {
    (void) state; /* unused */
    embc_size_t value = 42;
    struct embc_intmap_s * h = embc_intmap_new();
    assert_int_equal(0, embc_intmap_length(h));
    assert_int_equal(EMBC_ERROR_NOT_FOUND, embc_intmap_get(h, 10, (void **) &value));
    assert_int_equal(0, value);
    embc_intmap_free(h);
}

static void embc_intmap_put_get_remove_get(void **state) {
    (void) state; /* unused */
    embc_size_t value;
    struct embc_intmap_s * h = embc_intmap_new();
    assert_int_equal(0, embc_intmap_put(h, 10, (void *) 20, (void **) &value));
    assert_int_equal(1, embc_intmap_length(h));
    assert_int_equal(0, embc_intmap_get(h, 10, (void **) &value));
    assert_int_equal(20, value);
    assert_int_equal(0, embc_intmap_remove(h, 10, (void **) &value));
    assert_int_equal(0, embc_intmap_length(h));
    assert_int_equal(EMBC_ERROR_NOT_FOUND, embc_intmap_get(h, 10, 0));
    embc_intmap_free(h);
}

static void embc_intmap_resize(void **state) {
    (void) state; /* unused */
    embc_size_t value_in;
    embc_size_t value_out;
    struct embc_intmap_s * h = embc_intmap_new();
    for (embc_size_t idx = 0; idx < 0x100; ++idx) {
        value_in = idx + 0x1000;
        assert_int_equal(0, embc_intmap_put(h, idx, (void *) value_in, 0));
        assert_int_equal(idx + 1, embc_intmap_length(h));
        assert_int_equal(0, embc_intmap_get(h, idx, (void **) &value_out));
        assert_int_equal(value_in, value_out);
    }
    for (embc_size_t idx = 0; idx < 0x100; ++idx) {
        assert_int_equal(0, embc_intmap_get(h, idx, (void **) &value_out));
        assert_int_equal(idx + 0x1000, value_out);
    }
    embc_intmap_free(h);
}

static void embc_intmap_iterator(void **state) {
    (void) state; /* unused */
    embc_size_t key;
    embc_size_t value;
    struct embc_intmap_iterator_s * iter = 0;
    struct embc_intmap_s * h = embc_intmap_new();
    assert_int_equal(0, embc_intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, embc_intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, embc_intmap_put(h, 3, (void *) 3, 0));
    iter = embc_intmap_iterator_new(h);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    embc_intmap_iterator_free(iter);
    embc_intmap_free(h);
}

static void embc_intmap_iterator_remove_current(void **state) {
    (void) state; /* unused */
    embc_size_t key;
    embc_size_t value;
    struct embc_intmap_iterator_s * iter = 0;
    struct embc_intmap_s * h = embc_intmap_new();
    assert_int_equal(0, embc_intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, embc_intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, embc_intmap_put(h, 3, (void *) 3, 0));
    iter = embc_intmap_iterator_new(h);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, embc_intmap_remove(h, 1, (void **) &value));
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    embc_intmap_iterator_free(iter);
    embc_intmap_free(h);
}

static void embc_intmap_iterator_remove_next(void **state) {
    (void) state; /* unused */
    embc_size_t key;
    embc_size_t value;
    struct embc_intmap_iterator_s * iter = 0;
    struct embc_intmap_s * h = embc_intmap_new();
    assert_int_equal(0, embc_intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, embc_intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, embc_intmap_put(h, 3, (void *) 3, 0));
    iter = embc_intmap_iterator_new(h);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, embc_intmap_remove(h, 0x100001, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, embc_intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    embc_intmap_iterator_free(iter);
    embc_intmap_free(h);
}

EMBC_INTMAP_DECLARE(mysym, embc_size_t)
EMBC_INTMAP_DEFINE_STRUCT(mysym)
EMBC_INTMAP_DEFINE(mysym, embc_size_t)

static void mysym(void **state) {
    (void) state; /* unused */
    embc_size_t key;
    embc_size_t value;
    struct mysym_iterator_s * iter = 0;
    struct mysym_s * h = mysym_new();
    assert_int_equal(0, mysym_put(h, 1, 1, 0));
    assert_int_equal(0, mysym_put(h, 0x100001, 2, 0));
    assert_int_equal(0, mysym_put(h, 3, 3, 0));
    assert_int_equal(3, mysym_length(h));
    iter = mysym_iterator_new(h);
    assert_int_equal(0, mysym_iterator_next(iter, &key, &value));
    assert_int_equal(1, value);
    assert_int_equal(0, mysym_remove(h, 0x100001, &value));
    assert_int_equal(2, value);
    assert_int_equal(0, mysym_iterator_next(iter, &key, &value));
    assert_int_equal(3, value);
    mysym_iterator_free(iter);
    mysym_free(h);
}


int main(void) {
    embc_allocator_set((embc_alloc_fn) malloc, free);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(embc_intmap_empty),
        cmocka_unit_test(embc_intmap_put_get_remove_get),
        cmocka_unit_test(embc_intmap_resize),
        cmocka_unit_test(embc_intmap_iterator),
        cmocka_unit_test(embc_intmap_iterator_remove_current),
        cmocka_unit_test(embc_intmap_iterator_remove_next),
        cmocka_unit_test(mysym),
    };

    embc_log_initialize(app_printf);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
