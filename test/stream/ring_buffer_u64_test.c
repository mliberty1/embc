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
#include "embc/stream/ring_buffer_u64.h"


#define SZ (16)


struct test_s {
    struct embc_rb64_s rb;
    uint64_t b[SZ];  // must be at least 16
};

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    embc_rb64_init(&self->rb, self->b, sizeof(self->b) / sizeof(uint64_t));
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void test_initial_state(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_int_equal(0, self->rb.head);
    assert_int_equal(0, self->rb.tail);
    assert_int_equal(SZ, self->rb.buf_size);
    assert_ptr_equal(self->b, self->rb.buf);
    assert_int_equal(0, embc_rb64_size(&self->rb));
    assert_int_equal(SZ - 1, embc_rb64_empty_size(&self->rb));
    assert_int_equal(SZ - 1, embc_rb64_capacity(&self->rb));
    assert_ptr_equal(self->b, embc_rb64_head(&self->rb));
    assert_ptr_equal(self->b, embc_rb64_tail(&self->rb));
}

#define assert_pop_equal(rb, expected_value) { \
    uint64_t x__ = 0;                          \
    assert_true(embc_rb64_pop(rb, &x__));      \
    assert_int_equal(expected_value, x__);     \
}

static void test_push_until_full(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint64_t x = 0;
    embc_rb64_init(&self->rb, self->b, 4);
    assert_true(embc_rb64_push(&self->rb, 1));
    assert_true(embc_rb64_push(&self->rb, 2));
    assert_true(embc_rb64_push(&self->rb, 3));
    assert_false(embc_rb64_push(&self->rb, 4));
    assert_pop_equal(&self->rb, 1);
    assert_true(embc_rb64_push(&self->rb, 4));
    assert_false(embc_rb64_push(&self->rb, 5));
    assert_pop_equal(&self->rb, 2);
    assert_true(embc_rb64_push(&self->rb, 5));
    assert_pop_equal(&self->rb, 3);
    assert_pop_equal(&self->rb, 4);
    assert_pop_equal(&self->rb, 5);
    assert_false(embc_rb64_pop(&self->rb, &x));
}

static void test_clear(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_true(embc_rb64_push(&self->rb, 1));
    assert_int_equal(1, embc_rb64_size(&self->rb));
    embc_rb64_clear(&self->rb);
    assert_int_equal(0, embc_rb64_size(&self->rb));
    assert_int_equal(0, self->rb.head);
    assert_int_equal(0, self->rb.tail);
}

static void test_discard_simple(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_rb64_init(&self->rb, self->b, 4);

    assert_true(embc_rb64_push(&self->rb, 1));
    assert_true(embc_rb64_push(&self->rb, 2));
    assert_true(embc_rb64_push(&self->rb, 3));
    assert_true(embc_rb64_discard(&self->rb, 2));
    assert_pop_equal(&self->rb, 3);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initial_state, setup, teardown),
            cmocka_unit_test_setup_teardown(test_push_until_full, setup, teardown),
            cmocka_unit_test_setup_teardown(test_clear, setup, teardown),
            cmocka_unit_test_setup_teardown(test_discard_simple, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
