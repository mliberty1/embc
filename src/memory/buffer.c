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

#include "embc/memory/buffer.h"
#include "embc/dbc.h"
#include "embc/platform.h"
#include "embc/bbuf.h"

// Calculate the size to allocate for each structure, respecting platform
// alignment concerns
#define ALIGN (8)
#define MGR_SZ EMBC_ROUND_UP_TO_MULTIPLE((embc_size_t) sizeof(struct mgr_s), ALIGN)
#define POOL_SZ EMBC_ROUND_UP_TO_MULTIPLE((embc_size_t) sizeof(struct pool_s), ALIGN)
#define HDR_SZ EMBC_ROUND_UP_TO_MULTIPLE((embc_size_t) sizeof(struct embc_buffer_s), ALIGN)
EMBC_STATIC_ASSERT((sizeof(intptr_t) == 4) ? (24 == HDR_SZ) : (40 == HDR_SZ), header_size);

struct pool_s {
    embc_size_t payload_size;
    embc_size_t alloc_current;
    embc_size_t alloc_max;
    struct embc_list_s buffers;
};

struct mgr_s {
    embc_size_t size_max;
};

static uint8_t * self_ = 0;


static inline struct mgr_s * mgr_get() {
    return (struct mgr_s *) self_;
}

static inline struct pool_s * pool_get(embc_size_t index) {
    return (struct pool_s *) (self_ + MGR_SZ + (POOL_SZ * index));
}

static inline void buffer_init(struct embc_buffer_s * b) {
    b->cursor = 0;
    b->length = 0;
    b->buffer_id = 0;
    b->flags = 0;
}

void embc_buffer_initialize(embc_size_t const * sizes, embc_size_t length) {
    DBC_NOT_NULL(sizes);
    EMBC_ASSERT(self_ == 0);
    embc_size_t total_size = 0;
    total_size = MGR_SZ + POOL_SZ * length;
    embc_size_t header_size = total_size;
    embc_size_t buffer_sz = 0;
    for (embc_size_t i = 0; i < length; ++i) {
        buffer_sz = (32 << i);
        total_size += sizes[i] * (HDR_SZ + buffer_sz);
    }
    self_ = embc_alloc_clr(total_size);

    struct mgr_s * mgr = mgr_get();
    mgr->size_max = buffer_sz; // largest buffer

    uint8_t * buffers_ptr = self_ + header_size;

    for (embc_size_t i = 0; i < length; ++i) {
        struct pool_s * pool = pool_get(i);
        pool->payload_size = (32 << i);
        pool->alloc_current = 0;
        pool->alloc_max = 0;
        embc_list_initialize(&pool->buffers);
        for (embc_size_t k = 0; k < sizes[i]; ++k) {
            struct embc_buffer_s * b = (struct embc_buffer_s *) buffers_ptr;
            uint8_t ** d = (uint8_t **) &b->data;
            *d = buffers_ptr + HDR_SZ;
            uint16_t * c = (uint16_t *) &b->capacity;
            *c = pool->payload_size;
            embc_list_initialize(&b->item);
            embc_list_add_tail(&pool->buffers, &b->item);
            buffers_ptr += pool->payload_size + HDR_SZ;
        }
    }
}

void embc_buffer_finalize() {
    embc_free(self_);
    self_ = 0;
}

static embc_size_t size_to_index_(embc_size_t size) {
    EMBC_ASSERT((size > 0) && (size <= mgr_get()->size_max));
    embc_size_t index = 32 - embc_clz((uint32_t) (size - 1));
    if (index < 5) {
        index = 0; // 32 bytes is smallest
    } else {
        index = index - 5;
    }
    return index;
}

struct embc_buffer_s * embc_buffer_alloc(embc_size_t size) {
    embc_size_t index = size_to_index_(size);
    struct pool_s * p = pool_get(index);
    struct embc_list_s * item = embc_list_remove_head(&p->buffers);
    EMBC_ASSERT_ALLOC(item);
    struct embc_buffer_s * buffer = embc_list_entry(item, struct embc_buffer_s, item);
    ++p->alloc_current;
    if (p->alloc_current > p->alloc_max) {
        p->alloc_max = p->alloc_current;
    }
    return buffer;
}

void embc_buffer_free(struct embc_buffer_s * buffer) {
    embc_size_t size = embc_buffer_capacity(buffer);
    embc_size_t index = size_to_index_(size);
    struct pool_s * p = pool_get(index);
    embc_list_add_tail(&p->buffers, &buffer->item);
}

static inline void write_update_length(struct embc_buffer_s * buffer) {
    if (buffer->cursor > buffer->length) {
        buffer->length = buffer->cursor;
    }
}

