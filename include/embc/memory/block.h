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

/**
 * @file
 *
 * @brief A block-based allocator.
 */


#ifndef EMBC_MEMORY_BLOCK_H_
#define EMBC_MEMORY_BLOCK_H_

/**
 * @ingroup embc
 * @defgroup embc_block Block-based allocator.
 *
 * @brief A block-based allocator.
 *
 * This block based allocator is a simple form of a heap.  It allocates
 * the provided memory using fixed size blocks.  This allocator does suffer
 * from fragmentation and variable (but bounded) allocation time.  This
 * allocator is best used for objects with variable runtime size that are
 * allocated and deallocated together.
 *
 * The allocation information (the struct embc_mblock_s) is stored
 * separated from the managed memory which can be necessary in many cases.
 * A typical use of this block based allocator is to manage the USB device
 * endpoint memory buffers.
 *
 * This implementation is not thread safe.
 *
 * References include:
 *
 * @{
 */

#include <stdint.h>
#include <stdbool.h>
#include "embc/cmacro_inc.h"

EMBC_CPP_GUARD_START


// Forward declaration for internal structure.
struct embc_mblock_s;

/**
 * @brief Get the block instance size.
 *
 * @param mem_size The total size of the memory to manage.
 * @param block_size The size of each block in total_bytes.
 * @return The required size for embc_mblock_s in total_bytes.
 */
EMBC_API int32_t embc_mblock_instance_size(int32_t mem_size,
                                           int32_t block_size);

/**
 * @brief Initialize a new block allocator.
 *
 * @param[out] self The memory block allocator to initialize which must be at
 *      least embc_mblock_instance_size(mem_size, block_size) total_bytes.
 * @param mem The memory region to manage.
 * @param mem_size The total size of the memory to manage.
 * @param block_size The size of each block in total_bytes.
 * @return 0 or error code.
 */
EMBC_API int32_t embc_mblock_initialize(
        struct embc_mblock_s * self,
        void * mem,
        int32_t mem_size,
        int32_t block_size);

/**
 * @brief Finalize the memory block allocator instance.
 *
 * @param self The memory block allocator initialized by embc_mblock_initialize().
 *
 * This function does not free the instance memory as the allocated memory was
 * provided to embc_mblock_initialize().
 */
EMBC_API void embc_mblock_finalize(struct embc_mblock_s * self);

/**
 * @brief Allocate memory.
 *
 * @param self The memory block allocator instance.
 * @param size The required size from the pool
 * @return The allocated memory.
 *
 * This function will ASSERT and not return on out of memory conditions.
 */
EMBC_API void * embc_mblock_alloc(struct embc_mblock_s * self, int32_t size);

/**
 * @brief Allocate memory.
 *
 * @param self The memory block allocator instance.
 * @param size The required size from the pool
 * @return The allocated memory or 0.  Application should use
 *      embc_mblock_alloc() whenever out of memory is a fatal error.
 */
EMBC_API void * embc_mblock_alloc_unsafe(struct embc_mblock_s * self, int32_t size);

/**
 * @brief Free previously allocated memory.
 *
 * @param self The memory block allocator instance.
 * @param buffer The previously allocated memory.
 * @param size The size of buffer provided to the allocator
 *
 * This function must be called with the exact size provided to the allocator.
 * Providing a different size may cause memory corruption.  The allocator
 * is not capable of detecting duplicate frees.
 */
EMBC_API void embc_mblock_free(struct embc_mblock_s * self, void * buffer, int32_t size);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_MEMORY_POOL_H_ */
