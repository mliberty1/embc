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
 * @brief EMBC time representation.
 */

#ifndef EMBC_TIME_H__
#define EMBC_TIME_H__

/**
 * @ingroup embc
 * @defgroup embc_time Time representation
 *
 * @brief EMBC time representation.
 *
 * The C standard library includes time.h which is very inconvenient for
 * embedded systems.  This module defines a much simpler 64-bit fixed point
 * integer for representing time.  The value is 34Q30 with the upper 34 bits
 * to represent whole seconds and the lower 30 bits to represent fractional
 * seconds.  A value of 2**30 (1 << 30) represents 1 second.  This
 * representation gives a resolution of 2 ** -30 (approximately 1 nanosecond)
 * and a range of +/- 2 ** 33 (approximately 272 years).  The value is
 * signed to allow for simple arithmetic on the time either as a fixed value
 * or as deltas.
 *
 * Certain elements may elect to use floating point time given in seconds.
 * The macros EMBC_TIME_TO_F64() and EMBC_F64_TO_TIME() facilitate
 * converting between the domains.  Note that double precision floating
 * point is not able to maintain the same resolution over the time range
 * as the 64-bit representation.  EMBC_TIME_TO_F32() and EMBC_F32_TO_TIME()
 * allow conversion to single precision floating point which has significantly
 * reduce resolution compared to the 34Q30 value.
 *
 * @{
 */

#include <stdint.h>
#include "embc/cmacro_inc.h"

EMBC_CPP_GUARD_START

/**
 * @brief The number of fractional bits in the 64-bit time representation.
 */
#define EMBC_TIME_Q 30


/**
 * @brief The fixed-point representation for 1 second.
 */
#define EMBC_TIME_SECOND (((int64_t) 1) << EMBC_TIME_Q)

/**
 * @brief The approximate fixed-point representation for 1 millisecond.
 */
#define EMBC_TIME_MILLISECOND (EMBC_TIME_SECOND / 1000)

/**
 * @brief The approximate fixed-point representation for 1 microsecond.
 */
#define EMBC_TIME_MICROSECOND (EMBC_TIME_SECOND / 1000000)

/**
 * @brief The approximate fixed-point representation for 1 nanosecond.
 *
 * WARNING: this value is imprecise!
 */
#define EMBC_TIME_NANOSECOND ((int64_t) 1)

/**
 * @brief The fixed-point representation for 1 minute.
 */
#define EMBC_TIME_MINUTE (EMBC_TIME_SECOND * 60)

/**
 * @brief The fixed-point representation for 1 hour.
 */
#define EMBC_TIME_HOUR (EMBC_TIME_MINUTE * 60)

/**
 * @brief The fixed-point representation for 1 day.
 */
#define EMBC_TIME_DAY (EMBC_TIME_HOUR * 24)

/**
 * @brief Convert the 64-bit fixed point time to a double.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The time as a double p.  Note that IEEE 747 doubles only have
 *      52 bits of precision, so the result will be truncated for very
 *      small deltas.
 */
#define EMBC_TIME_TO_F64(x) (((double) (x)) / ((double) EMBC_TIME_SECOND))

/**
 * @brief Convert the double precision time to 64-bit fixed point time.
 *
 * @param x The double-precision floating point time in seconds.
 * @return The time as a 34Q30.
 */
#define EMBC_F64_TO_TIME(x) ((int64_t) (((double) (x)) * (double) EMBC_TIME_SECOND))

/**
 * @brief Convert the 64-bit fixed point time to single precision float.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The time as a float p in seconds.  Note that IEEE 747 singles only
 *      have 32 bits of precision, so the result will likely be truncated.
 */
#define EMBC_TIME_TO_F32(x) (((double) (x)) / ((double) EMBC_TIME_SECOND))

/**
 * @brief Convert the single precision float time to 64-bit fixed point time.
 *
 * @param x The single-precision floating point time in seconds.
 * @return The time as a 34Q30.
 */
#define EMBC_F32_TO_TIME(x) ((int64_t) (((double) (x)) * (double) EMBC_TIME_SECOND))

#define _EMBC_TIME_SIGNUM(x) ((0 < (x) ) - ((x) < 0))

