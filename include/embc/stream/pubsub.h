/*
 * Copyright 2020 Jetperch LLC
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
 * @brief Trivial publish-subscribe.
 */

#ifndef EMBC_STREAM_PUBSUB_H__
#define EMBC_STREAM_PUBSUB_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_pubsub Publish-Subscribe for small embedded systems.
 *
 * @brief Provide a simple, opinionated Publish-Subscribe architecture.
 *
 * Alternatives include:
 * - [pubsub-c](https://github.com/jaracil/pubsub-c) but uses dynamic memory.
 * - [ZCM](https://zerocm.github.io/zcm/)
 *
 * @{
 */

/// The allowed data types.
enum embc_pubsub_type_e {
    EMBC_PUBSUB_TYPE_NULL = 0,
    EMBC_PUBSUB_TYPE_CSTR,
    EMBC_PUBSUB_TYPE_U32,
};

/// The value holder for all types.
struct embc_pubsub_value_s {
    /// The value type indicator.
    enum embc_pubsub_type_e type;
    /// The actual value.
    union {
        const char * cstr;  ///< EMBC_PUBSUB_TYPE_CSTR
        uint32_t u32;       ///< EMBC_PUBSUB_TYPE_U32
    } value;
};

/// The opaque PubSub instance.
struct embc_pubsub_s;

/**
 * @brief Function called on topic updates.
 *
 * @param user_data The arbitrary user data.
 * @param topic The topic for this update.
 * @param value The value for this update.
 */
typedef void (*embc_pubsub_subscribe_fn)(void * user_data, const char * topic, const struct embc_pubsub_value_s * value);

/**
 * @brief Function called whenever a new message is published.
 *
 * @param user_data Arbitrary user data.
 */
typedef void (*embc_pubsub_on_publish_fn)(void * user_data);

/**
 * @brief Create and initialize a new PubSub instance.
 *
 * @return The new PubSub instance.
 */
struct embc_pubsub_s * embc_pubsub_initialize();

/**
 * @brief Finalize the instance and free resources.
 *
 * @param self The PubSub instance.
 */
void embc_pubsub_finalize(struct embc_pubsub_s * self);

/**
 * @brief Register the function called for each call to embc_pubsub_publish().
 *
 * @param self The PubSub instance
 * @param cbk_fn The callback function.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 *
 * Threaded implementations can use this callback to set an event,
 * task notification, or file handle to tell the thread that
 * embc_pubsub_process() should be invoked.
 */
void embc_pubsub_register_on_publish(struct embc_pubsub_s * self,
        embc_pubsub_on_publish_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Subscribe to a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to subscribe.
 * @param cbk_fn The function to call on topic updates.
 *      This function may be initially called from
 *      within embc_pubsub_subscribe().  Future invocations
 *      are from embc_pubsub_process().  The cbk_fn is responsible
 *      for any thread resynchronization.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return 0 or error code.
 *
 * If the topic does not already exist, this function will
 * automatically create it.
 */
int32_t embc_pubsub_subscribe(struct embc_pubsub_s * self, const char * topic,
        embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Publish to a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to update.
 * @param value The new value for the topic.
 * @param src_fn The callback function for the source subscriber
 *      that is publishing the update.  Can be NULL.
 * @param src_user_data The arbitrary user data for the source subscriber
 *      callback function.
 * @return 0 or error code.
 *
 * If the topic does not already exist, this function will
 * automatically create it.
 *
 * The src_fn and src_user_data provide trivial, built-in support
 * to ensure that a publisher/subscriber does not receive their
 * own updates.  One deduplication technique is to compare values.
 * However, some pub/sub instances, such as communication bridges
 * between PubSub instances, would need to maintain a lot of state.
 * This simple approach great simplifies implementing a
 * subscriber that also publishes to the same topics.
 *
 * Any pointer types, such as cstr, must remain valid indefinitely.
 * One "trick" to freeing pointers is to publish two messages:
 * first one with the pointer and then one with NULL.
 * If the publisher also subscribes, then they can free the pointer
 * when they receive the NULL value.  However, this requires that
 * all subscribers only operated on the pointer during the subscriber
 * callback and do not hold on to it.
 */
int32_t embc_pubsub_publish(struct embc_pubsub_s * self,
        const char * topic, const struct embc_pubsub_value_s * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data);

/// Convenience wrapper for embc_pubsub_publish
static inline int32_t embc_pubsub_publish_cstr(
        struct embc_pubsub_s * self,
        const char * topic, const char * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data) {
    struct embc_pubsub_value_s s;
    s.type = EMBC_PUBSUB_TYPE_CSTR;
    s.value.cstr = value;
    return embc_pubsub_publish(self, topic, &s, src_fn, src_user_data);
}

/// Convenience wrapper for embc_pubsub_publish
static inline int32_t embc_pubsub_publish_u32(
        struct embc_pubsub_s * self,
        const char * topic, uint32_t value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data) {
    struct embc_pubsub_value_s s;
    s.type = EMBC_PUBSUB_TYPE_U32;
    s.value.u32 = value;
    return embc_pubsub_publish(self, topic, &s, src_fn, src_user_data);
}

/**
 * @brief Get the retained value for a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic name.
 * @param[out] value The current value for topic.  Since this request is
 *      handled in the caller's thread, it does not account
 *      for any updates queued for embc_pubsub_process().
 * @return 0 or error code.
 */
int32_t embc_pubsub_query(struct embc_pubsub_s * self, const char * topic, struct embc_pubsub_value_s * value);

/**
 * @brief Process all outstanding topic updates.
 *
 * @param self The PubSub instance to process.
 *
 * Many implementation choose to run this from a unique thread.
 *
 */
void embc_pubsub_process(struct embc_pubsub_s * self);

/**
 * @brief The function used to lock the mutex
 *
 * @param user_data The arbitrary data.
 */
typedef void (*embc_pubsub_lock)(void * user_data);

/**
 * @brief The function used to unlock the mutex
 *
 * @param user_data The arbitrary data.
 */
typedef void (*embc_pubsub_unlock)(void * user_data);

/**
 * @brief Register functions to lock and unlock the send-side mutex.
 *
 * @param self The instance.
 * @param lock The function called to lock the mutex.
 * @param unlock The function called to unlock the mutex.
 * @param user_data The arbitrary data passed to lock and unlock.
 */
void embc_pubsub_register_lock(struct embc_pubsub_s * self, embc_pubsub_lock lock, embc_pubsub_unlock unlock, void * user_data);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PUBSUB_H__ */
