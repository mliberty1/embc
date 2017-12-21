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
 * @brief A managed buffer.
 */


#ifndef EMBC_BUFFER_H_
#define EMBC_BUFFER_H_

/**
 * @ingroup embc
 * @defgroup embc_buffer Memory-safe mutable buffers.
 *
 * @brief Buffers with full memory safety and constant time alloc/free.
 *
 * @{
 */

#include <stdint.h>
#include <stdbool.h>
#include "embc/cmacro_inc.h"
#include "embc/collections/list.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * @brief Opaque memory allocator.
 */
struct embc_buffer_allocator_s;

/**
 * @brief A memory safe buffer with support for safe mutable operations
 *      and fast dynamic allocation / deallocation.
 *
 * This module defines a buffer structure along with a fast memory manager
 * that performs constant-time allocation and deallocation.  The memory
 * manager is similar to a memory pool.  However, this memory manager
 * supports multiple buffer payload sizes in powers of 2 started with 32 bytes.
 * This approach shares many features with slab allocators.  The number
 * of blocks of each size are specified at initialization, and the system
 * memory manager only allocates once.
 *
 * Each buffer structure provides a memory safe implementation for writing
 * and reading the buffer.  The functions perform full bounds checking on
 * all operations.
 *
 * These buffers are normally used to hold dynamic data that needs to
 * passed through the system.  Buffers can easily be sent between tasks
 * or through networking stacks.  Each buffer adds 32 bytes of overhead
 * on most 32-bit architectures.  The buffer includes a linked list item
 * so that it can easily participate in queues and scatter/gather lists.
 * For single words of data, standard RTOS message queues are usually more
 * efficient.
 *
 * Ownership of memory is critical for networking stacks and data processing.
 * This implementation allows the consumer to take ownership of the buffer
 * and then free it without further communication with the producer.  However,
 * the consumer can also pass the buffer back to the producer so that the
 * producer can free it.  The later approach can save memory when
 * retransmissions may be required.
 */
struct embc_buffer_s {
    /**
     * @brief The pointer to the allocated buffer memory.
     *
     * Applications should **never** modify this pointer, only
     * the data itself.
     */
    uint8_t * const data;

    /**
     * @brief The total storage capacity of the buffer in bytes.
     *
     * The last byte in the buffer is data[capacity - 1].
     * Applications should **never** modify this capacity!
     */
    uint16_t const capacity;

    /**
     * @brief The active byte index for operations.
     *
     * When writing to the buffer, this index allows for memory
     * safe appending or modification of the data.  When reading
     * from the buffer, this index ensures that data past end
     * is never accessed.
     *
     * By definition cursor <= length.
     */
    uint16_t cursor;

    /**
     * @brief The length of the current buffer contents in bytes.
     *
     * This field allows memory safe reading from the buffer.
     *
     * By definition 0 <= length <= capacity.
     */
    uint16_t length;

    /**
     * @brief The length to reserve at the end of the buffer in bytes.
     *
     * This field allows safe reservation of space at the end of the buffer.
     * A common use is for networking stacks.  The stack can allocate the
     * buffer, set cursor to the header length and reserve the footer.  The
     * upper level can then fill in whatever data it would like into the
     * buffer and pass the buffer back to the networking stack.  The stack
     * can then add the header and footer in place without copying.
     *
     * By definition 0 <= reserve <= capacity.
     */
    uint16_t reserve;

    /**
     * @brief The buffer identifier.
     *
     * In many use cases, a producer allocates a buffer and then sends it
     * to a consumer.  Although the consumer takes ownership of the buffer
     * and eventually deallocates the buffer, this identifier allows the
     * consumer to provide status updates regarding this buffer to the
     * producer.
     */
    uint16_t buffer_id;

    /**
     * @brief Application-specific flags.
     *
     * Applications are free to define and use this field in any way.
     */
    uint16_t flags;

    /**
     * @brief The list item pointers.
     *
     * This field is used to manage a linked list of buffer instances.
     * Common usages include message queues and scatter/gather DMA.  The
     * owner of the buffer
     */
    struct embc_list_s item;
};