/**
 * @brief Convert to counter ticks, rounded to nearest.
 *
 * @param x The 64-bit signed fixed point time.
 * @param z The counter frequency in Hz.
 * @return The 64-bit time in counter ticks.
 */
#define EMBC_TIME_TO_COUNTER(x, z) \
    (( ((int64_t) (x)) * z + \
       _EMBC_TIME_SIGNUM(z) * EMBC_TIME_SECOND / 2) \
     >> EMBC_TIME_Q)

/**
 * @brief Convert to counter ticks, rounded up.
 *
 * @param x The 64-bit signed fixed point time.
 * @param z The counter frequency in Hz.
 * @return The 64-bit time in counter ticks.
 */
#define EMBC_TIME_TO_COUNTER_ROUND_UP(x, z) ((((int64_t) (x)) * z + EMBC_TIME_SECOND - 1) >> EMBC_TIME_Q)

/**
 * @brief Convert to counter ticks, rounded down
 *
 * @param x The 64-bit signed fixed point time.
 * @param z The counter frequency in Hz.
 * @return The 64-bit time in counter ticks.
 */
#define EMBC_TIME_TO_COUNTER_ROUND_DOWN(x, z) ((((int64_t) (x)) * z) >> EMBC_TIME_Q)

/**
 * @brief Convert to 32-bit unsigned seconds.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The 64-bit unsigned time in seconds, rounded up.
 */
#define EMBC_TIME_TO_SECONDS(x) EMBC_TIME_TO_COUNTER(x, 1)

/**
 * @brief Convert to milliseconds.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The 64-bit signed time in milliseconds, rounded up.
 */
#define EMBC_TIME_TO_MILLISECONDS(x) EMBC_TIME_TO_COUNTER(x, 1000)

/**
 * @brief Convert to microseconds.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The 64-bit signed time in microseconds, rounded up.
 */
#define EMBC_TIME_TO_MICROSECONDS(x) EMBC_TIME_TO_COUNTER(x, 1000000)

/**
 * @brief Convert to nanoseconds.
 *
 * @param x The 64-bit signed fixed point time.
 * @return The 64-bit signed time in nanoseconds, rounded up.
 */
#define EMBC_TIME_TO_NANOSECONDS(x) EMBC_TIME_TO_COUNTER(x, 1000000000ll)

/**
 * @brief Convert to 64-bit signed fixed point time.
 *
 * @param x The counter value in ticks.
 * @param z The counter frequency in Hz.
 * @return The 64-bit signed fixed point time.
 */
#define EMBC_COUNTER_TO_TIME(x, z) ((((int64_t) (x)) << EMBC_TIME_Q) / ((int64_t) (z)))

/**
 * @brief Convert to 64-bit signed fixed point time.
 *
 * @param x he 32-bit unsigned time in seconds.
 * @return The 64-bit signed fixed point time.
 */
#define EMBC_SECONDS_TO_TIME(x) (((int64_t) (x)) << EMBC_TIME_Q)

/**
 * @brief Convert to 64-bit signed fixed point time.
 *
 * @param x The 32-bit unsigned time in milliseconds.
 * @return The 64-bit signed fixed point time.
 */
#define EMBC_MILLISECONDS_TO_TIME(x) EMBC_COUNTER_TO_TIME(x, 1000)

/**
 * @brief Convert to 64-bit signed fixed point time.
 *
 * @param x The 32-bit unsigned time in microseconds.
 * @return The 64-bit signed fixed point time.
 */
#define EMBC_MICROSECONDS_TO_TIME(x) EMBC_COUNTER_TO_TIME(x, 1000000)

/**
 * @brief Convert to 64-bit signed fixed point time.
 *
 * @param x The 32-bit unsigned time in microseconds.
 * @return The 64-bit signed fixed point time.
 */
#define EMBC_NANOSECONDS_TO_TIME(x) EMBC_COUNTER_TO_TIME(x, 1000000000ll)

/**
 * @brief Compute the absolute value of a time.
 *
 * @param t The time.
 * @return The absolute value of t.
 */
static inline int64_t EMBC_TIME_ABS(int64_t t) {
    return ( (t) < 0 ? -(t) : (t) );
}

EMBC_CPP_GUARD_END

/** @} */

#endif /* EMBC_TIME_H__ */
