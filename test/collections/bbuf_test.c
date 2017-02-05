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
#include "embc/collections/bbuf.h"
#include "embc.h"


#ifdef BBUF_UNSAFE

static void u8_unsafe(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u8(&p1, 42);
    assert_int_equal(b + 1, p1);
    assert_int_equal(42, *b);
    assert_int_equal(42, bbuf_unsafe_decode_u8(&p2));
    assert_int_equal(p1, p2);
}

static void u16_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u16_be(&p1, 0x1122);
    assert_int_equal(b + 2, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x1122, bbuf_unsafe_decode_u16_be(&p2));
    assert_int_equal(p1, p2);
}

static void u16_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u16_le(&p1, 0x1122);
    assert_int_equal(b + 2, p1);
    assert_int_equal(0x22, b[0]);
    assert_int_equal(0x11, b[1]);
    assert_int_equal(0x1122, bbuf_unsafe_decode_u16_le(&p2));
    assert_int_equal(p1, p2);
}

static void u32_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u32_be(&p1, 0x11223344);
    assert_int_equal(b + 4, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x33, b[2]);
    assert_int_equal(0x44, b[3]);
    assert_int_equal(0x11223344, bbuf_unsafe_decode_u32_be(&p2));
    assert_int_equal(p1, p2);
}

static void u32_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u32_le(&p1, 0x11223344);
    assert_int_equal(b + 4, p1);
    assert_int_equal(0x44, b[0]);
    assert_int_equal(0x33, b[1]);
    assert_int_equal(0x22, b[2]);
    assert_int_equal(0x11, b[3]);
    assert_int_equal(0x11223344, bbuf_unsafe_decode_u32_le(&p2));
    assert_int_equal(p1, p2);
}

static void u64_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u64_be(&p1, 0x1122334455667788llu);
    assert_int_equal(b + 8, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x33, b[2]);
    assert_int_equal(0x44, b[3]);
    assert_int_equal(0x55, b[4]);
    assert_int_equal(0x66, b[5]);
    assert_int_equal(0x77, b[6]);
    assert_int_equal(0x88, b[7]);
    assert_int_equal(0x1122334455667788llu, bbuf_unsafe_decode_u64_be(&p2));
    assert_int_equal(p1, p2);
}

static void u64_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    bbuf_unsafe_encode_u64_le(&p1, 0x1122334455667788llu);
    assert_int_equal(b + 8, p1);
    assert_int_equal(0x88, b[0]);
    assert_int_equal(0x77, b[1]);
    assert_int_equal(0x66, b[2]);
    assert_int_equal(0x55, b[3]);
    assert_int_equal(0x44, b[4]);
    assert_int_equal(0x33, b[5]);
    assert_int_equal(0x22, b[6]);
    assert_int_equal(0x11, b[7]);
    assert_int_equal(0x1122334455667788llu, bbuf_unsafe_decode_u64_le(&p2));
    assert_int_equal(p1, p2);
}

#endif /* BBUF_UNSAFE */

/* ------------------------------------------------------------------------- */

static void buf_s_define(void **state) {
    (void) state;
    BBUF_DEFINE(b, 16);
    assert_int_equal(b_mem_, b.buf_start);
    assert_int_equal(sizeof(b_mem_), b.buf_end - b.buf_start);
    assert_int_equal(b.buf_start, b.cursor);
    assert_int_equal(b.buf_start, b.end);

    assert_int_equal(0, bbuf_size(&b));
    assert_int_equal(0, bbuf_size(0));
    assert_int_equal(16, bbuf_capacity(&b));
    assert_int_equal(0, bbuf_capacity(0));
}

static void initialize(void **state) {
    (void) state;
    uint8_t data[16];
    struct bbuf_u8_s b;
    bbuf_initialize(&b, data, sizeof(data));
    assert_int_equal(0, bbuf_size(&b));
    assert_int_equal(0, bbuf_tell(&b));
    bbuf_seek(&b, 0);
}

static void enclose(void **state) {
    (void) state;
    uint8_t data[12] = "hello world";
    struct bbuf_u8_s b;
    bbuf_enclose(&b, data, sizeof(data));
    assert_int_equal(sizeof(data), bbuf_size(&b));
    assert_int_equal(0xc, bbuf_tell(&b));
    bbuf_seek(&b, 0);
}

