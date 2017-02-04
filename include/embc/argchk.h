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
 * @brief Argument checking.
 */

#ifndef ARGCHK_H_
#define ARGCHK_H_

#include "log.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup embc_argchk Argument checking and validation.
 *
 * @brief Macros and functions to support argument checking.
 *
 * This module provides input argument checking for functions that return
 * an error code when the argument is invalid.  For functions that assert
 * and do not return on errors, see dbc for design-by-contract.
 *
 * @{
 */

/**
 * @def ARGCHK_FAIL_RETURN_CODE
 *
 * @brief The return code for argument check failure.
 */
#ifndef ARGCHK_FAIL_RETURN_CODE
#define ARGCHK_FAIL_RETURN_CODE 5
#endif


/**
 * @brief Check that an argument is true.
 *
 * @param condition The condition for the check which is expected to be 
 *      a truthy C value, as would be provided to assert.  When false, the 
 *      check fails and the macro causes the enclosing function to return with
 *      an error.
 * @param message The message to display when condition is false.
 */
#define ARGCHK_ASSERT(condition, message) do { \
    if (!(condition)) { \
        LOGF_INFO("chk_assert: %s", (message)); \
        return ARGCHK_FAIL_RETURN_CODE; \
    } \
} while (0)

/**
 * @brief Check for a "true" value.
 *
 * @param x The expression which should not be null.
 */
#define ARGCHK_TRUE(x) ARGCHK_ASSERT((x), #x " is false")

/**
 * @brief Check for a "false" value.
 *
 * @param x The expression which should not be null.
 */
#define ARGCHK_FALSE(x) ARGCHK_ASSERT(!(x), #x " is true")
    
/**
 * @brief Check for a non-null value.
 *
 * @param x The expression which should not be null.
 */
#define ARGCHK_NOT_NULL(x) ARGCHK_ASSERT((x) != 0, #x " is null")

/**
 * @brief Assert that a function argument is greater than zero.
 *
 * @param x The function argument to check.
 */
#define ARGCHK_GT_ZERO(x) ARGCHK_ASSERT((x) > 0, #x " <= 0")

/**
 * @brief Assert that a function argument is greater than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define ARGCHK_GTE_ZERO(x) ARGCHK_ASSERT((x) >= 0, #x " < 0")

/**
 * @brief Assert that a function argument is not equal to zero.
 *
 * @param x The function argument to check.
 */
#define ARGCHK_NE_ZERO(x) ARGCHK_ASSERT((x) != 0, #x " != 0")

/**
 * @brief Assert that a function argument is less than zero.
 *
 * @param x The function argument to check.
 */
#define ARGCHK_LT_ZERO(x) ARGCHK_ASSERT((x) < 0, #x " >= 0")

/**
 * @brief Assert that a function argument is less than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define ARGCHK_LTE_ZERO(x) ARGCHK_ASSERT((x) <= 0, #x " > 0")

/**
 * @brief Assert that a function argument is less than or equal to zero.
 *
 * @param x The function argument to check.
 * @param xmin The minimum value, inclusive.
 * @param xmax The maximum value, inclusive.
 */
#define ARGCHK_RANGE_INT(x, x_min, x_max)  do { \
    int x__ = (x); \
    int x_min__ = (x_min); \
    int x_max__ = (x_max); \
    if (x__ < x_min__) { \
        LOGF_INFO("chk_assert: %s [%d] < %d", #x, x__, x_min__); \
        return ARGCHK_FAIL_RETURN_CODE; \
    } \
    if (x__ > x_max__) { \
        LOGF_INFO("chk_assert: %s [%d] > %d", #x, x__, x_max__); \
        return ARGCHK_FAIL_RETURN_CODE; \
    } \
} while (0)

/**
 * @brief Assert on a function argument condition.
 *
 * @param x The function argument or condition to check.
 */
#define ARGCHK_REQUIRE(x) ARGCHK_ASSERT((x), #x)

/** @} */

EMBC_CPP_GUARD_END

#endif /* ARGCHK_H_ */
