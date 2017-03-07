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
 * @brief Support encoding and decoding from byte buffers.
 */

#ifndef BBUF_H_
#define BBUF_H_

#include <stddef.h>
#include <stdint.h>
#include "embc/cmacro_inc.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_bbuf Byte buffer
 *
 * @brief Encoding and decoding from byte buffers.
 *
 * This module contains several different mechanisms for encoding and
 * decoding from byte buffers.  The macros are completely memory unsafe
 * but very fast.  The unsafe functions should inline and track the current
 * cursor location, but will not respect the buffer end.  The safe functions
 * fully verify every operation for memory safety and are recommend except
 * when performance is critical.
 *
 * This entire module is very pedantic.  However, many memory violations and
 * buffer overflows result from bad serialization and deserialization,
 * something this module hopes to improve.  It also alleviates many common
 * typing errors that can be difficult to identify.
 *
 * @{
 */

/**
 * @defgroup bbuf_macro Unsafe buffer macros
 *
 * @brief Support efficient but memory-unsafe encoding and decoding from
 *      byte buffers using macros.
 *
 * These macros are NOT memory safe to enable high performance for packing
 * and unpacking structures.  The caller is responsible for ensuring
 * that sufficient buffer space exists BEFORE calling these functions.
 *
 * @{
 */

/**
 * @brief Encode a uint8 to the buffer.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U8(buffer, value) (buffer)[0] = (value)

/**
 * @brief Encode a uint16 to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U16_BE(buffer, value) \
        (buffer)[0] = ((value) >> 8) & 0xff; \
        (buffer)[1] = ((value)     ) & 0xff

/**
 * @brief Encode a uint16 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U16_LE(buffer, value) \
        (buffer)[0] = ((value)     ) & 0xff; \
        (buffer)[1] = ((value) >> 8) & 0xff

/**
 * @brief Encode a uint32 to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U32_BE(buffer, value) \
        BBUF_ENCODE_U16_BE((buffer), ( (value) >> 16)); \
        BBUF_ENCODE_U16_BE((buffer) + 2, (value) )

/**
 * @brief Encode a uint32 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U32_LE(buffer, value) \
        BBUF_ENCODE_U16_LE((buffer), (value) ); \
        BBUF_ENCODE_U16_LE((buffer) + 2, (value) >> 16)

/**
 * @brief Encode a uint64to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U64_BE(buffer, value) \
        BBUF_ENCODE_U32_BE((buffer), ( (value) >> 32)); \
        BBUF_ENCODE_U32_BE((buffer) + 4, (value) )

/**
 * @brief Encode a uint64 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define BBUF_ENCODE_U64_LE(buffer, value) \
        BBUF_ENCODE_U32_LE((buffer), (value) ); \
        BBUF_ENCODE_U32_LE((buffer) + 4, (value) >> 32)

/**
 * @brief Decode a uint8 from the buffer.
 *
 * @param[in] buffer The pointer to the buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U8(buffer) (buffer)[0];

/**
 * @brief Decode a uint16 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U16_BE(buffer) ( \
        (((uint16_t) ((buffer)[0])) <<  8) | \
        (((uint16_t) ((buffer)[1]))      ) )

/**
 * @brief Decode a uint16 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U16_LE(buffer) ( \
        (((uint16_t) ((buffer)[0]))     ) | \
        (((uint16_t) ((buffer)[1])) << 8) )

/**
 * @brief Decode a uint32 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U32_BE(buffer) ( \
        (((uint32_t) BBUF_DECODE_U16_BE(buffer)) << 16) | \
        ((uint32_t) BBUF_DECODE_U16_BE(buffer + 2)) )

/**
 * @brief Decode a uint32 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U32_LE(buffer) ( \
        ((uint32_t) BBUF_DECODE_U16_LE(buffer)) | \
        (((uint32_t) BBUF_DECODE_U16_LE(buffer + 2)) << 16) )

/**
 * @brief Decode a uint64 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U64_BE(buffer) ( \
        (((uint64_t) BBUF_DECODE_U32_BE(buffer)) << 32) | \
        ((uint64_t) BBUF_DECODE_U32_BE(buffer + 4)) )

/**
 * @brief Decode a uint64 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define BBUF_DECODE_U64_LE(buffer) ( \
        ((uint64_t) BBUF_DECODE_U32_LE(buffer)) | \
        (((uint64_t) BBUF_DECODE_U32_LE(buffer + 4)) << 32) )


/** @} */

/**
 * @defgroup bbuf_unsafe Unsafe buffer functions
 *
 * @brief Support efficient but memory-unsafe encoding and decoding from
 *      byte buffers.
 *
 * These functions are NOT memory safe to enable high performance for packing
 * and unpacking structures.  The caller is responsible for ensuring
 * that sufficient buffer space exists BEFORE calling these functions.
 *
 * These functions uses the "inline" keyword which requires C99 (not ANSI-C).
 *
 * @{
 */

#ifndef __STRICT_ANSI__
#define BBUF_UNSAFE

/**
 * @brief Encode a uint8 to the buffer.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u8(uint8_t ** buffer, uint8_t value) {
    BBUF_ENCODE_U8(*buffer, value);
    *buffer += 1;
}

/**
 * @brief Encode a uint16 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u16_be(uint8_t ** buffer, uint16_t value) {
    BBUF_ENCODE_U16_BE(*buffer, value);
    *buffer += 2;
}

/**
 * @brief Encode a uint16 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u16_le(uint8_t ** buffer, uint16_t value) {
    BBUF_ENCODE_U16_LE(*buffer, value);
    *buffer += 2;
}

/**
 * @brief Encode a uint32 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u32_be(uint8_t ** buffer, uint32_t value) {
    BBUF_ENCODE_U32_BE(*buffer, value);
    *buffer += 4;
}

/**
 * @brief Encode a uint32 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u32_le(uint8_t ** buffer, uint32_t value) {
    BBUF_ENCODE_U32_LE(*buffer, value);
    *buffer += 4;
}

/**
 * @brief Encode a uint64 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u64_be(uint8_t ** buffer, uint64_t value) {
    BBUF_ENCODE_U64_BE(*buffer, value);
    *buffer += 8;
}

/**
 * @brief Encode a uint64 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void bbuf_unsafe_encode_u64_le(uint8_t ** buffer, uint64_t value) {
    BBUF_ENCODE_U64_LE(*buffer, value);
    *buffer += 8;
}

/**
 * @brief Decode a uint8 from the buffer.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint8_t bbuf_unsafe_decode_u8(uint8_t const ** buffer) {
    uint8_t v = BBUF_DECODE_U8(*buffer);
    *buffer += 1;
    return v;
}

/**
 * @brief Decode a uint16 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint16_t bbuf_unsafe_decode_u16_be(uint8_t const ** buffer) {
    uint16_t v = BBUF_DECODE_U16_BE(*buffer);
    *buffer += 2;
    return v;
}

/**
 * @brief Decode a uint16 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint16_t bbuf_unsafe_decode_u16_le(uint8_t const ** buffer) {
    uint16_t v = BBUF_DECODE_U16_LE(*buffer);
    *buffer += 2;
    return v;
}

/**
 * @brief Decode a uint32 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint32_t bbuf_unsafe_decode_u32_be(uint8_t const ** buffer) {
    uint32_t v = BBUF_DECODE_U32_BE(*buffer);
    *buffer += 4;
    return v;
}

/**
 * @brief Decode a uint32 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint32_t bbuf_unsafe_decode_u32_le(uint8_t const ** buffer) {
    uint32_t v = BBUF_DECODE_U32_LE(*buffer);
    *buffer += 4;
    return v;
}

/**
 * @brief Decode a uint64 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint64_t bbuf_unsafe_decode_u64_be(uint8_t const ** buffer) {
    uint64_t v = BBUF_DECODE_U64_BE(*buffer);
    *buffer += 8;
    return v;
}

/**
 * @brief Decode a uint64 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint64_t bbuf_unsafe_decode_u64_le(uint8_t const ** buffer) {
    uint64_t v = BBUF_DECODE_U64_LE(*buffer);
    *buffer += 8;
    return v;
}

#endif

/** @} */

/**
 * @defgroup bbuf_safe Safe buffer functions
 *
 * @brief Support memory-safe encoding and decoding from
 *      byte buffers.
 *
 * These macros are NOT memory safe to enable high performance for packing
 * and unpacking structures.  The caller is responsible for ensuring
 * that sufficient buffer space exists BEFORE calling these functions.
 *
 * @{
 */

/**
 * @brief A memory-safe mutable buffer instance.
 */
struct bbuf_u8_s {
    /**
     * @brief The start of the allocated buffer memory.
     */
    uint8_t * buf_start;

    /**
     * @brief The end of the allocated buffer memory, exclusive.
     *
     * The available buffer size is buf_end - buf_start.
     */
    uint8_t * buf_end;

    /**
     * @brief The active location for operations.
     *
     * By definition buf_start <= cursor <= buf_end
     */
    uint8_t * cursor;

    /**
     * @brief The end of the current buffer contents, exclusive.
     *
     * By definition buf_start <= end <= buf_end.
     * The currently used buffer size is end - buf_start.
     */
    uint8_t * end;
};

/**
 * @brief Declare a new memory-safe buffer.
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
 *
 */
#define BBUF_DECLARE(name, size) \
    uint8_t (name ## _mem_)[size]; \
    struct bbuf_u8_s name

/**
 * @brief Initialize (or reinitialize) a bbuf instance.
 *
 * @param name The name for the bbuf instance provided to BBUF_DECLARE().
 *      The name may include a structure prefix.
 */
#define BBUF_INITIALIZE(name) \
    (name).buf_start = (name ## _mem_); \
    (name).buf_end = (name ## _mem_) + sizeof(name ## _mem_); \
    (name).cursor = (name ## _mem_); \
    (name).end = (name ## _mem_)

/**
 * @brief Define a new memory-safe buffer.
 *
 * @param name The name for the buffer.
 * @param size The size of the buffer in bytes.
 *
 * The allocation is static.  If this macro is placed in the outer scope of
 * a .c file, the buffer will be placed into BSS memory.  If this macro is
 * placed into a function, the buffer will be placed onto the stack.  This
 * macro is functionally equivalent to:
 *
 *      BBUF_DECLARE(name, size);
 *      BBUF_INITIALIZE(name);
 */
#define BBUF_DEFINE(name, size) \
    uint8_t name ## _mem_ [size]; \
    struct bbuf_u8_s name = { \
        (name ## _mem_), \
        (name ## _mem_) + sizeof(name ## _mem_), \
        (name ## _mem_), \
        (name ## _mem_) \
    }

/**
 * @brief Allocate and initialize a new memory-safe buffer.
 *
 * @param[in] size The size of the buffer in bytes.
 * @return The new buffer instance.
 *
 * Call bbuf_free() when done with the buffer.  This function will assert
 * on out of memory.
 */
EMBC_API struct bbuf_u8_s * bbuf_alloc(size_t size);

/**
 * @brief Allocate and initialize a new memory-safe buffer from a string.
 *
 * @param[in] str The string used to allocate this instance.
 * @return The new buffer instance or NULL.
 *
 * Call bbuf_free() when done with the buffer.
 */
EMBC_API struct bbuf_u8_s * bbuf_alloc_from_string(char const * str);

/**
 * @brief Allocate and initialize a new memory-safe buffer from a C buffer.
 *
 * @param[in] buffer The buffer used to allocate this instance.
 * @param[in] size The size of buffer in bytes.
 * @return The new buffer instance or NULL.
 *
 * Call bbuf_free() when done with the buffer.
 */
EMBC_API struct bbuf_u8_s * bbuf_alloc_from_buffer(uint8_t const * buffer, size_t size);

/**
 * @brief Free a memory-safe buffer.
 *
 * @param[in] self The buffer to free.
 */
EMBC_API void bbuf_free(struct bbuf_u8_s * self);

/**
 * @brief Initialize a memory-safe buffer.
 * @param[inout] self The buffer to initialize.
 * @param[in] data The underlying data for the buffer.  This memory must
 *      remain valid for the life of the buffer.
 */
EMBC_API void bbuf_initialize(struct bbuf_u8_s * self, uint8_t * data, size_t size);

/**
 * @brief Initialize a memory-safe buffer.
 * @param[inout] self The buffer to initialize which will contain the entire
 *      contents of data.
 * @param[in] data The underlying data for the buffer.  The caller maintains
 *      ownership and the memory must remain valid for the life of the buffer.
 * @param[in] size The size of data and the initialized buffer.
 *
 * The cursor position is set to 0.
 */
EMBC_API void bbuf_enclose(struct bbuf_u8_s * self, uint8_t * data, size_t size);

/**
 * @brief The maximum capacity of a buffer.
 *
 * @param[in] self The buffer.
 * @return The maximum capacity of buffer.  If self is not valid, return 0.
 */
EMBC_API size_t bbuf_capacity(struct bbuf_u8_s const * self);

/**
 * @brief The current size of the data in a buffer.
 *
 * @param[in] self The buffer.
 * @return The size of buffer.  If self is not valid, return 0.
 */
EMBC_API size_t bbuf_size(struct bbuf_u8_s const * self);

/**
 * @brief The remaining size available in a buffer.
 *
 * @param[in] self The buffer.
 * @return The remaining size of buffer.  If self is not valid, return 0.
 */
EMBC_API size_t bbuf_available(struct bbuf_u8_s const * self);

/**
 * @brief Resize the current data size.
 *
 * @param[in] self The buffer.
 * @param[in] size The new buffer size.
 * @return 0 or success.
 *
 * The cursor position will be unmodified.  However, if the cursor position is
 * beyond the end of the resized buffer, the cursor position will be moved to
 * the end of the buffer.
 */
EMBC_API int bbuf_resize(struct bbuf_u8_s * self, size_t size);

/**
 * @brief Clear the buffer.
 *
 * @param[inout] self The buffer to clear.
 *
 * This function only changes pointers, but the underlying data remains in the
 * buffer.  For cases where the existing data should also be destroyed,
 * use bbuf_clear_and_overwrite().
 */
EMBC_API void bbuf_clear(struct bbuf_u8_s * self);

/**
 * @brief Clear the buffer and overwrite the underlying data.
 *
 * @param[inout] self The source buffer to clear.
 * @param[in] value The value used to initialize the buffer.
 * @return EMBC_SUCCESS or EMBC_ERROR_PARAMETER_INVALID.
 */
EMBC_API void bbuf_clear_and_overwrite(struct bbuf_u8_s * self, uint8_t value);

/**
 * @brief Copy from another bbuf instance.
 *
 * @param[inout] self The buffer instance.
 * @param other The source buffer instance to copy into self.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_copy(struct bbuf_u8_s * self, struct bbuf_u8_s const * other);

/**
 * @brief Copy from another C buffer.
 *
 * @param[inout] self The buffer instance.
 * @param buffer The source buffer to copy into self.
 * @param length The length of buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_copy_buffer(struct bbuf_u8_s * self, uint8_t * buffer, size_t length);

/**
 * @brief Seek a position in the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[in] pos The buffer position.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_seek(struct bbuf_u8_s * self, size_t pos);

/**
 * @brief Get the current buffer position.
 *
 * @param[inout] self The buffer instance.
 * @return The buffer position.
 */
EMBC_API size_t bbuf_tell(struct bbuf_u8_s * self);

/**
 * @brief Encode a uint8 to the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u8(struct bbuf_u8_s * self, uint8_t value);

/**
 * @brief Encode a uint8 array to the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @param[in] size The size of value in bytes.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u8a(struct bbuf_u8_s * self,
                             uint8_t const * value, size_t size);

/**
 * @brief Encode a uint16 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u16_be(struct bbuf_u8_s * self, uint16_t value);

/**
 * @brief Encode a uint16 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u16_le(struct bbuf_u8_s * self, uint16_t value);

/**
 * @brief Encode a uint32 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u32_be(struct bbuf_u8_s * self, uint32_t value);

/**
 * @brief Encode a uint32 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u32_le(struct bbuf_u8_s * self, uint32_t value);

/**
 * @brief Encode a uint64 to the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u64_be(struct bbuf_u8_s * self, uint64_t value);

/**
 * @brief Encode a uint64 to the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[in] value The value to add to the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_encode_u64_le(struct bbuf_u8_s * self, uint64_t value);

/**
 * @brief Decode a uint8 from the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u8(struct bbuf_u8_s * self, uint8_t * value);

/**
 * @brief Decode a uint8 array to the buffer.
 *
 * @param[inout] self The buffer instance.
 * @param[in] size The number of bytes to retrive.  The value must be at
 *      least size bytes long.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u8a(struct bbuf_u8_s * self,
                             size_t size, uint8_t * value);

/**
 * @brief Decode a uint16 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u16_be(struct bbuf_u8_s * self, uint16_t * value);

/**
 * @brief Decode a uint16 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u16_le(struct bbuf_u8_s * self, uint16_t * value);

/**
 * @brief Decode a uint32 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u32_be(struct bbuf_u8_s * self, uint32_t * value);

/**
 * @brief Decode a uint32 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u32_le(struct bbuf_u8_s * self, uint32_t * value);

/**
 * @brief Decode a uint64 from the buffer in big-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u64_be(struct bbuf_u8_s * self, uint64_t * value);

/**
 * @brief Decode a uint64 from the buffer in little-endian order.
 *
 * @param[inout] self The buffer instance.
 * @param[out] value The value from the buffer.
 * @return 0 on success or error code.
 */
EMBC_API int bbuf_decode_u64_le(struct bbuf_u8_s * self, uint64_t * value);

/** @} */

/** @} */

EMBC_CPP_GUARD_END

#endif /* BBUF_H_ */
