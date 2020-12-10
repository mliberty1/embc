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
 * @brief The EMBC shared library support functions.
 */

#ifndef EMBC_LIB_H_
#define EMBC_LIB_H_

/**
 * @ingroup embc
 * @defgroup embc_lib lib
 *
 * @brief The EMBC shared library support functions.
 *
 * This module allows EMBC to be used under Linux, Mac OSX or Windows
 * as a shared library.  This library can then be integrated under
 * Python using ctypes.
 *
 * @{
 */

#include "embc/cmacro_inc.h"
#include "embc/platform.h"

EMBC_CPP_GUARD_START

/**
 * @brief Initialize the library.
 */
EMBC_API void embc_lib_initialize();

/**
 * @brief The function called on fatal errors.
 *
 * @param user_data The arbitrary user data for the callback.
 * @param file The file name where the fatal error originated.
 * @param line The line number where the fatal error originated.
 * @param msg The message describing the fatal error.
 */
typedef void (*embc_lib_fatal_fn)(void * user_data, char const * file, int line, char const * msg);

/**
 * @brief The function called to print string.
 *
 * @param user_data The arbitrary user data for this callback.
 * @param str The C null-terminated UTF-8 encoded string to print.
 */
typedef void (*embc_lib_print_fn)(void * user_data, char const * str);

/**
 * @brief Set the function called by the library on fatal errors.
 *
 * @param fn The function to call.
 * @param user_data The arbitrary data to provide to fn.
 */
EMBC_API void embc_lib_fatal_set(embc_lib_fatal_fn fn, void * user_data);

/**
 * @brief Set the function called by the library to print strings.
 *
 * @param fn The function to call.
 * @param user_data The arbitrary data to provide to fn.
 */
EMBC_API void embc_lib_print_set(embc_lib_print_fn fn, void * user_data);

/**
 * @brief Dynamically allocate a block of memory.
 *
 * @param sz The size in total_bytes.
 * @return The pointer to the allocated memory or 0.
 */
EMBC_API void * embc_lib_alloc(embc_size_t sz);

/**
 * @brief Free a block of memory allocated by embc_lib_alloc().
 *
 * @param ptr The pointer to the memory to free.
 */
EMBC_API void embc_lib_free(void * ptr);


EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_LIB_H_ */
