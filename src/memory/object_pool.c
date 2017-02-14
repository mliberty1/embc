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

#include "embc/memory/object_pool.h"
#include "embc/argchk.h"
#include "embc/assert.h"
#include "embc/cdef.h"
#include "embc/log.h"
#include "embc.h"
#include "utlist.h"
#include <stdlib.h>
#include <string.h> // memset


#define EMBC_POOL_ALIGNMENT 8  /* in bytes */
#define MAGIC 0x9548CE11
#define STATUS_ALLOC 0xbfbf  /* bit[0] must be set! */

/**
 * @brief The structure to store status when allocated.
 */
struct embc_object_pool_alloc_s {
    uint16_t status;
    uint16_t count;
};

/**
 * @brief The object header in each pool element.
 */
struct embc_object_pool_element_s {
    /** The pointer to the pool (used for deallocation). */
    struct embc_object_pool_s * pool;
    union {
        /** The status when allocated. */
        struct embc_object_pool_alloc_s alloc;
        /** The next free element when deallocated. */
        struct embc_object_pool_element_s * next;
    } d;
};

EMBC_STATIC_ASSERT((sizeof(intptr_t) != 4) ||
                           (8 == sizeof(struct embc_object_pool_element_s)),
                   embc_object_pool_element_header_size_32);
EMBC_STATIC_ASSERT((sizeof(intptr_t) != 8) ||
                           (16 == sizeof(struct embc_object_pool_element_s)),
                   embc_object_pool_element_header_size_64);

/**
 * @brief The memory pool instance.
 */
struct embc_object_pool_s {
    /** A magic number used to verify that the pool pointer is valid. */
    uint32_t magic;
    /** The list of free elements. */
    struct embc_object_pool_element_s * free_head;
    int32_t obj_size;
    embc_object_pool_constructor constructor;
    embc_object_pool_destructor destructor;
};

struct embc_object_pool_size_s {
    size_t obj_hdr;
    size_t obj_data;
    size_t pool_hdr;
    size_t element_sz;
    size_t sz;
};

static struct embc_object_pool_size_s embc_object_pool_size(int32_t obj_count, int32_t obj_size) {
    struct embc_object_pool_size_s sz;
    sz.obj_hdr = EMBC_ROUND_UP_TO_MULTIPLE(obj_size, EMBC_POOL_ALIGNMENT);
    sz.obj_data = EMBC_ROUND_UP_TO_MULTIPLE(sizeof(struct embc_object_pool_element_s), EMBC_POOL_ALIGNMENT);
    sz.pool_hdr = EMBC_ROUND_UP_TO_MULTIPLE(sizeof(struct embc_object_pool_s), EMBC_POOL_ALIGNMENT);
    sz.element_sz = sz.obj_hdr + sz.obj_data;
    sz.sz = sz.pool_hdr + sz.element_sz * obj_count;
    return sz;
}

int32_t embc_object_pool_instance_size(int32_t obj_count, int32_t obj_size) {
    struct embc_object_pool_size_s sz = embc_object_pool_size(obj_count, obj_size);
    return sz.sz;
}

int32_t embc_object_pool_initialize(
        struct embc_object_pool_s * self, int32_t obj_count, int32_t obj_size,
        embc_object_pool_constructor constructor, embc_object_pool_destructor destructor) {
    ARGCHK_NOT_NULL(self);
    ARGCHK_GT_ZERO(obj_count);
    ARGCHK_GT_ZERO(obj_size);
    struct embc_object_pool_size_s sz = embc_object_pool_size(obj_count, obj_size);
    memset(self, 0, sz.sz);
    self->magic = MAGIC;
    self->obj_size = obj_size;
    self->constructor = constructor;
    self->destructor = destructor;
    uint8_t * m = (uint8_t *) self;
    for (int32_t i = 0; i < obj_count; ++i) {
        uint8_t * el = m + sz.pool_hdr + sz.element_sz * i + sz.obj_hdr;
        el -= sizeof(struct embc_object_pool_element_s);
        struct embc_object_pool_element_s * hdr = (struct embc_object_pool_element_s *) (el);
        hdr->pool = self;
        LL_PREPEND2(self->free_head, hdr, d.next);
    }
    return 0;
}

void embc_object_pool_finalize(struct embc_object_pool_s * self) {
    EMBC_ASSERT(self);
    EMBC_ASSERT(self->magic == MAGIC);
    self->magic = 0;
}

void * embc_object_pool_alloc(struct embc_object_pool_s * self) {
    struct embc_object_pool_element_s * next = self->free_head;
    EMBC_ASSERT(next);
    LL_DELETE2(self->free_head, next, d.next);
    next->d.alloc.status = STATUS_ALLOC;
    next->d.alloc.count = 1;
    ++next; // advance to object
    if (self->constructor) {
        self->constructor(next);
    } else {
        memset(next, 0, self->obj_size);
    }
    return next;
}

static inline struct embc_object_pool_element_s * get_obj_header(void * obj) {
    uint8_t * m = (uint8_t *) obj;
    m -= sizeof(struct embc_object_pool_element_s);
    struct embc_object_pool_element_s * hdr = (struct embc_object_pool_element_s *) m;
    EMBC_ASSERT(hdr->d.alloc.status == STATUS_ALLOC);  // otherwise already free!
    return hdr;
}

void embc_object_pool_incr(void * obj) {
    struct embc_object_pool_element_s * hdr = get_obj_header(obj);
    EMBC_ASSERT(hdr->d.alloc.count < 65535);
    ++hdr->d.alloc.count;
}

bool embc_object_pool_decr(void * obj) {
    struct embc_object_pool_element_s * hdr = get_obj_header(obj);
    if (hdr->d.alloc.count > 1) {
        --hdr->d.alloc.count;
        return false;
    } else if (hdr->d.alloc.count == 1) {
        if (hdr->pool->destructor) {
            hdr->pool->destructor(obj);
        }
        LL_PREPEND2(hdr->pool->free_head, hdr, d.next);
        return true;
    }
    EMBC_FATAL("not allocated");
    return true;
}
