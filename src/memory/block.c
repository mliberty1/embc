
#include "embc/memory/block.h"
#include "embc/cdef.h"
#include "embc/dbc.h"

struct mblock_s {
    uint8_t * mem;
    int32_t mem_size;
    int32_t block_size;
    int32_t block_count;
    uint8_t bitmap[4]; // dynamically allocated to be the right size!
};

int32_t embc_mblock_instance_size(int32_t mem_size, int32_t block_size) {
    DBC_GT_ZERO(mem_size);
    DBC_GT_ZERO(block_size);
    int32_t blocks_count = mem_size / block_size;
    int32_t bytes_count = (blocks_count + 7) / 8;
    int32_t bitmap_size = EMBC_ROUND_UP_TO_MULTIPLE(bytes_count, 4);
    if (bitmap_size >= 4) {
        bitmap_size -= 4;
    }
    return (sizeof(struct mblock_s) + bitmap_size);
}

int32_t embc_mblock_initialize(
        struct embc_mblock_s * self,
        void * mem,
        int32_t mem_size,
        int32_t block_size) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(mem);
    DBC_GT_ZERO(mem_size);
    DBC_GT_ZERO(block_size);
    struct mblock_s * s = (struct mblock_s *) self;
    int32_t sz = embc_mblock_instance_size(mem_size, block_size);
    embc_memset(s, 0, sz);
    s->mem = (uint8_t *) mem;
    s->mem_size = mem_size;
    s->block_size = block_size;
    s->block_count = mem_size / block_size;
    return 0;
}

void embc_mblock_finalize(struct embc_mblock_s * self) {
    (void) self;
}

static inline int32_t size_to_blocks(struct mblock_s * s, int32_t size) {
    return (size + s->block_size - 1) / s->block_size;
}

void * embc_mblock_alloc_unsafe(struct embc_mblock_s * self, int32_t size) {
    // greedy allocator: take first space large enough
    struct mblock_s * s = (struct mblock_s *) self;
    DBC_NOT_NULL(s);
    DBC_GT_ZERO(size);
    int32_t blocks = size_to_blocks(s, size);
    int32_t idx_invalid = s->block_count + 1;
    int32_t idx_start = idx_invalid;
    int32_t free_count = 0;
    for (int32_t idx_search = 0; idx_search < s->block_count; ++idx_search) {
        int32_t bit = s->bitmap[idx_search / 8] >> (idx_search & 0x7);
        if (bit) {
            idx_start = idx_invalid;
            free_count = 0;
        } else if (idx_start == idx_invalid) {
            idx_start = idx_search;
            free_count = 1;
        } else {
            ++free_count;
        }
        if (free_count >= blocks) { // allocated, mark!
            for (int idx_alloc = idx_start; idx_alloc <= idx_search; ++idx_alloc) {
                s->bitmap[idx_alloc / 8] |= (uint8_t) (1 << (idx_alloc & 0x07));
            }
            return (s->mem + (idx_start * s->block_size));
        }
    }
    return 0;
}

void * embc_mblock_alloc(struct embc_mblock_s * self, int32_t size) {
    void * p = embc_mblock_alloc_unsafe(self, size);
    EMBC_ASSERT_ALLOC(p);
    return p;
}

void embc_mblock_free(struct embc_mblock_s * self, void * buffer, int32_t size) {
    struct mblock_s * s = (struct mblock_s *) self;
    uint8_t * b = (uint8_t *) buffer;
    EMBC_ASSERT(b >= s->mem);
    EMBC_ASSERT(b < (s->mem + s->mem_size));
    int32_t blocks = size_to_blocks(s, size);
    int32_t idx_start = (b - s->mem) / s->block_size;
    for (int idx = idx_start; idx < idx_start + blocks; ++idx) {
        int32_t bit = s->bitmap[idx / 8] >> (idx & 0x7);
        EMBC_ASSERT(bit);  // ensure already allocated
        s->bitmap[idx / 8] &= ~((uint8_t) (1 << (idx & 0x07)));
    }
}
