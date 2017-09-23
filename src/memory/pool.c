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

#include "embc/memory/pool.h"
#include "embc.h"
#include "utlist.h"
#include <stdlib.h>
#include <string.h> // memset

#define EMBC_POOL_ALIGNMENT sizeof(int *)
#define MAGIC 0x9548CE12
#define STATUS_ALLOC 0xbfbf  /* bit[0] must be set! */

/**
 * @brief The object header in each pool element.
 */
struct embc_pool_element_s {
    struct embc_pool_element_s * next;
};

/**
 * @brief The memory pool instance.
 */
struct embc_pool_s {
    /** A magic number used to verify that the pool pointer is valid. */
    uint32_t magic;
    /** The list of free elements. */
    struct embc_pool_element_s * free_head;
};

struct embc_pool_size_s {
    size_t block_hdr;
    size_t block_data;
    size_t pool_hdr;
    size_t element_sz;
    size_t sz;
};

static struct embc_pool_size_s embc_pool_size(int32_t block_count, int32_t block_size) {
    struct embc_pool_size_s sz;
    sz.block_hdr = EMBC_ROUND_UP_TO_MULTIPLE(sizeof(struct embc_pool_element_s), EMBC_POOL_ALIGNMENT);
    sz.block_data = EMBC_ROUND_UP_TO_MULTIPLE(block_size, EMBC_POOL_ALIGNMENT);
    sz.pool_hdr = EMBC_ROUND_UP_TO_MULTIPLE(sizeof(struct embc_pool_s), EMBC_POOL_ALIGNMENT);
    sz.element_sz = sz.block_hdr + sz.block_data;
    sz.sz = sz.pool_hdr + sz.element_sz * block_count;
    return sz;
}

int32_t embc_pool_instance_size(int32_t block_count, int32_t block_size) {
    struct embc_pool_size_s sz = embc_pool_size(block_count, block_size);
    return sz.sz;
}

int32_t embc_pool_initialize(
        struct embc_pool_s * self,
        int32_t block_count,
        int32_t block_size) {
    DBC_NOT_NULL(self);
    DBC_GT_ZERO(block_count);
    DBC_GT_ZERO(block_size);
    struct embc_pool_size_s sz = embc_pool_size(block_count, block_size);
    memset(self, 0, sz.sz);
    self->magic = MAGIC;
    uint8_t * m = (uint8_t *) self;
    for (int32_t i = 0; i < block_count; ++i) {
        uint8_t * el = m + sz.pool_hdr + sz.element_sz * i + sz.block_hdr;
        el -= sizeof(struct embc_pool_element_s);
        struct embc_pool_element_s * hdr = (struct embc_pool_element_s *) (el);
        LL_PREPEND(self->free_head, hdr);
    }

    return 0;
}

void embc_pool_finalize(struct embc_pool_s * self) {
    DBC_NOT_NULL(self);
    DBC_EQUAL(self->magic, MAGIC);
    self->magic = 0;
}

int embc_pool_is_empty(struct embc_pool_s * self) {
    DBC_NOT_NULL(self);
    return (self->free_head ? 0 : 1);
}

void * embc_pool_alloc(struct embc_pool_s * self) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(self->free_head);
    struct embc_pool_element_s * hdr = self->free_head;
    LL_DELETE(self->free_head, hdr);
    return ((void *) (hdr + 1));
}

void * embc_pool_alloc_unsafe(struct embc_pool_s * self) {
    DBC_NOT_NULL(self);
    if (!self->free_head) {
        return 0;
    }
    struct embc_pool_element_s * hdr = self->free_head;
    LL_DELETE(self->free_head, hdr);
    return ((void *) (hdr + 1));
}

void embc_pool_free(struct embc_pool_s * self, void * block) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(block);
    struct embc_pool_element_s * hdr = block;
    --hdr;
    LL_PREPEND(self->free_head, hdr);
}
