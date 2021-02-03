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
 * @brief Platform
 */

#ifndef EMBC_PLATFORM_H_
#define EMBC_PLATFORM_H_

#include "embc/cmacro_inc.h"
#include "embc/config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup embc
 * @defgroup embc_platform platform
 *
 * @brief Platform dependencies that must be defined for each target platform.
 *
 * @{
 */

EMBC_CPP_GUARD_START

/**
 * \brief Function which handles a fatal event.
 *
 * \param[in] file The file name.
 * \param[in] line The line numbers.
 * \param[in] msg The message to display.
 *
 * This function, embc_fatal(), is not implemented inside of EMBC.  The
 * software using EMBC must define this function, and it should take the
 * appropriate actions to handle an unrecoverable error.  The recommendation
 * for most embedded systems is to log the error if possible and then reset.
 */
void embc_fatal(char const * file, int line, char const * msg);

/**
 * @brief The value to use for all embc sizes.
 *
 * The use of signed and unsigned types for lengths/sizes, which are in the
 * domain of positive integers, is a debated topic.  Here are some references:
 * - http://www.soundsoftware.ac.uk/c-pitfall-unsigned
 * - http://blog.robertelder.org/signed-or-unsigned/
 * - https://embeddedgurus.com/stack-overflow/2009/08/a-tutorial-on-signed-and-unsigned-integers/
 *
 * In the opinion of the EMBC authors, signed integers should be used except
 * for bit operations (~ & | ^ << >>) and applications using modulo arithmetic,
 * such as cryptography.  Unfortunately, C does not perform saturation
 * arithmetic by default which opens up a whole class of potential errors.
 * With both signed and unsigned operations, the application is responsible
 * for checking overflows and underflows.  C's automatic promotion from signed
 * to unsigned is a problem, but you are using
 * "-Wall -Werror -Wpedantic -Wextra", right?
 */
typedef intptr_t embc_size_t;

#define embc_sizeof(x) ((embc_size_t) sizeof(x))

/**
 * @brief Count leading zeros.
 *
 * @param x The value for count leading zeros.
 * @return The number of most significant bits with zeros.
 */
static inline uint32_t embc_clz(uint32_t x);

/**
 * @brief Round up to the nearest power of 2.
 *
 * @param x The value to round up to the nearest power of 2.
 */
static inline uint32_t embc_upper_power_of_two(uint32_t x);

/**
 * @brief Fill the first num total_bytes of the memory buffer to value.
 *
 * @param ptr The memory buffer to fill.
 * @param value The new value for each byte.
 * @param num The number of total_bytes to fill.
 */
static inline void embc_memset(void * ptr, int value, embc_size_t num);

/**
 * @brief Copy data from one buffer to another.
 *
 * @param destination The destination buffer.
 * @param source The source buffer.
 * @param num The number of total_bytes to copy.
 *
 * The buffers destination and source must not overlap or the buffers may
 * be corrupted by this function!
 */
static inline void embc_memcpy(void * destination, void const * source, embc_size_t num);

/**
 * @brief The function type used by EMBC to allocate memory.
 *
 * @param size_bytes The number of total_bytes to allocate.
 * @return The pointer to the allocated memory.
 */
typedef void * (*embc_alloc_fn)(embc_size_t size_bytes);

/**
 * @brief The function type used by EMBC to deallocate memory.
 *
 * @param ptr The pointer to the memory to free.
 *
 * Many embedded systems do not allow free and can just call embc_fatal().
 */
typedef void (*embc_free_fn)(void * ptr);

/**
 * @brief Set the allocator used by embc.
 *
 * @param alloc The function used to allocate memory.
 * @param free The function used to free memory.
 */
EMBC_API void embc_allocator_set(embc_alloc_fn alloc, embc_free_fn free);

/**
 * @brief Allocate memory from the heap.
 *
 * @param size_bytes The number of total_bytes to allocate.
 * @return The pointer to the allocated memory.
 *
 * This function will assert on out of memory conditions.
 * For platforms that support freeing memory, use embc_free() to return the
 * memory to the heap.
 *
 * Call embc_allocator_set to assign an allocator before calling this function!
 */
EMBC_API void * embc_alloc(embc_size_t size_bytes);

/**
 * @brief Allocate memory from the heap and clear to 0.
 *
 * @param size_bytes The number of total_bytes to allocate.
 * @return The pointer to the allocated memory.
 *
 * This function will assert on out of memory conditions.
 * For platforms that support freeing memory, use embc_free() to return the
 * memory to the heap.
 */
static inline void * embc_alloc_clr(embc_size_t size_bytes) {
    void * ptr = embc_alloc(size_bytes);
    embc_memset(ptr, 0, size_bytes);
    return ptr;
}

/**
 * \brief Function to deallocate memory provided by embc_alloc() or embc_alloc_clr().
 *
 * \param ptr The pointer to the memory to free.
 *
 * Many embedded systems do not allow free and can just call embc_fatal().
 */
EMBC_API void embc_free(void * ptr);


/**
 * Additional functionals to define:
 *
 * log.h:
 *  A embc_log_printf implementation, and call embc_log_initialize()
 *
 * time.h:
 * - embc_time_counter;
 * - embc_time_utc
 */


#if defined(EMBC_PLATFORM_STDLIB)
#include "embc/platform/stdlib.h"
#elif defined(EMBC_PLATFORM_ARM)
#include "embc/platform/arm.h"
#else
#error "unsupported platform"
#endif

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_PLATFORM_H_ */