static void u8(void **state) {
    (void) state;
    uint8_t value = 0;
    BBUF_DEFINE(b, 16);
    assert_int_equal(0, bbuf_size(&b));
    assert_int_equal(0, bbuf_encode_u8(&b, 42));
    assert_int_equal(1, bbuf_size(&b));
    assert_int_equal(42, b.buf_start[0]);
    assert_int_equal(b.buf_start + 1, b.cursor);
    assert_int_equal(b.buf_start + 1, b.end);

    assert_int_equal(1, bbuf_tell(&b));
    bbuf_seek(&b, 0);

    assert_int_equal(0, bbuf_decode_u8(&b, &value));
    assert_int_equal(42, value);
}

static void u8a(void **state) {
    (void) state;
    char const str_in[] = "hello";
    char str_out[6] = {0, 0, 0, 0, 0, 0};
    BBUF_DEFINE(b, 16);
    assert_int_equal(0, bbuf_encode_u8a(&b, (uint8_t const *) str_in, sizeof(str_in) - 1));
    assert_int_equal(5, bbuf_size(&b));
    assert_int_equal(5, bbuf_tell(&b));
    bbuf_seek(&b, 0);
    assert_int_equal(0, bbuf_decode_u8a(&b, 5, (uint8_t *) str_out));
    assert_string_equal(str_in, str_out);
    assert_int_equal(5, bbuf_tell(&b));
}

static void safe_alloc_free(void **state) {
    (void) state;
    uint32_t v;
    struct bbuf_u8_s * b = bbuf_alloc(8);
    assert_true(b);
    assert_int_equal(0, bbuf_encode_u32_be(b, 0x11223344));

    assert_int_equal(4, bbuf_tell(b));
    bbuf_seek(b, 0);

    assert_int_equal(0, bbuf_decode_u32_be(b, &v));
    assert_int_equal(0x11223344, v);

    bbuf_free(b);
}

static void safe_alloc_string_free(void **state) {
    (void) state;
    uint8_t p;
    char str[] = "hello world";
    char * str_ptr = str;
    char * str_end = str + sizeof(str) - 1;
    struct bbuf_u8_s * b = bbuf_alloc_from_string(str);
    assert_true(b);
    while (str_ptr < str_end) {
        assert_int_equal(0, bbuf_decode_u8(b, &p));
        assert_int_equal(*str_ptr, (char) p);
        ++str_ptr;
    }
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(b, &p));
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(b, &p));
    bbuf_free(b);
}

static void safe_alloc_buffer_free(void **state) {
    (void) state;
    uint8_t p;
    char const str[] = "hello world";
    struct bbuf_u8_s * b = bbuf_alloc_from_buffer((uint8_t const *) str, sizeof(str));
    assert_true(b);
    assert_int_equal(sizeof(str), bbuf_size(b));
    for (size_t i = 0; i < sizeof(str); ++i) {
        assert_int_equal(0, bbuf_decode_u8(b, &p));
        assert_int_equal(str[i], (char) p);
    }
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(b, &p));
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(b, &p));
    bbuf_free(b);
}

static void u8_fill(void **state) {
    (void) state;
    uint8_t v;
    BBUF_DEFINE(b, 4);
    assert_int_equal(0, bbuf_encode_u8(&b, 42));
    assert_int_equal(0, bbuf_encode_u8(&b, 43));
    assert_int_equal(0, bbuf_encode_u8(&b, 44));
    assert_int_equal(0, bbuf_encode_u8(&b, 45));

    assert_int_equal(b.buf_end, b.cursor);
    assert_int_equal(b.buf_end, b.end);

    assert_int_equal(EMBC_ERROR_FULL, bbuf_encode_u8(&b, 46));
    assert_int_equal(EMBC_ERROR_FULL, bbuf_encode_u8(&b, 46));

    bbuf_seek(&b, 0);
    assert_int_equal(0, bbuf_decode_u8(&b, &v));  assert_int_equal(42, v);
    assert_int_equal(0, bbuf_decode_u8(&b, &v));  assert_int_equal(43, v);
    assert_int_equal(0, bbuf_decode_u8(&b, &v));  assert_int_equal(44, v);
    assert_int_equal(0, bbuf_decode_u8(&b, &v));  assert_int_equal(45, v);
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(&b, &v));
    assert_int_equal(EMBC_ERROR_EMPTY, bbuf_decode_u8(&b, &v));
}