/**
 * @brief Initialize the buffer allocator.
 *
 * @param sizes The length 8 array of sizes to allocate to each
 *      buffer size.  sizes[0] (minimum size) determines
 *      how many 32 byte buffers to allocate.  See the table below
 *      for details on sizes and the overhead for each size.
 * @param length The number of entries in sizes.
 * @return The buffer allocator instance which gets memory from embc_alloc().
 *
 * The overhead for each index in size are:
 *
 * <table class="doxtable message">
 *  <tr><th>Index</th><th>Size</td><th>Actual</td><th>Overhead %</td></tr>
 *  <tr><td>0</td><td>32</td><td>64</td><td>100.0%</td></tr>
 *  <tr><td>1</td><td>64</td><td>96</td><td>50.0%</td></tr>
 *  <tr><td>2</td><td>128</td><td>160</td><td>25.0%</td></tr>
 *  <tr><td>3</td><td>256</td><td>288</td><td>12.5%</td></tr>
 *  <tr><td>4</td><td>512</td><td>544</td><td>6.3%</td></tr>
 *  <tr><td>5</td><td>1024</td><td>1056</td><td>3.1%</td></tr>
 *  <tr><td>6</td><td>2048</td><td>2080</td><td>1.6%</td></tr>
 *  <tr><td>7</td><td>4096</td><td>4128</td><td>0.8%</td></tr>
 * </table>
 */
struct embc_buffer_allocator_s * embc_buffer_initialize(embc_size_t const * sizes, embc_size_t length);

/**
 * @brief Finalize the buffer module and return all memory to the heap.
 *
 * @param self The instance to finalize.
 *
 * WARNING: all instances of all buffers must be returned first, or use after
 * free will be likely!
 */
void embc_buffer_finalize(struct embc_buffer_allocator_s * self);

/**
 * @brief Allocate a buffer.
 *
 * @param self The allocator instance for the buffer.
 * @param size The desired size for the buffer.
 * @return The new buffer whose total storage capacity is at least size.
 *      The caller takes ownership of the buffer.
 *      The caller can use embc_buffer_free() to release ownership of
 *      the buffer back to the allocator.
 *
 * This function will assert on out of memory.
 * This function is not thread-safe and must be protected by critical sections
 * if it is to be used from multiple tasks or ISRs.
 */
struct embc_buffer_s * embc_buffer_alloc(struct embc_buffer_allocator_s * self, embc_size_t size);

/**
 * @brief Free the buffer and return ownership to the allocator.
 *
 * @param buffer The buffer to free.
 *
 * This function is not thread-safe and must be protected by critical sections
 * if it is to be used from multiple tasks or ISRs.
 *
 * Note that the buffer knows what allocator it came from.  The allocator
 * information is stored opaquely in adjacent memory.
 */
void embc_buffer_free(struct embc_buffer_s * buffer);

/**
 * @brief Get the total number of bytes that can be stored in the buffer.
 *
 * @param buffer The buffer instance.
 * @return To total size of buffer in bytes.
 */
static inline embc_size_t embc_buffer_capacity(struct embc_buffer_s * buffer) {
    return (embc_size_t) (buffer->capacity);
}

/**
 * @brief Get the number of bytes currently in the buffer.
 *
 * @param buffer The buffer instance.
 * @return To size of data currently in buffer in bytes.
 */
static inline embc_size_t embc_buffer_length(struct embc_buffer_s * buffer) {
    return (embc_size_t) (buffer->length);
}

/**
 * @brief Get the remaining buffer size available for write from the cursor.
 *
 * @param buffer The buffer instance.
 * @return The amount of additional data that buffer can hold in bytes.
 */
static inline embc_size_t embc_buffer_write_remaining(struct embc_buffer_s * buffer) {
    return (embc_size_t) (buffer->capacity - buffer->cursor - buffer->reserve);
}

/**
 * @brief Get the remaining buffer size available for reads from the cursor.
 *
 * @param buffer The buffer instance.
 * @return The amount of additional data that can be read from the buffer in bytes.
 */
static inline embc_size_t embc_buffer_read_remaining(struct embc_buffer_s * buffer) {
    return (embc_size_t) (buffer->length - buffer->cursor);
}

