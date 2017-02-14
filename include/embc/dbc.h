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
 * @brief Design by contract macros.
 */

#ifndef EMBC_DBC_H_
#define EMBC_DBC_H_


#include "log.h"
#include "embc/assert.h"


/**
 * @ingroup embc
 * @defgroup emb_dbc Design by contract
 *
 * @brief Macros and functions to support design by contract.
 *
 * When a DBC check fails, these macros call embc_fatal() which will usually
 * intentionally halt the program which typically reboots an embedded system.
 * These checks should be used for internal APIs where error handling is
 * not meaningful.  For error handling see argchk.h.
 *
 * References include:
 *
 * - http://dbc.rubyforge.org/
 * - https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=3&cad=rja&uact=8&ved=0CCsQFjACahUKEwiLoK6g04HIAhWFmh4KHY8aAvo&url=http%3A%2F%2Fwww.barrgroup.com%2FEmbedded-Systems%2FHow-To%2FDesign-by-Contract-for-Embedded-Software&usg=AFQjCNHiKuGGWvikJrMCsyzwSx1y229nYQ&sig2=wFxNiKE3JQpYNfFZRif1tQ&bvm=bv.103073922,d.dmo
 *
 * @{
 */


/**
 * @brief Assert on a design-by-contract condition.
 *
 * @param condition The condition for the assert.  The condition is successful
 *      when true.  When false, an error has occurred.
 * @param message The message to display when condition is false.
 */
#define DBC_ASSERT(condition, message) do { \
    if (!(condition)) { \
        LOGF_ERROR("dbc_assert: %s", (message)); \
        embc_fatal(__FILENAME__, __LINE__, (message)); \
    } \
} while (0);

/**
 * @brief Check for a "true" value.
 *
 * @param x The expression which should not be null.
 */
#define DBC_TRUE(x) DBC_ASSERT((x), #x " is false")

/**
 * @brief Check for a "false" value.
 *
 * @param x The expression which should not be null.
 */
#define DBC_FALSE(x) DBC_ASSERT(!(x), #x " is true")

/**
 * @brief Assert on a null value.
 *
 * @param x The expression which should not be null.
 */
#define DBC_NOT_NULL(x) DBC_ASSERT((x) != 0, #x " is null")

/**
 * @brief Assert that two values are strictly equal.
 *
 * @param a The first value.
 * @param b The second value.
 */
#define DBC_EQUAL(a, b) DBC_ASSERT((a) == (b), #a " != " #b)

/**
 * @brief Assert that a value is greater than zero.
 *
 * @param x The function argument to check.
 */
#define DBC_GT_ZERO(x) DBC_ASSERT((x) > 0, #x " <= 0")

/**
 * @brief Assert that a value is greater than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define DBC_GTE_ZERO(x) DBC_ASSERT((x) >= 0, #x " < 0")

/**
 * @brief Assert that a value is not equal to zero.
 *
 * @param x The function argument to check.
 */
#define DBC_NE_ZERO(x) DBC_ASSERT((x) != 0, #x " != 0")

/**
 * @brief Assert that a value is less than zero.
 *
 * @param x The function argument to check.
 */
#define DBC_LT_ZERO(x) DBC_ASSERT((x) < 0, #x " >= 0")

/**
 * @brief Assert that a value is less than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define DBC_LTE_ZERO(x) DBC_ASSERT((x) <= 0, #x " > 0")

/**
 * @brief Assert that a value is less than or equal to zero.
 *
 * @param x The function argument to check.
 * @param xmin The minimum value, inclusive.
 * @param xmax The maximum value, inclusive.
 */
#define DBC_RANGE_INT(x, x_min, x_max)  do { \
    int x__ = (x); \
    int x_min__ = (x_min); \
    int x_max__ = (x_max); \
    DBC_ASSERT(x__ >= x_min__, #x " too small"); \
    DBC_ASSERT(x__ <= x_max__, #x " too big"); \
} while(0)

/**
 * @brief Assert on a value.
 *
 * @param x The function argument or condition to check.
 *
 * Alias for DBC_ASSERT().
 */
#define DBC_REQUIRE(x) DBC_ASSERT((x), #x)

/** @} */

#endif /* EMBC_DBC_H_ */
