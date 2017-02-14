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
 * @brief A reference counted memory pool for fixed-size objects.
 */


#ifndef EMBC_MEMORY_OBJECT_POOL_H_
#define EMBC_MEMORY_OBJECT_POOL_H_

/**
 * @ingroup embc
 * @defgroup embc_pool Memory pool for objects (class instances).
 *
 * @brief A reference counted memory pool for fixed-size objects.
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


/**
 * @brief The function called when a new object is allocated from the pool.
 *
 * @param[inout] obj The raw memory to be initialized in place.
 */
typedef void (*embc_object_pool_constructor)(void * obj);

/**
 * @brief The function called when an object is freed and returned to the pool.
 *
 * @param[in] obj The raw memory for the object being deallocated.
 */
typedef void (*embc_object_pool_destructor)(void * obj);


// Forward declaration for internal structure.
struct embc_object_pool_s;

/**
 * @brief Get the pool instance size.
 *
 * @param obj_count The number of objects in the pool.
 * @param obj_size The size of each object in bytes.
 * @return The required size for embc_object_pool_s in bytes.
 */
EMBC_API int32_t embc_object_pool_instance_size(int32_t obj_count,
                                                int32_t obj_size);

/**
 * @brief Initialize a new memory pool.
 *
 * @param[out] self The memory pool to initialize which must be at least
 *      embc_object_pool_instance_size(obj_count, obj_size) bytes.
 * @param obj_count The number of objects in the pool.
 * @param obj_size The size of each object in bytes.
 * @param constructor The function called for each new object allocated.
 *      If not provided, the object memory will be set to zero.
 * @param destructor The function called when an object is freed.
 * @return 0 or error code.
 */
EMBC_API int32_t embc_object_pool_initialize(
        struct embc_object_pool_s * self,
       int32_t obj_count,
       int32_t obj_size,
       embc_object_pool_constructor constructor,
       embc_object_pool_destructor destructor);

/**
 * @brief Finalize the memory pool instance.
 *
 * @param self The memory pool initialize by embc_object_pool_initialize().
 *
 * This function does not free the instance memory as the allocated memory was
 * provided to embc_object_pool_initialize().
 */
EMBC_API void embc_object_pool_finalize(struct embc_object_pool_s * self);

/**
 * @brief Allocate a new object from the pool.
 *
 * @param self The memory pool instance.
 * @return The new object.  If a pool contains a valid constructor, then the
 *      constructor will be called on the object before it is returned.
 *
 * This function will ASSERT and not return on out of memory conditions.
 */
EMBC_API void * embc_object_pool_alloc(struct embc_object_pool_s * self);

/**
 * @brief Increment the object's reference count.
 *
 * @param obj An object returned by embc_object_pool_alloc().
 */
EMBC_API void embc_object_pool_incr(void * obj);

/**
 * @brief Decrement the object's reference count.
 *
 * @param obj An object returned by embc_object_pool_alloc().
 * @return true if the object was returned to the pool.  false if the object
 *      is still available for use.
 *
 * If the reference count reaches zero, the object's memory will be returned
 * to the pool.  The caller must not use obj if this function returns false.
 * If the pool contains a valid destructor, it will be called before the
 * returning obj to the pool.
 */
EMBC_API bool embc_object_pool_decr(void * obj);

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_MEMORY_OBJECT_POOL_H_ */
