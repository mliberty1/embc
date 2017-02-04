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
 * @brief Assert
 */

#ifndef EMBC_ASSERT_H_
#define EMBC_ASSERT_H_

#include "embc/cmacro_inc.h"

/**
 * @ingroup embc
 * @defgroup embc_assert Assert
 *
 * @brief Assert and halt execution.
 *
 * This module provides assertion checking.  When an assertion fails,
 * program execution halts.  On an embedded system, the recommended behavior
 * is to log the fault and then reboot.
 *
 * @{
 */

/**
 * @brief Check a condition that is normally true.
 *
 * @param[in] condition The condition which is normally true.
 *      when false, invoke fault().
 */
#define EMBC_ASSERT(condition) \
    if (! (condition) ) { embc_fatal(__FILE__, __LINE__, "assert"); }

/**
 * @brief Check that a memory allocation succeeded (not NULL).
 *
 * @param[in] pointer The pointer, usually returned from malloc/calloc,
 *      which must not be NULL.
 */
#define EMBC_ASSERT_ALLOC(pointer) \
    if (0 == (pointer)) { embc_fatal(__FILE__, __LINE__, "memory allocation"); }

/**
 * @brief Signal that a fatal error occurred.
 *
 * @param[in] msg The message for debugging.
 */
#define EMBC_FATAL(msg) embc_fatal(__FILE__, __LINE__, msg)

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
EMBC_API void embc_fatal(char const * file, int line, char const * msg);

/* @} */

#endif /* EMBC_ASSERT_H_ */
