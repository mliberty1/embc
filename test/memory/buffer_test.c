/*
 * Copyright 2017 Jetperch LLC
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
#include "embc/memory/buffer.h"
#include "embc/cdef.h"

embc_size_t SIZES1[] = {8, 7, 6, 5, 4, 3, 2, 1};


static void init_alloc_free_one(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 10);
    assert_non_null(b);
    assert_int_equal(32, embc_buffer_capacity(b));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void init_alloc_until_empty(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b[8];
    for (int i = 0; i < 8; ++i) {
        b[i] = embc_buffer_alloc(a, 30);
    }
    assert_ptr_not_equal(b[0], b[1]);
    expect_assert_failure(embc_buffer_alloc(a, 30));
    for (int i = 0; i < 8; ++i) {
        embc_buffer_free(b[i]);
        assert_ptr_equal(b[i], embc_buffer_alloc(a, 30));
    }
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void init_alloc_unsafe_until_empty(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b[8];
    for (int i = 0; i < 8; ++i) {
        b[i] = embc_buffer_alloc_unsafe(a, 30);
    }
    assert_ptr_not_equal(b[0], b[1]);
    assert_ptr_equal(0, embc_buffer_alloc_unsafe(a, 30));
    for (int i = 0; i < 8; ++i) {
        embc_buffer_free(b[i]);
        assert_ptr_equal(b[i], embc_buffer_alloc_unsafe(a, 30));
    }
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void init_alloc_free_around(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b;
    for (int i = 0; i < 16; ++i) {
        b = embc_buffer_alloc(a, 30);
        embc_buffer_free(b);
    }
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}


static void buffer_write_str(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    assert_int_equal(32, embc_buffer_capacity(b));
    assert_int_equal(0, embc_buffer_length(b));
    assert_int_equal(32, embc_buffer_write_remaining(b));
    assert_int_equal(0, embc_buffer_read_remaining(b));
    assert_int_equal(0, embc_buffer_cursor_get(b));
    embc_buffer_write_str_truncate(b, "hello");
    assert_int_equal(5, embc_buffer_length(b));
    assert_int_equal(27, embc_buffer_write_remaining(b));
    assert_int_equal(0, embc_buffer_read_remaining(b));
    assert_int_equal(5, embc_buffer_cursor_get(b));
    embc_buffer_write_str(b, " world!");
    assert_memory_equal("hello world!", b->data, 12);
    assert_false(embc_buffer_write_str_truncate(b, "this is a very long message which will be successfully truncated"));
    assert_int_equal(32, embc_buffer_length(b));
    assert_int_equal(0, embc_buffer_write_remaining(b));
    assert_int_equal(0, embc_buffer_read_remaining(b));
    assert_int_equal(32, embc_buffer_cursor_get(b));
    assert_false(embc_buffer_write_str_truncate(b, "!"));
    expect_assert_failure(embc_buffer_write_str(b, "!"));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_little_endian(void **state) {
    (void) state;
    uint8_t expect[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write_u8(b, (uint8_t) 0x01U);
    embc_buffer_write_u16_le(b, (uint16_t) 0x0302U);
    embc_buffer_write_u32_le(b, (uint32_t) 0x07060504U);
    embc_buffer_write_u64_le(b, (uint64_t) 0x0f0e0d0c0b0a0908U);
    assert_memory_equal(expect, b->data, sizeof(expect));
    embc_buffer_cursor_set(b, 0);
    assert_int_equal(0x01U, embc_buffer_read_u8(b));
    assert_int_equal(0x0302U, embc_buffer_read_u16_le(b));
    assert_int_equal(0x07060504U, embc_buffer_read_u32_le(b));
    assert_int_equal(0x0f0e0d0c0b0a0908U, embc_buffer_read_u64_le(b));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_big_endian(void **state) {
    (void) state;
    uint8_t expect[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write_u8(b, (uint8_t) 0x01U);
    embc_buffer_write_u16_be(b, (uint16_t) 0x0203U);
    embc_buffer_write_u32_be(b, (uint32_t) 0x04050607U);
    embc_buffer_write_u64_be(b, (uint64_t) 0x08090a0b0c0d0e0fU);
    assert_memory_equal(expect, b->data, sizeof(expect));
    embc_buffer_cursor_set(b, 0);
    assert_int_equal(0x01U, embc_buffer_read_u8(b));
    assert_int_equal(0x0203U, embc_buffer_read_u16_be(b));
    assert_int_equal(0x04050607U, embc_buffer_read_u32_be(b));
    assert_int_equal(0x08090a0b0c0d0e0fU, embc_buffer_read_u64_be(b));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_write_read(void **state) {
    (void) state;
    uint8_t const wr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t rd[sizeof(wr)];
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write(b, wr, sizeof(wr));
    embc_buffer_cursor_set(b, 0);
    assert_int_equal(sizeof(wr), embc_buffer_length(b));
    assert_memory_equal(wr, b->data, sizeof(wr));
    embc_buffer_read(b, rd, sizeof(wr));
    assert_memory_equal(wr, rd, sizeof(wr));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_write_past_end(void **state) {
    (void) state;
    uint8_t const wr[32] = {0};
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write(b, wr, sizeof(wr));
    expect_assert_failure(embc_buffer_write(b, wr, 1));
    expect_assert_failure(embc_buffer_write_u8(b, 1));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_read_past_end(void **state) {
    (void) state;
    uint8_t rd[32];
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write_str_truncate(b, "hello");
    expect_assert_failure(embc_buffer_read(b, rd, 1));
    expect_assert_failure(embc_buffer_read_u8(b));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_overwrite(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    embc_buffer_write_str_truncate(b, "hello great world!");
    assert_int_equal(18, embc_buffer_length(b));
    assert_int_equal(14, embc_buffer_write_remaining(b));
    embc_buffer_cursor_set(b, 6);
    assert_int_equal(26, embc_buffer_write_remaining(b));
    embc_buffer_write_str_truncate(b, "weird");
    assert_int_equal(18, embc_buffer_length(b));
    assert_int_equal(21, embc_buffer_write_remaining(b));
    assert_int_equal(7, embc_buffer_read_remaining(b));
    assert_int_equal(11, embc_buffer_cursor_get(b));
    assert_memory_equal("hello weird world!", b->data, 18);
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_reserve(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b = embc_buffer_alloc(a, 30);
    b->reserve = 27; // leaves 5 bytes
    embc_buffer_write_str_truncate(b, "hello world!");
    assert_int_equal(5, embc_buffer_length(b));
    expect_assert_failure(embc_buffer_write_u8(b, 1));
    b->reserve = 0;
    embc_buffer_write_u8(b, 1);
    assert_int_equal(6, embc_buffer_length(b));
    embc_buffer_free(b);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

static void buffer_copy(void **state) {
    (void) state;
    struct embc_buffer_allocator_s * a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    struct embc_buffer_s * b1 = embc_buffer_alloc(a, 30);
    struct embc_buffer_s * b2 = embc_buffer_alloc(a, 30);
    embc_buffer_write_str(b1, "hello world!");
    embc_buffer_cursor_set(b1, 6);
    embc_buffer_copy(b2, b1, embc_buffer_read_remaining(b1));
    assert_int_equal(6, embc_buffer_length(b2));
    assert_memory_equal("world!", b2->data, 6);
    embc_buffer_free(b1);
    embc_buffer_free(b2);
    embc_buffer_allocator_finalize(a);
    embc_free(a);
}

struct erase_s {
    struct embc_buffer_allocator_s * a;
    struct embc_buffer_s * b;
};

static int setup_erase(void ** state) {
    struct erase_s * self = (struct erase_s *) test_calloc(1, sizeof(struct erase_s));
    self->a = embc_buffer_allocator_new(SIZES1, EMBC_ARRAY_SIZE(SIZES1));
    self->b = embc_buffer_alloc(self->a, 30);
    *state = self;
    return 0;
}

static int teardown_erase(void ** state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_free(self->b);
    embc_buffer_allocator_finalize(self->a);
    embc_free(self->a);
    test_free(self);
    return 0;
}

static void buffer_erase_cursor_at_end(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    embc_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, embc_buffer_length(self->b));
    assert_int_equal(20, embc_buffer_write_remaining(self->b));
    assert_int_equal(0, embc_buffer_read_remaining(self->b));
    assert_int_equal(12, embc_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_cursor_in_middle(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    embc_buffer_cursor_set(self->b, 8);
    embc_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, embc_buffer_length(self->b));
    assert_int_equal(26, embc_buffer_write_remaining(self->b));
    assert_int_equal(6, embc_buffer_read_remaining(self->b));
    assert_int_equal(6, embc_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_cursor_before(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    embc_buffer_cursor_set(self->b, 1);
    embc_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, embc_buffer_length(self->b));
    assert_int_equal(31, embc_buffer_write_remaining(self->b));
    assert_int_equal(11, embc_buffer_read_remaining(self->b));
    assert_int_equal(1, embc_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_invalid(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    expect_assert_failure(embc_buffer_erase(self->b, 6, 30));
    embc_buffer_erase(self->b, 6, 6);
    assert_int_equal(17, embc_buffer_length(self->b));
    expect_assert_failure(embc_buffer_erase(self->b, 6, -1));
    expect_assert_failure(embc_buffer_erase(self->b, -1, 6));
}

static void buffer_erase_all(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    embc_buffer_erase(self->b, 0, embc_buffer_length(self->b));
    assert_int_equal(0, embc_buffer_length(self->b));
    assert_int_equal(0, embc_buffer_cursor_get(self->b));
}

static void buffer_erase_to_length(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    embc_buffer_write_str(self->b, "hello good world!");
    embc_buffer_erase(self->b, 5, self->b->length);
    assert_int_equal(5, embc_buffer_length(self->b));
    assert_int_equal(5, embc_buffer_cursor_get(self->b));
}

static void basic_test(struct embc_buffer_s * b) {
    assert_int_equal(32, embc_buffer_capacity(b));
    assert_int_equal(0, embc_buffer_length(b));
    assert_int_equal(0, embc_buffer_cursor_get(b));
    embc_buffer_write_str_truncate(b, "hello world!");
    assert_memory_equal("hello world!", b->data, 12);
}

static void buffer_static_declare(void **state) {
    (void) state;
    EMBC_BUFFER_STATIC_DECLARE(b, 32);
    EMBC_BUFFER_STATIC_INITIALIZE(b);
    basic_test(&b);
}

static void buffer_static_define(void **state) {
    (void) state;
    EMBC_BUFFER_STATIC_DEFINE(b, 32);
    basic_test(&b);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(init_alloc_free_one),
            cmocka_unit_test(init_alloc_until_empty),
            cmocka_unit_test(init_alloc_unsafe_until_empty),
            cmocka_unit_test(init_alloc_free_around),
            cmocka_unit_test(buffer_write_str),
            cmocka_unit_test(buffer_little_endian),
            cmocka_unit_test(buffer_big_endian),
            cmocka_unit_test(buffer_write_read),
            cmocka_unit_test(buffer_write_past_end),
            cmocka_unit_test(buffer_read_past_end),
            cmocka_unit_test(buffer_overwrite),
            cmocka_unit_test(buffer_reserve),
            cmocka_unit_test(buffer_copy),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_at_end, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_in_middle, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_before, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_invalid, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_all, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_to_length, setup_erase, teardown_erase),
            cmocka_unit_test(buffer_static_declare),
            cmocka_unit_test(buffer_static_define),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
