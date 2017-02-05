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

#include "embc/collections/bbuf.h"
#include "embc.h"
#include "embc/cdef.h"
#include "embc/assert.h"
#include <string.h>  // memset
#include <stdlib.h>

#define CHECK_ENCODE_ARGS(self, size) \
    DBC_NOT_NULL(self); \
    if (self->cursor + size > self->buf_end) { \
        return EMBC_ERROR_FULL; \
    }

#define CHECK_DECODE_ARGS(self, value, size) \
    DBC_NOT_NULL(self); \
    ARGCHK_NOT_NULL(value); \
    if (self->cursor + size > self->end) { \
        return EMBC_ERROR_EMPTY; \
    }

static inline int cursor_write(struct bbuf_u8_s * self, size_t count) {
    uint8_t * p = self->cursor + count;
    if (p > self->buf_end) {
        LOGS_INFO("cursor_write but buffer full");
        return EMBC_ERROR_FULL;
    }
    self->cursor = p;
    if (p == self->buf_end) {
        self->end = self->buf_end;
    } else if (self->cursor >= self->end) {
        self->end = p;
    }
    return EMBC_SUCCESS;
}

static inline int cursor_advance(struct bbuf_u8_s * self, size_t count) {
    uint8_t * p = self->cursor + count;
    if (p > self->buf_end) {
        LOGS_INFO("cursor_write but buffer full");
        return EMBC_ERROR_FULL;
    }
    self->cursor = p;
    return EMBC_SUCCESS;
}

struct bbuf_u8_s * bbuf_alloc(size_t size) {
    uint8_t * start;
    struct bbuf_u8_s * buf = calloc(1, size + sizeof(struct bbuf_u8_s));
    EMBC_ASSERT_ALLOC(buf);
    start = ((uint8_t *) buf) + sizeof(struct bbuf_u8_s);
    buf->buf_start = start;
    buf->cursor = start;
    buf->end = start;
    buf->buf_end = start + size;
    return buf;
}

struct bbuf_u8_s * bbuf_alloc_from_string(char const * str) {
    size_t sz = strlen(str);
    struct bbuf_u8_s * self = bbuf_alloc(sz);
    memcpy(self->buf_start, str, sz);
    self->end = self->buf_end;
    return self;
}

struct bbuf_u8_s * bbuf_alloc_from_buffer(uint8_t const * buffer, size_t size) {
    struct bbuf_u8_s * self = bbuf_alloc(size);
    memcpy(self->buf_start, buffer, size);
    self->end = self->buf_end;
    return self;
}

void bbuf_free(struct bbuf_u8_s * self) {
    if (self) {
        self->buf_start = 0;
        self->buf_end = 0;
        self->cursor = 0;
        self->end = 0;
        free(self);
    }
}

void bbuf_initialize(struct bbuf_u8_s * self, uint8_t * data, size_t size) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(data);
    self->buf_start = data;
    self->buf_end = data + size;
    self->cursor = data;
    self->end = data;
}

void bbuf_enclose(struct bbuf_u8_s * self, uint8_t * data, size_t size) {
    bbuf_initialize(self, data, size);
    self->end = self->buf_end;
}

size_t bbuf_capacity(struct bbuf_u8_s const * self) {
    if (!self) {
        return 0;
    }
    return (self->buf_end - self->buf_start);
}

size_t bbuf_size(struct bbuf_u8_s const * self) {
    if (!self) {
        return 0;
    }
    return (self->end - self->buf_start);
}

int bbuf_seek(struct bbuf_u8_s * self, size_t pos) {
    DBC_NOT_NULL(self);
    ARGCHK_REQUIRE(pos <= (size_t) (self->end - self->buf_start));
    self->cursor = self->buf_start + pos;
    return 0;
}

void bbuf_clear(struct bbuf_u8_s * self) {
    DBC_NOT_NULL(self);
    self->cursor = self->buf_start;
    self->end = self->buf_start;
}

void bbuf_clear_and_overwrite(struct bbuf_u8_s * self, uint8_t value) {
    DBC_NOT_NULL(self);
    self->cursor = self->buf_start;
    self->end = self->buf_start + 1;
    memset(self->buf_start, value, self->buf_end - self->buf_start);
}