/**
 * Set the cursor location (seek).
 *
 * @param buffer The buffer instance.
 * @param index The new cursor location between 0 and length.
 */
static inline void embc_buffer_cursor_set(struct embc_buffer_s * buffer, embc_size_t index) {
    EMBC_ASSERT((index >= 0) && (index <= buffer->length));
    buffer->cursor = index;
}

/**
 * Get the cursor location (tell).
 *
 * @param buffer The buffer instance.
 * @return The cursor location.
 */
static inline embc_size_t embc_buffer_cursor_get(struct embc_buffer_s * buffer) {
    return (embc_size_t) (buffer->cursor);
}

/**
 * Reset the buffer to empty without modifying the underlying data.
 *
 * @param buffer The buffer instance.
 */
static inline void embc_buffer_reset(struct embc_buffer_s * buffer) {
    buffer->cursor = 0;
    buffer->length = 0;
}

/**
 * Clear the buffer and set the data buffer to zeros.
 *
 * @param buffer The buffer instance.
 */
static inline void embc_buffer_clear(struct embc_buffer_s * buffer) {
    embc_memset(buffer->data, 0, buffer->capacity);
    embc_buffer_reset(buffer);
}

/**
 * @ingroup embc
 * @defgroup embc_buffer_write Write data to the buffer.
 *
 * @brief Write data to the buffer and update the cursor location.
 *
 * @{
 */

/**
 * Write to the buffer.
 *
 * @param[inout] buffer The buffer instance.
 * @param[in] data The pointer to the data to write.
 * @param[in] size The size of data in bytes.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write(struct embc_buffer_s * buffer,
                                void const * data,
                                embc_size_t size);

/**
 * Copy data from another buffer.
 *
 * @param[inout] destination The destination buffer instance.
 * @param[in] source The source buffer instance.  Data will be copied starting
 *      from the cursor.
 * @param[in] size The number of bytes to copy.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_copy(struct embc_buffer_s * destination,
                                struct embc_buffer_s * source,
                                embc_size_t size);

/**
 * Write a standard C null-terminated string to the buffer.
 *
 * @param[inout] buffer The buffer instance.
 * @param[in] str The string to append.  The null terminator will NOT
 *      be added.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_str(struct embc_buffer_s * buffer,
                                    char const * str);

/**
 * Write a standard C null-terminated string to the buffer.
 *
 * @param[inout] buffer The buffer instance.
 * @param[in] str The string to append.  The null terminator will NOT
 *      be added.
 * @return true on success or false if str was truncated to fit into the
 *      buffer.
 */
EMBC_API bool embc_buffer_write_str_truncate(struct embc_buffer_s * buffer,
                                             char const * str);

/**
 * @brief Write a uint8 to the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u8(struct embc_buffer_s * buffer, uint8_t value);

/**
 * @brief Write a uint16 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u16_le(struct embc_buffer_s * buffer, uint16_t value);

/**
 * @brief Write a uint16 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u32_le(struct embc_buffer_s * buffer, uint32_t value);

/**
 * @brief Write a uint16 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u64_le(struct embc_buffer_s * buffer, uint64_t value);

/**
 * @brief Write a uint16 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u16_be(struct embc_buffer_s * buffer, uint16_t value);

/**
 * @brief Write a uint16 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u32_be(struct embc_buffer_s * buffer, uint32_t value);

/**
 * @brief Write a uint16 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to write to the buffer at the cursor location.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_write_u64_be(struct embc_buffer_s * buffer, uint64_t value);

/** @} */

/**
 * @ingroup embc
 * @defgroup embc_buffer_read Read data from the buffer.
 *
 * @brief Read data to the buffer and update the cursor location.
 *
 * @{
 */

/**
 * Read from a buffer.
 *
 * @param[inout] buffer The buffer instance.
 * @param[inout] data The pointer to the data to write.
 * @param[in] size The size of data to read in bytes.
 * @warning This function asserts if buffer capacity is exceeded.
 */
EMBC_API void embc_buffer_read(struct embc_buffer_s * buffer,
                               void * data,
                               embc_size_t size);

