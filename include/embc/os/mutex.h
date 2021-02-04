/*
 * Copyright 2021 Jetperch LLC
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
 * @brief OS Mutex abstraction.
 */

#ifndef EMBC_OS_MUTEX_H__
#define EMBC_OS_MUTEX_H__

#include "embc/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_os_mutex OS Mutex abstraction.
 *
 * @brief Provide a simple OS mutex abstraction.
 *
 * Although EMBC attempts to avoid OS calls, mutexes occur
 * frequently enough that the EMBC library standardizes
 * on a convention.
 *
 * @{
 */

/**
 * @brief Allocate a new mutex.
 *
 * @return The mutex or 0.
 */
embc_os_mutex_t embc_os_mutex_alloc();

/**
 * @brief Free an existing mutex (not recommended).
 *
 * @param mutex The mutex to free, previous produced using embc_os_mutex_alloc().
 */
void embc_os_mutex_free(embc_os_mutex_t mutex);

/**
 * @brief Lock a mutex.
 *
 * @param mutex The mutex to lock.
 *
 * Be sure to call embc_os_mutex_unlock() when done.
 *
 * This function will use the default platform timeout.
 * An lock that takes longer than the timeout indicates
 * a system failure.  In deployed embedded systems, this
 * should trip the watchdog timer.
 */
void embc_os_mutex_lock(embc_os_mutex_t mutex);

/**
 * @brief Unlock a mutex.
 *
 * @param mutex The mutex to unlock, which was previously locked
 *      with embc_os_mutex_lock().
 */
void embc_os_mutex_unlock(embc_os_mutex_t mutex);


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_OS_MUTEX_H__ */