size_t bbuf_tell(struct bbuf_u8_s * self) {
    DBC_NOT_NULL(self);
    return ((size_t) (self->end - self->buf_start));
}

int bbuf_encode_u8(struct bbuf_u8_s * self, uint8_t value) {
    CHECK_ENCODE_ARGS(self, 1);
    BBUF_ENCODE_U8(self->cursor, value);
    return cursor_write(self, 1);
}

EMBC_API int bbuf_encode_u8a(struct bbuf_u8_s * self,
                             uint8_t const * value, size_t size) {
    CHECK_ENCODE_ARGS(self, size);
    memcpy(self->cursor, value, size);
    return cursor_write(self, size);
}

int bbuf_encode_u16_be(struct bbuf_u8_s * self, uint16_t value) {
    CHECK_ENCODE_ARGS(self, 2);
    BBUF_ENCODE_U16_BE(self->cursor, value);
    return cursor_write(self, 2);
}

int bbuf_encode_u16_le(struct bbuf_u8_s * self, uint16_t value) {
    CHECK_ENCODE_ARGS(self, 2);
    BBUF_ENCODE_U16_LE(self->cursor, value);
    return cursor_write(self, 2);
}

int bbuf_encode_u32_be(struct bbuf_u8_s * self, uint32_t value) {
    CHECK_ENCODE_ARGS(self, 4);
    BBUF_ENCODE_U32_BE(self->cursor, value);
    return cursor_write(self, 4);
}

int bbuf_encode_u32_le(struct bbuf_u8_s * self, uint32_t value) {
    CHECK_ENCODE_ARGS(self, 4);
    BBUF_ENCODE_U32_LE(self->cursor, value);
    return cursor_write(self, 4);
}

int bbuf_encode_u64_be(struct bbuf_u8_s * self, uint64_t value) {
    CHECK_ENCODE_ARGS(self, 8);
    BBUF_ENCODE_U64_BE(self->cursor, value);
    return cursor_write(self, 8);
}

int bbuf_encode_u64_le(struct bbuf_u8_s * self, uint64_t value) {
    CHECK_ENCODE_ARGS(self, 8);
    BBUF_ENCODE_U64_LE(self->cursor, value);
    return cursor_write(self, 8);
}

int bbuf_decode_u8(struct bbuf_u8_s * self, uint8_t * value) {
    CHECK_DECODE_ARGS(self, value, 1);
    *value = BBUF_DECODE_U8(self->cursor);
    return cursor_advance(self, 1);
}

int bbuf_decode_u8a(struct bbuf_u8_s * self,
                    size_t size, uint8_t * value) {
    CHECK_DECODE_ARGS(self, value, size);
    memcpy(value, self->cursor, size);
    return cursor_advance(self, size);
}

int bbuf_decode_u16_be(struct bbuf_u8_s * self, uint16_t * value) {
    CHECK_DECODE_ARGS(self, value, 2);
    *value = BBUF_DECODE_U16_BE(self->cursor);
    return cursor_advance(self, 2);
}

int bbuf_decode_u16_le(struct bbuf_u8_s * self, uint16_t * value) {
    CHECK_DECODE_ARGS(self, value, 2);
    *value = BBUF_DECODE_U16_LE(self->cursor);
    return cursor_advance(self, 2);
}

int bbuf_decode_u32_be(struct bbuf_u8_s * self, uint32_t * value) {
    CHECK_DECODE_ARGS(self, value, 4);
    *value = BBUF_DECODE_U32_BE(self->cursor);
    return cursor_advance(self, 4);
}

int bbuf_decode_u32_le(struct bbuf_u8_s * self, uint32_t * value) {
    CHECK_DECODE_ARGS(self, value, 4);
    *value = BBUF_DECODE_U32_LE(self->cursor);
    return cursor_advance(self, 4);
}

int bbuf_decode_u64_be(struct bbuf_u8_s * self, uint64_t * value) {
    CHECK_DECODE_ARGS(self, value, 8);
    *value = BBUF_DECODE_U64_BE(self->cursor);
    return cursor_advance(self, 8);
}

int bbuf_decode_u64_le(struct bbuf_u8_s * self, uint64_t * value) {
    CHECK_DECODE_ARGS(self, value, 8);
    *value = BBUF_DECODE_U64_LE(self->cursor);
    return cursor_advance(self, 8);
}