/**
 * @brief Read a uint8 from the buffer.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint8_t embc_buffer_read_u8(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint16 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint16_t embc_buffer_read_u16_le(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint32 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint32_t embc_buffer_read_u32_le(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint64 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint64_t embc_buffer_read_u64_le(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint16 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint16_t embc_buffer_read_u16_be(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint32 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint32_t embc_buffer_read_u32_be(struct embc_buffer_s * buffer);

/**
 * @brief Read a uint64 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @return The value at the cursor location.
 * @warning This function asserts if cursor exceeds length.
 */
EMBC_API uint64_t embc_buffer_read_u64_be(struct embc_buffer_s * buffer);

/** @} */

/**
 * @ingroup embc
 * @defgroup embc_buffer_modify Modify the buffer.
 *
 * @brief Modify the buffer.
 *
 * @{
 */

/**
 * @brief Erase buffer contents.
 *
 * @param buffer The buffer instance to modify.
 * @param start The starting index which is the first byte to remove from the
 *      buffer.  0 <= start < buffer->length
 * @param end The ending index (exclusive) which is the first byte after start
 *      that is in the modified buffer.  0 <= start <= buffer->length.
 *      If end < start, then no erase is performed.
 * @warning This function asserts if start or end are out of range.
 */
EMBC_API void embc_buffer_erase(struct embc_buffer_s * buffer,
                                embc_size_t start,
                                embc_size_t end);

/** @} */

/**
 * @ingroup embc
 * @defgroup embc_buffer_static Statically allocate buffers.
 *
 * @brief Statically allocate buffers.
 *
 * Although this module is intended for buffers that communicate between
 * tasks, some applications benefit from the memory safety of these buffers
 * and do not need the memory allocator.  The functions in this section
 * allow for static allocation of buffer instances.  Instances created this
 * way should NEVER be used with applications expecting buffers from the
 * buffer memory allocator.
 *
 * @{
 */

/**
 * @brief Declare a new static buffer instance.
 *
 * @param name The name for the buffer.
 * @param size The size of the buffer in bytes.
 *
 * The allocation is static.  If this macro is placed in the outer scope of
 * a .c file, the buffer will be placed into BSS memory.  If this macro is
 * placed into a function, the buffer will be placed onto the stack.
 *
 * The buffer structure is declared but not initialized.  Use BBUF_INITIALIZE()
 * to initialize.
 */
#define EMBC_BUFFER_STATIC_DECLARE(name, size) \
    uint8_t (name ## _mem_)[size]; \
    struct embc_buffer_s name

/**
 * @brief Initialize (or reinitialize) a static buffer instance.
 *
 * @param name The name for the buffer instance provided to
 *      EMBC_BUFFER_STATIC_DECLARE().  The name may include a structure prefix.
 */
#define EMBC_BUFFER_STATIC_INITIALIZE(name) \
    do { \
        uint8_t ** d__ = (uint8_t **) &(name).data; \
        *d__ = (name ## _mem_); \
        uint16_t * c__ = (uint16_t *) &(name).capacity; \
        *c__ = sizeof(name ## _mem_); \
    } while(0); \
    (name).cursor = 0; \
    (name).length = 0; \
    (name).buffer_id = 0; \
    (name).flags = 0; \
    (name).item.next = &(name).item; \
    (name).item.prev = &(name).item;

/**
 * @brief Define a new static buffer instance.
 *
 * @param name The name for the buffer.
 * @param size The size of the buffer in bytes.
 *
 * The allocation is static.  If this macro is placed in the outer scope of
 * a .c file, the buffer will be placed into BSS memory.  If this macro is
 * placed into a function, the buffer will be placed onto the stack.  This
 * macro is functionally equivalent to:
 *
 *      EMBC_BUFFER_STATIC_DECLARE(name, size);
 *      EMBC_BUFFER_STATIC_INITIALIZE(name);
 */
#define EMBC_BUFFER_STATIC_DEFINE(name, size) \
    uint8_t name ## _mem_ [size]; \
    struct embc_buffer_s name = { \
        .data = (name ## _mem_), \
        .capacity = sizeof(name ## _mem_), \
        .cursor = 0, \
        .length = 0, \
        .buffer_id = 0, \
        .flags = 0, \
        .item = {&(name).item, &(name).item} \
    }


/** @} */

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_MEMORY_BUFFER_H_ */