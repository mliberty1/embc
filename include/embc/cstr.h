/*
 * Copyright 2014-2018 Jetperch LLC
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
 * @brief Safe C-style string utilities.
 */

#ifndef EMBC_CSTR_H_
#define EMBC_CSTR_H_

#include "embc/cmacro_inc.h"
#include "embc/platform.h"
#include "embc/config.h"
#include <stdint.h>

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_cstr C-style string utilities
 *
 * @brief C-style string utilities.
 *
 * This module supplies c-style (null terminated byte array) string utilities.
 * The utilities are designed for memory safety (unlike <string.h>).
 *
 * @{
 */

/**
 * @brief Safely copy src to tgt.
 *
 * @param tgt The null-terminated destination string.
 * @param src The null-terminated source string.  If NULL, then tgt
 *      will be populated with an empty string.
 * @param tgt_size The total number of bytes available in tgt.
 * @return 0 on success, 1 if truncated, -1 on tgt NULL or tgt_size <= 0.
 */
EMBC_API int embc_cstr_copy(char * tgt, char const * src, embc_size_t tgt_size);

/**
 * @brief Safely copy src to tgt.
 *
 * @param tgt The null-terminated destination character array.
 * @param src The null-terminated source string.
 * @return 0 on success, 1 if truncated, -1 on other errors.
 */
#define embc_cstr_array_copy(tgt, src) \
    embc_cstr_copy(tgt, src, (embc_size_t) (sizeof(tgt) / sizeof(tgt[0])))

/**
 * @brief Compare strings ignoring case.
 *
 * @param s1 The first null-terminated string.
 * @param s2 The second null-terminated string.
 * @return 0 if s1 and s2 are comparable, -1 if s1 < s2, 1 if s2 > s1.
 */
EMBC_API int embc_cstr_casecmp(const char * s1, const char * s2);

/**
 * @brief Determine if a string starts with another string.
 *
 * @param s The string to search.
 * @param prefix The case-sensitive string prefix to match in s.
 * @return 0 on no match.  On match, return the pointer to s at the location of
 *     the first character after the matching prefix.
 */
EMBC_API const char * embc_cstr_starts_with(const char * s, const char * prefix);

/**
 * @brief Convert a string to an unsigned 32-bit integer.
 *
 * @param src The input source string containing an integer.  Strings that
 *      start with "0x" are processed as case-insensitive hexadecimal.
 * @param value The output unsigned 32-bit integer value.
 * @return 0 on success or error code.  On error, the value will not be
 *      modified.  To allow default values on parsing errors, set value
 *      before calling this function.
 */
EMBC_API int embc_cstr_to_u32(const char * src, uint32_t * value);

/**
 * @brief Convert a string to an signed 32-bit integer.
 *
 * @param src The input source string containing an integer.
 * @param value The output integer value.
 * @return 0 on success or error code.  On error, the value will not be
 *      modified.  To allow default values on parsing errors, set value
 *      before calling this function.
 */
EMBC_API int embc_cstr_to_i32(const char * src, int32_t * value);

/**
 * @brief Convert a fractional value into a scaled 32-bit integer.
 *
 * @param src The input source string.  Excess fractional digits will
 *      be truncated and ignored, not rounded.
 * @param exponent The base10 exponent.
 * @param value The resulting integer value.
 * @return 0 on success or error code.  On error, the value will not be
 *      modified.  To allow default values on parsing errors, set value
 *      before calling this function.
 *
 * Examples:
 *
 *     embc_cstr_to_i32s("1", 0, &x) => 1
 *     embc_cstr_to_i32s("1", 2, &x) => 100
 *     embc_cstr_to_i32s("1.01", 2, &x) => 101
 *     embc_cstr_to_i32s("   1.01   ", 2, &x) => 101
 *     embc_cstr_to_i32s("+1.01", 2, &x) => 101
 *     embc_cstr_to_i32s("-1.01", 2, &x) => -101
 *     embc_cstr_to_i32s("1.019", 2, &x) => 101
 */
EMBC_API int embc_cstr_to_i32s(const char * src, int32_t exponent, int32_t * value);

/**
 * @brief Convert a string to a floating point number.
 *
 * @param src The input source string containing a floating point number.
 * @param value The output floating point value.
 * @return 0 on success or error code.  On error, the value will not be
 *      modified.  To allow default values on parsing errors, set value
 *      before calling this function.
 *
 * This function is only available if EMBC_CSTR_FLOAT_ENABLE is 1 in the
 * embc config.h file.
 */
EMBC_API int embc_cstr_to_f32(const char * src, float * value);

/**
 * @brief Convert a string to upper case.  Equivalent to nonstandard strupr().
 *
 * @param[inout] s The null-terminated ASCII string to convert which is
 *      modified in place.  This function will corrupt UTF-8 data!
 *      NULL will cause error.  All other inputs are valid and return
 *      success.
 * @return 0 on success or error code.
 */
EMBC_API int embc_cstr_toupper(char * s);

/**
 * @brief Convert a string value into an index into table.
 *
 * @param[in] s The null-terminated ASCII string value input.
 * @param[in] table The list of possible null-terminated string values.
 *      The list is terminated with a NULL entry.
 * @param[inout] index The output index.  Index is only modified on a successful
 *      conversion, a default value can be set before calling this function.
 * @return 0 or error code.
 */
EMBC_API int embc_cstr_to_index(char const * s, char const * const * table, int * index);

/**
 * @brief Convert a string value into a boolean.
 *
 * @param[in] s The null-terminated ASCII string value input which is not
 *      case sensitive.
 *      True values include "TRUE", "ON", "1", "ENABLE".
 *      False values include "FALSE", "OFF", "0", "DISABLE".
 * @param[inout] value The output value.  Value is only modified on a
 *      successful conversion, a default value can be set before calling
 *      this function.
 * @return SUCCESS or error code.
 */
EMBC_API int embc_cstr_to_bool(char const * s, bool * value);

/**
 * @brief Convert a hex character to a 4-bit nibble.
 *
 * @param v The ASCII character value to convert.
 * @return The nibble value (0 to 16) or 0 on error.
 */
EMBC_API uint8_t embc_cstr_hex_to_u4(char v);

/**
 * @brief Convert a 4-bit nibble to a hex character.
 *
 * @param v The 4-bit nibble value.
 * @return The ASCII character value or '0' on error.
 */
EMBC_API char embc_cstr_u4_to_hex(uint8_t v);

/** @} */

EMBC_CPP_GUARD_END

#endif /* EMBC_CSTR_H_ */
