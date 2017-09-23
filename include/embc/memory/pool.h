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

/**
 * @file
 *
 * @brief A memory pool for fixed-size blocks.
 */


#ifndef EMBC_MEMORY_POOL_H_
#define EMBC_MEMORY_POOL_H_

/**
 * @ingroup embc
 * @defgroup embc_pool Memory pool for fixed-size blocks.
 *
 * @brief A memory pool for fixed-size blocks.
 *
 * This memory pool implementation provides constant time allocation
 * and constant time deallocation with no risk of fragmentation.
 *
 * This implementation is not thread safe.
 *
 * References include:
 *
 * - https://en.wikipedia.org/wiki/Reference_counting
 * - https://en.wikipedia.org/wiki/Memory_pool
 * - http://www.barrgroup.com/Embedded-Systems/How-To/Malloc-Free-Dynamic-Memory-Allocation
 * - http://nullprogram.com/blog/2015/02/17/
 * - http://nullprogram.com/blog/2014/10/21/
 *
 * @{
 */

#include <stdint.h>
#include <stdbool.h>
#include "embc/cmacro_inc.h"

EMBC_CPP_GUARD_START


// Forward declaration for internal structure.
struct embc_pool_s;

/**
 * @brief Get the pool instance size.
 *
 * @param block_count The number of blocks in the pool.
 * @param block_size The size of each block in bytes.
 * @return The required size for embc_pool_s in bytes.
 */
EMBC_API int32_t embc_pool_instance_size(int32_t block_count,
                                         int32_t block_size);

/**
 * @brief Initialize a new memory pool.
 *
 * @param[out] self The memory pool to initialize which must be at least
 *      embc_pool_instance_size(block_count, block_size) bytes.
 * @param block_count The number of blocks in the pool.
 * @param block_size The size of each block in bytes.
 * @return 0 or error code.
 */
EMBC_API int32_t embc_pool_initialize(
        struct embc_pool_s * self,
        int32_t block_count,
        int32_t block_size);

/**
 * @brief Finalize the memory pool instance.
 *
 * @param self The memory pool initialize by embc_pool_initialize().
 *
 * This function does not free the instance memory as the allocated memory was
 * provided to embc_pool_initialize().
 */
EMBC_API void embc_pool_finalize(struct embc_pool_s * self);

/**
 * @brief Check if all blocks are allocated from the pool.
 *
 * @param self The memory pool instance.
 * @return 1 if empty, 0 if more blocks are available.
 */
EMBC_API int embc_pool_is_empty(struct embc_pool_s * self);

/**
 * @brief Allocate a new block from the pool.
 *
 * @param self The memory pool instance.
 * @return The new block from the pool.
 *
 * This function will ASSERT and not return on out of memory conditions.
 */
EMBC_API void * embc_pool_alloc(struct embc_pool_s * self);

/**
 * @brief Allocate a new block from the pool.
 *
 * @param self The memory pool instance.
 * @return The new block from the pool or 0.
 */
EMBC_API void * embc_pool_alloc_unsafe(struct embc_pool_s * self);

/**
 * @brief Free a block previous allocated from the pool.
 *
 * @param block The block returned by embc_pool_alloc().
 */
EMBC_API void embc_pool_free(struct embc_pool_s * self, void * block);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_MEMORY_POOL_H_ */
