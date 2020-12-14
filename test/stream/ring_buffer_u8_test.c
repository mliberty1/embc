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
#include "embc/stream/ring_buffer_u8.h"


#define SZ (16)


struct test_s {
    struct embc_rb8_s rb;
    uint8_t b[SZ];  // must be at least 16
};

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    embc_rb8_init(&self->rb, self->b, sizeof(self->b));
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
    assert_int_equal(0, embc_rb8_size(&self->rb));
    assert_int_equal(SZ - 1, embc_rb8_empty_size(&self->rb));
    assert_int_equal(SZ - 1, embc_rb8_capacity(&self->rb));
}

static void test_push_until_full(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_true(embc_rb8_push(&self->rb, SZ - 1));
    assert_false(embc_rb8_push(&self->rb, 1));
    embc_rb8_pop(&self->rb, 1);
    assert_true(embc_rb8_push(&self->rb, 1));
    assert_false(embc_rb8_push(&self->rb, 1));
}

static void test_insert(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_rb8_init(&self->rb, self->b, 16);
    assert_true(embc_rb8_push(&self->rb, 12));
    assert_true(0 == embc_rb8_insert(&self->rb, 8));
    embc_rb8_pop(&self->rb, 8);
    assert_true(0 == embc_rb8_insert(&self->rb, 8));
    uint8_t * b = embc_rb8_insert(&self->rb, 7);
    assert_non_null(b);
    assert_ptr_equal(b, self->rb.buf);
    assert_int_equal(11, embc_rb8_size(&self->rb));
    assert_int_equal(0, embc_rb8_empty_size(&self->rb));
    embc_rb8_pop(&self->rb, 11);
    assert_int_equal(self->rb.rollover, self->rb.buf_size);
}

static void test_clear(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_true(embc_rb8_push(&self->rb, SZ - 1));
    assert_int_equal(SZ - 1, embc_rb8_size(&self->rb));
    embc_rb8_clear(&self->rb);
    assert_int_equal(0, embc_rb8_size(&self->rb));
    assert_int_equal(0, self->rb.head);
    assert_int_equal(0, self->rb.tail);
}

static void test_rollover_tail(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_rb8_init(&self->rb, self->b, 16);
    assert_ptr_equal(embc_rb8_insert(&self->rb, 12), self->b);
    embc_rb8_pop(&self->rb, 8);
    assert_ptr_equal(embc_rb8_tail(&self->rb), self->b + 8);
    assert_ptr_equal(embc_rb8_insert(&self->rb, 6), self->b);
    embc_rb8_pop(&self->rb, 4);
    assert_ptr_equal(embc_rb8_tail(&self->rb), self->b);
}

static void test_rollover_edge_case(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_rb8_init(&self->rb, self->b, 9);
    assert_ptr_equal(embc_rb8_insert(&self->rb, 8), self->b);
    embc_rb8_pop(&self->rb, 8);
    assert_ptr_equal(embc_rb8_insert(&self->rb, 8), self->b);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initial_state, setup, teardown),
            cmocka_unit_test_setup_teardown(test_push_until_full, setup, teardown),
            cmocka_unit_test_setup_teardown(test_insert, setup, teardown),
            cmocka_unit_test_setup_teardown(test_clear, setup, teardown),
            cmocka_unit_test_setup_teardown(test_rollover_tail, setup, teardown),
            cmocka_unit_test_setup_teardown(test_rollover_edge_case, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