static void safe(void **state) {
    (void) state;
    uint8_t v8 = 0;
    uint16_t v16 = 0;
    uint32_t v32 = 0;
    uint64_t v64 = 0;
    uint8_t expect[] = {0x01, 0x02, 0x10, 0x11, 0x13, 0x12,
                        0x20, 0x21, 0x22, 0x23, 0x27, 0x26, 0x25, 0x24,
                        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                        0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40
    };

    BBUF_DEFINE(b, 30);
    assert_int_equal(0, bbuf_encode_u8(&b, 0x01));
    assert_int_equal(0, bbuf_encode_u8(&b, 0x02));
    assert_int_equal(0, bbuf_encode_u16_be(&b, 0x1011));
    assert_int_equal(0, bbuf_encode_u16_le(&b, 0x1213));
    assert_int_equal(0, bbuf_encode_u32_be(&b, 0x20212223));
    assert_int_equal(0, bbuf_encode_u32_le(&b, 0x24252627));
    assert_int_equal(0, bbuf_encode_u64_be(&b, 0x3031323334353637llu));
    assert_int_equal(0, bbuf_encode_u64_le(&b, 0x4041424344454647llu));

    assert_memory_equal(expect, b.buf_start, sizeof(expect));

    assert_int_equal(0, bbuf_seek(&b, 0));
    assert_int_equal(0, bbuf_decode_u8(&b, &v8));
    assert_int_equal(0x01, v8);
    assert_int_equal(0, bbuf_decode_u8(&b, &v8));
    assert_int_equal(0x02, v8);
    assert_int_equal(0, bbuf_decode_u16_be(&b, &v16));
    assert_int_equal(0x1011, v16);
    assert_int_equal(0, bbuf_decode_u16_le(&b, &v16));
    assert_int_equal(0x1213, v16);
    assert_int_equal(0, bbuf_decode_u32_be(&b, &v32));
    assert_int_equal(0x20212223, v32);
    assert_int_equal(0, bbuf_decode_u32_le(&b, &v32));
    assert_int_equal(0x24252627, v32);
    assert_int_equal(0, bbuf_decode_u64_be(&b, &v64));
    assert_int_equal(0x3031323334353637llu, v64);
    assert_int_equal(0, bbuf_decode_u64_le(&b, &v64));
    assert_int_equal(0x4041424344454647llu, v64);
}

static void clear(void **state) {
    (void) state;
    BBUF_DEFINE(b, 30);
    b.buf_start[0] = 0;
    b.buf_start[1] = 0;
    assert_int_equal(0, bbuf_encode_u8(&b, 0x01));
    assert_int_equal(1, bbuf_size(&b));
    bbuf_clear(&b);
    assert_int_equal(0, bbuf_size(&b));
    assert_int_equal(0x01, b.buf_start[0]);
    bbuf_clear_and_overwrite(&b, 0);
    assert_int_equal(0, b.buf_start[0]);
}

int main(void) {
    const struct CMUnitTest tests[] = {
#ifdef BBUF_UNSAFE
            cmocka_unit_test(u8_unsafe),
            cmocka_unit_test(u16_unsafe_be),
            cmocka_unit_test(u16_unsafe_le),
            cmocka_unit_test(u32_unsafe_be),
            cmocka_unit_test(u32_unsafe_le),
            cmocka_unit_test(u64_unsafe_be),
            cmocka_unit_test(u64_unsafe_le),
#endif
            cmocka_unit_test(buf_s_define),
            cmocka_unit_test(initialize),
            cmocka_unit_test(enclose),
            cmocka_unit_test(u8),
            cmocka_unit_test(u8a),
            cmocka_unit_test(safe_alloc_free),
            cmocka_unit_test(safe_alloc_buffer_free),
            cmocka_unit_test(safe_alloc_string_free),
            cmocka_unit_test(u8_fill),
            cmocka_unit_test(safe),
            cmocka_unit_test(clear),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
