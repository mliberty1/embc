/*
 * Copyright 2017-2018 Jetperch LLC
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
 * @brief
 *
 * Time-based event manager.
 */

#ifndef EMBC_EVENT_MANAGER_H_
#define EMBC_EVENT_MANAGER_H_

#include <stdint.h>
#include <stddef.h>
#include "embc/cmacro_inc.h"
#include "embc/os/mutex.h"

EMBC_CPP_GUARD_START

/**
 * @ingroup embc
 * @defgroup evm Event manager
 *
 * @brief Event manager.
 *
 * This module contains an event manager for scheduling, cancelling and
 * processing events based upon the passage of time.  This module is intended
 * to allow event scheduling within a single thread.  It provides an optional
 * locking mechanism through embc_evm_register_mutex() to support accesses
 * from multiple threads.
 *
 * @{
 */


/// The opaque event manager instance.
struct embc_evm_s;

/**
 * @brief The function prototype called when an event completes.
 *
 * @param user_data The arbitrary user data.
 * @param event_id The event that completed.
 */
typedef void (*embc_evm_callback)(void * user_data, int32_t event_id);

/**
 * @brief Get the current time in EMBC 34Q30 format.
 *
 * @param hal The HAL instance (user_data).
 * @return The current time.
 *      Usually wraps one of:
 *      - embc_time_rel(): monotonic relative to platform start.
 *      - embc_time_utc(): most accurate to real wall clock time, but may jump.
 */
typedef int64_t (*embc_evm_timestamp_fn)(struct embc_evm_s * evm);

/**
 * @brief Schedule a new event.
 *
 * @param evm The event manager instance.
 * @param timestamp The timestamp for when the event should occur.  If <= 0,
 *      then ignore and return 0.
 * @param cbk_fn The function to call at timestsmp time.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return The event_id which is provided to the callback.  The event_id
 *      can also be used to cancel the event with event_cancel.
 *      On error, return 0.
 */
typedef int32_t (*embc_evm_schedule_fn)(struct embc_evm_s * evm, int64_t timestamp,
                    embc_evm_callback cbk_fn, void * cbk_user_data);

/**
 * @brief Cancel a pending event.
 *
 * @param evm The event manager instance.
 * @param event_id The event_id returned by event_schedule().  If 0, ignore.
 * @return 0 or error code.
 */
typedef int32_t (*embc_evm_cancel_fn)(struct embc_evm_s * evm, int32_t event_id);

/**
 * @brief An abstract scheduler interface.
 *
 * This interface allows multiple clients (different modules)
 * to easily share the same scheduler.  The processing-related functions
 * are intentionally omitted, since only the main module (thread)
 * calls them.
 */
struct embc_evm_api_s {
    struct embc_evm_s * evm;
    embc_evm_timestamp_fn timestamp;
    embc_evm_schedule_fn schedule;
    embc_evm_cancel_fn cancel;
};

/**
 * Allocate a new event manager instance.
 *
 * @return The new event manager instance.
 *
 * Use embc_evm_free() to free the instance when done.
 */
EMBC_API struct embc_evm_s * embc_evm_allocate();

/**
 * @brief Free an instance previously allocated by embc_evm_allocate.
 *
 * @param self The event manager instance previous returned by embc_evm_allocate().
 */
EMBC_API void embc_evm_free(struct embc_evm_s * self);

/**
 * @brief Schedule a new event.
 *
 * @param self The event manager instance.
 * @param timestamp The timestamp for when the event should occur.  If <= 0,
 *      then ignore and return 0.
 * @param cbk_fn The function to call at timestsmp time.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return The event_id which is provided to the callback.  The event_id
 *      can also be used to cancel the event with event_cancel.
 *      On error, return 0.
 */
EMBC_API int32_t embc_evm_schedule(struct embc_evm_s * self, int64_t timestamp,
                                   embc_evm_callback cbk_fn, void * cbk_user_data);

/**
 * @brief Cancel a pending event.
 *
 * @param self The event manager instance.
 * @param event_id The event_id returned by event_schedule().  If 0, ignore.
 * @return 0 or error code.
 */
EMBC_API int32_t embc_evm_cancel(struct embc_evm_s * self, int32_t event_id);

/**
 * @brief The time for the next scheduled event.
 *
 * @param self The event manager instance.
 * @return The time for the next scheduled event.  If no events are
 *      currently pending, returns EMBC_TIME_MIN.
 */
EMBC_API int64_t embc_evm_time_next(struct embc_evm_s * self);

/**
 * @brief The time remaining until the next scheduled event.
 *
 * @param self The event manager instance.
 * @param time_current The current time.
 * @return The interval until the next scheduled event.  If no events are
 *      currently pending, returns -1.
 */
EMBC_API int64_t embc_evm_interval_next(struct embc_evm_s * self, int64_t time_current);

/**
 * @brief Process all pending events.
 *
 * @param self The event manager instance.
 * @param time_current The current time.  Any event schedule at or before this
 *      time will be processed.  The definition of the time is left to the caller,
 *      but must be shared by all clients.  Commonly selected times are:
 *      - embc_time_rel(): monotonic relative to platform start
 *      - embc_time_utc(): most accurate to real wall clock time, but may jump.
 * @return The total number of events processed.
 */
EMBC_API int32_t embc_evm_process(struct embc_evm_s * self, int64_t time_current);

/**
 * @brief Register a mutex to support blocking multi-threaded operation.
 *
 * @param self The event manager instance.
 * @param mutex The mutex instance.
 */
void embc_evm_register_mutex(struct embc_evm_s * self, embc_os_mutex_t mutex);

/**
 * @brief Populate the API.
 *
 * @param self The event manager instance.
 * @param api The API instance to populate with the default functions.
 * @return 0 or error code.
 *
 * Use embc_time_rel for api->timestamp by default.
 */
EMBC_API int32_t embc_evm_api_config(struct embc_evm_s * self, struct embc_evm_api_s * api);

/** @} */

EMBC_CPP_GUARD_END

#endif  /* EMBC_EVENT_MANAGER_H_ */