void embc_buffer_write(struct embc_buffer_s * buffer,
                       void const * data,
                       embc_size_t size) {
    DBC_NOT_NULL(buffer);
    DBC_NOT_NULL(data);
    if (size > 0) {
        EMBC_ASSERT(size <= embc_buffer_write_remaining(buffer));
        uint8_t * ptr = buffer->data + buffer->cursor;
        embc_memcpy(ptr, data, size);
        buffer->cursor += size;
        write_update_length(buffer);
    }
}

static inline bool write_str_(struct embc_buffer_s * buffer,
                              char const * str) {
    DBC_NOT_NULL(buffer);
    DBC_NOT_NULL(str);
    while (buffer->cursor < buffer->capacity) {
        if (*str == 0) {
            write_update_length(buffer);
            return true;
        }
        buffer->data[buffer->cursor] = *str;
        ++str;
        ++buffer->cursor;
    }
    buffer->length = buffer->capacity;
    return false;
}

void embc_buffer_write_str(struct embc_buffer_s * buffer,
                           char const * str) {
    EMBC_ASSERT(write_str_(buffer, str));
}

bool embc_buffer_write_str_truncate(struct embc_buffer_s * buffer,
                                    char const * str) {
    return write_str_(buffer, str);
}


#define WRITE(buffer, value, buftype) \
    DBC_NOT_NULL(buffer); \
    EMBC_ASSERT((embc_size_t) sizeof(value) <= embc_buffer_write_remaining(buffer)); \
    uint8_t * ptr = buffer->data + buffer->cursor; \
    EMBC_BBUF_ENCODE_##buftype (ptr, value); \
    buffer->cursor += sizeof(value); \
    write_update_length(buffer);

void embc_buffer_write_u8(struct embc_buffer_s * buffer, uint8_t value) {
    WRITE(buffer, value, U8);
}

void embc_buffer_write_u16_le(struct embc_buffer_s * buffer, uint16_t value) {
    WRITE(buffer, value, U16_LE);
}

void embc_buffer_write_u32_le(struct embc_buffer_s * buffer, uint32_t value) {
    WRITE(buffer, value, U32_LE);
}

void embc_buffer_write_u64_le(struct embc_buffer_s * buffer, uint64_t value) {
    WRITE(buffer, value, U64_LE);
}

void embc_buffer_write_u16_be(struct embc_buffer_s * buffer, uint16_t value) {
    WRITE(buffer, value, U16_BE);
}

void embc_buffer_write_u32_be(struct embc_buffer_s * buffer, uint32_t value) {
    WRITE(buffer, value, U32_BE);
}

void embc_buffer_write_u64_be(struct embc_buffer_s * buffer, uint64_t value) {
    WRITE(buffer, value, U64_BE);
}

void embc_buffer_read(struct embc_buffer_s * buffer,
                      void * data,
                      embc_size_t size) {
    DBC_NOT_NULL(buffer);
    DBC_NOT_NULL(data);
    if (size > 0) {
        EMBC_ASSERT(size <= embc_buffer_read_remaining(buffer));
        uint8_t * ptr = buffer->data + buffer->cursor;
        embc_memcpy(data, ptr, size);
        buffer->cursor += size;
    }
}

#define READ(buffer, ctype, buftype) \
    DBC_NOT_NULL(buffer); \
    EMBC_ASSERT((embc_size_t) sizeof(ctype) <= embc_buffer_read_remaining(buffer)); \
    uint8_t * ptr = buffer->data + buffer->cursor; \
    ctype value = EMBC_BBUF_DECODE_##buftype (ptr); \
    buffer->cursor += sizeof(ctype); \
    return value;

uint8_t embc_buffer_read_u8(struct embc_buffer_s * buffer) {
    READ(buffer, uint8_t, U8);
}

uint16_t embc_buffer_read_u16_le(struct embc_buffer_s * buffer) {
    READ(buffer, uint16_t, U16_LE);
}

uint32_t embc_buffer_read_u32_le(struct embc_buffer_s * buffer) {
    READ(buffer, uint32_t, U32_LE);
}

uint64_t embc_buffer_read_u64_le(struct embc_buffer_s * buffer) {
    READ(buffer, uint64_t, U64_LE);
}

uint16_t embc_buffer_read_u16_be(struct embc_buffer_s * buffer) {
    READ(buffer, uint16_t, U16_BE);
}

uint32_t embc_buffer_read_u32_be(struct embc_buffer_s * buffer) {
    READ(buffer, uint32_t, U32_BE);
}

uint64_t embc_buffer_read_u64_be(struct embc_buffer_s * buffer) {
    READ(buffer, uint64_t, U64_BE);
}
