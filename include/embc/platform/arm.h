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
 * @brief Platform for the ARM with FreeRTOS.
 */

#ifndef EMBC_PLATFORM_ARM_FREERTOS_H_
#define EMBC_PLATFORM_ARM_FREERTOS_H_

#include "embc/platform.h"
#include "embc/assert.h"
#include <string.h>  // use memset and memcpy from the standard library

EMBC_CPP_GUARD_START

static inline uint32_t embc_clz(uint32_t x) {
    uint32_t leading_zeros;
    __asm volatile ( "clz %0, %1" : "=r" ( leading_zeros ) : "r" ( x ) );
    return leading_zeros;
}

static inline uint32_t embc_upper_power_of_two(uint32_t x) {
    uint32_t pow = 32 - embc_clz(x - 1);
    return (1 << pow);
}

static inline void embc_memset(void * ptr, int value, embc_size_t num) {
    memset(ptr, value, num);
}

static inline void embc_memcpy(void * destination, void const * source, embc_size_t num) {
    memcpy(destination, (void *) source, num);
}

EMBC_CPP_GUARD_END

/* @} */

#endif /* EMBC_PLATFORM_ARM_FREERTOS_H_ */
