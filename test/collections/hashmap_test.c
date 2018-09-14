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
#include "embc/collections/hashmap.h"
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

embc_size_t myhash(void * value) {
    return (embc_size_t) (value);
}

bool mycompare(void * self, void * other) {
    return (self == other);
}


/* A test case that does nothing and succeeds. */
static void embc_hashmap_empty(void **state) {
    (void) state; /* unused */
    embc_size_t value = 42;
    struct embc_hashmap_s * h = embc_hashmap_new(myhash, mycompare);
    assert_int_equal(0, embc_hashmap_length(h));
    assert_int_equal(EMBC_ERROR_NOT_FOUND, embc_hashmap_get(h, (void *) 10, (void **) &value));
    assert_int_equal(0, value);
    embc_hashmap_free(h);
}

static void embc_hashmap_put_get_remove_get(void **state) {
    (void) state; /* unused */
    embc_size_t value;
    struct embc_hashmap_s * h = embc_hashmap_new(myhash, mycompare);
    assert_int_equal(0, embc_hashmap_put(h, (void *) 10, (void *) 20, (void **) &value));
    assert_int_equal(1, embc_hashmap_length(h));
    assert_int_equal(0, embc_hashmap_get(h, (void *) 10, (void **) &value));
    assert_int_equal(20, value);
    //todo: assert_int_equal(0, embc_hashmap_remove(h, (void *) 10));
    //todo: assert_int_equal(JETLEX_ERROR_NOT_FOUND, embc_hashmap_get(h, (void *) 10, 0));
    embc_hashmap_free(h);
}

int main(void) {
    embc_allocator_set((embc_alloc_fn) malloc, free);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(embc_hashmap_empty),
        cmocka_unit_test(embc_hashmap_put_get_remove_get),
    };

    embc_log_initialize(app_printf);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
