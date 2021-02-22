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

#include "embc/os/mutex.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_pubsub Publish-Subscribe for small embedded systems.
 *
 * @brief A simple, opinionated Publish-Subscribe architecture.
 *
 * This modules provides a publish-subscribe implementation suitable
 * for small embedded microcontrollers.  Features include:
 * - Multiple data types including u32, str, json, binary
 * - Constant pointers for more efficient memory usage
 * - Retained messages
 * - Topic metadata for automatically populating user interfaces.
 * - Designed to easily support distributed instances in
 *   [polytree](https://en.wikipedia.org/wiki/Polytree) topology.
 *   - Concept of topic prefix ownership.
 *   - Query retained messages
 *   - Asynchronous error reporting
 *   - State recovery in the event that any pubsub instance resets.
 *   - Automatic topic routing using pubsub_port.h.
 * - Thread-safe, in-order operation.  All updates queued and processed from
 *   the pubsub context.  Subscribers can safely publish topic updates from
 *   the subscriber callback.
 *
 *
 * ## Topics
 *
 * Topic names are any valid UTF-8.  However, we highly recommend sticking
 * to standard letters and numbers.  The following symbols are reserved:
 * /?#$'"`&
 *
 * Topics are hierarchical, and each level of the hierarchy is separated by
 * '/'.  The topic should NOT start with '/'.  We recommend using short
 * names or abbreviations, including single letters, to keep the topic
 * string short.  Topics should be 30 bytes or less.  Topic storages is
 * limited to 32 bytes by default which includes the operation suffix
 * character and the null termination.  This small topic size supports
 * memory-constrained devices.  Topics can have metadata to provide
 * user-meaningful names and descriptions.
 *
 * Topics that end in '$' are the JSON metadata for the associated topic
 * without the $.  Most microcontrollers should use both CONST and RETAIN
 * flags for metadata.  The metadata format is JSON with the following
 * keys:
 * - dtype: one of ['u32', 'str', 'json', 'bin']
 * - brief: A brief string description (recommended).
 * - detail: A more detailed string description (optional).
 * - default: The recommended default value (optional).
 * - options: A list of options, which is each a list of:
 *      [value, [alt1 [, ...]]]
 *      The alternates should be given in order.  The first value
 *      must be the value as dtype.  The second value alt1
 *      (when provided) is used to automatically populate user
 *      interfaces, and it can be the same as value.  Additional
 *      values will be interpreted as equivalents.
 *  - flags: A list of flags for this topic.  Options include:
 *    - read_only: This topic cannot be updated.
 *    - hidden: This topic should not appear in the user interface.
 *    - dev: Developer option that should not be used in production.
 *
 * To re-enumerate all metadata, publish NULL to "$" or "t/h/$".
 * This implementation recognizes this request, and will publish
 * all metadata instances to "topic/hierarchy/name$".  Each pubsub instance
 * will respond to the matching topics it owns.
 *
 * To query a value, publish NULL to "?", "t/h/?", or "t/h/n?".
 * The implementation recognizes this request,
 * and will publish all matching "t/h/n?" retained values.  Each pubsub
 * instance will respond to the matching topics it owns.  This mechanism
 * provides state recovery in the event that a host pubsub instance resets.
 *
 * If publishing to a topic owned by a pubsub instance fails, then that
 * instance will publish to "t/h/n#".  The value is a string containing
 * the error code, a space, then the error message.
 *
 * Alternatives include:
 * - [pubsub-c](https://github.com/jaracil/pubsub-c) but uses dynamic memory.
 * - [ZCM](https://zerocm.github.io/zcm/)
 *
 * @{
 */

#define EMBC_PUBSUB_TOPIC_LENGTH_MAX (32)
#define EMBC_PUBSUB_TOPIC_LENGTH_PER_LEVEL (8)
#define EMBC_PUBSUB_DTYPE_MASK (0x0f)
#define EMBC_PUBSUB_DFLAG_MASK (0xf0)


/// The allowed data types.
enum embc_pubsub_dtype_e {
    EMBC_PUBSUB_DTYPE_NULL = 0, ///< NULL value.  Also used to clear existing value.
    EMBC_PUBSUB_DTYPE_U32 = 1,  ///< Unsigned 32-bit integer value.
    EMBC_PUBSUB_DTYPE_STR = 4,  ///< UTF-8 string value, null terminated.
    EMBC_PUBSUB_DTYPE_JSON = 5, ///< UTF-8 JSON string value, null terminated.
    EMBC_PUBSUB_DTYPE_BIN = 6,  ///< Raw binary value
};

enum embc_pubsub_dflag_e {
    /// No flags specified.
    EMBC_PUBSUB_DFLAG_NONE = 0,

    /// The PubSub instance should retain this value.
    EMBC_PUBSUB_DFLAG_RETAIN = (1 << 4),

    /// The value points to a const that will remain valid indefinitely.
    EMBC_PUBSUB_DFLAG_CONST = (1 << 5),

    /// The value is a response to a direct "?" query.
    EMBC_PUBSUB_DFLAG_QUERY = (1 << 6),
};

#define EMBC_PUBSUB_DTYPE_MASK (0x0f)
#define EMBC_PUBSUB_DFLAG_MASK (0xf0)


/// The actual value holder for embc_pubsub_value_s.
union embc_pubsub_value_inner_u {
    const char * str;      ///< EMBC_PUBSUB_TYPE_STR, EMBC_PUBSUB_DTYPE_JSON
    const uint8_t * bin;   ///< EMBC_PUBSUB_TYPE_BIN
    uint32_t u32;          ///< EMBC_PUBSUB_TYPE_U32
};

/// The value holder for all types.
struct embc_pubsub_value_s {
    /**
     * @brief The data format indicator
     *
     * type[3:0] = DTYPE
     * type[7:4] = DFLAGS
     */
    uint8_t type;

    /// The actual value.
    union embc_pubsub_value_inner_u value;
    uint32_t size;             ///< payload size for pointer types.
};

#define embc_pubsub_null() ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_NULL, .value={.u32=0}, .size=0})
#define embc_pubsub_null_r() ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_NULL | EMBC_PUBSUB_DFLAG_RETAIN, .value={.u32=0}, .size=0})
#define embc_pubsub_u32(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_U32, .value={.u32=_value}, .size=0})
#define embc_pubsub_u32_r(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_U32 | EMBC_PUBSUB_DFLAG_RETAIN, .value={.u32=_value}, .size=0})

#define embc_pubsub_str(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_STR, .value={.str=_value}, .size=0})
#define embc_pubsub_cstr(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_STR | EMBC_PUBSUB_DFLAG_CONST, .value={.str=_value}, .size=0})
#define embc_pubsub_cstr_r(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_STR | EMBC_PUBSUB_DFLAG_CONST | EMBC_PUBSUB_DFLAG_RETAIN, .value={.str=_value}, .size=0})

#define embc_pubsub_json(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_JSON, .value={.str=_value}, .size=0})
#define embc_pubsub_cjson(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_JSON | EMBC_PUBSUB_DFLAG_CONST, .value={.str=_value}, .size=0})
#define embc_pubsub_cjson_r(_value) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_JSON | EMBC_PUBSUB_DFLAG_CONST | EMBC_PUBSUB_DFLAG_RETAIN, .value={.str=_value}, .size=0})

#define embc_pubsub_bin(_value, _size) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_BIN, .value={.bin=_value}, .size=_size})
#define embc_pubsub_cbin(_value, _size) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_BIN | EMBC_PUBSUB_DFLAG_CONST, .value={.bin=_value}, .size=_size})
#define embc_pubsub_cbin_r(_value, _size) ((struct embc_pubsub_value_s){.type=EMBC_PUBSUB_DTYPE_BIN | EMBC_PUBSUB_DFLAG_CONST | EMBC_PUBSUB_DFLAG_RETAIN, .value={.bin=_value}, .size=_size})

/// The opaque PubSub instance.
struct embc_pubsub_s;

/**
 * @brief Function called on topic updates.
 *
 * @param user_data The arbitrary user data.
 * @param topic The topic for this update.
 * @param value The value for this update.
 * @return 0 or error code.
 */
typedef uint8_t (*embc_pubsub_subscribe_fn)(void * user_data,
        const char * topic, const struct embc_pubsub_value_s * value);

/**
 * @brief Function called whenever a new message is published.
 *
 * @param user_data Arbitrary user data.
 *
 * When used in a threaded environment, this function can signal the
 * thread that it should call embc_pubsub_process().  This automatic
 * hook often eliminates the need for more complicated wrappers.
 */
typedef void (*embc_pubsub_on_publish_fn)(void * user_data);

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
 */
typedef int32_t (*embc_pubsub_publish_fn)(
        struct embc_pubsub_s * self,
        const char * topic, const struct embc_pubsub_value_s * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data);

/**
 * @brief Create and initialize a new PubSub instance.
 *
 * @param topic_prefix The topic prefix that is owned by this
 *      pubsub instance.  This instance will reply to metadata and
 *      query requests for all topics starting with this prefix.
 * @param buffer_size The buffer size for dynamic pointer messages.
 *      0 prohibits non-CONST pointer types.
 * // todo add authority topic for general requests like $ (meta) and ? (get retained).
 * @return The new PubSub instance.
 */
struct embc_pubsub_s * embc_pubsub_initialize(const char * topic_prefix, uint32_t buffer_size);

/**
 * @brief Finalize the instance and free resources.
 *
 * @param self The PubSub instance.
 */
void embc_pubsub_finalize(struct embc_pubsub_s * self);

/**
 * @brief Get the topic prefix that is owned by this instance.
 *
 * @param self The PubSub instance.
 * @return The topic prefix provided to embc_pubsub_initialize().
 */
const char * embc_pubsub_topic_prefix(struct embc_pubsub_s * self);

/**
 * @brief Register the function called for each call to embc_pubsub_publish().
 *
 * @param self The PubSub instance.
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
 *      within embc_pubsub_subscribe() with any retained values.
 *      Future invocations are from embc_pubsub_process().
 *      The cbk_fn is responsible for any thread resynchronization.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return 0 or error code.
 * @see embc_pubsub_unsubscribe()
 * @see embc_pubsub_subscribe_link()
 *
 * If the topic does not already exist, this function will
 * automatically create it.
 *
 * Normal subscribers on receive topic publish values.  They do not receive
 * metadata and topic get requests or responses.
 * Use embc_pubsub_subscribe_link() for a subscriber that wants
 * to also receive these extra messages.
 */
int32_t embc_pubsub_subscribe(struct embc_pubsub_s * self, const char * topic,
        embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Unsubscribe from a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to unsubscribe.
 * @param cbk_fn The function provided to embc_pubsub_subscribe().
 * @param cbk_user_data The arbitrary data provided to embc_pubsub_subscribe().
 * @return 0 or error code.
 * @see embc_pubsub_subscribe()
 */
int32_t embc_pubsub_unsubscribe(struct embc_pubsub_s * self, const char * topic,
                                embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Subscribe to provide a link to a distributed pubsub instance.
 *
 * @param self This PubSub instance.
 * @param cbk_fn The function to call on topic updates.
 *      Unlike with embc_pubsub_subscribe(), cbk_fn is not initially called
 *      with retained values.  Use "?" to get retained values as needed.
 *      This function will be called from embc_pubsub_process().
 *      The cbk_fn is responsible for any thread resynchronization.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return 0 or error code.
 * @see embc_pubsub_unsubscribe
 *
 * Link subscribers function just like normal subscribers.  However, this
 * PubSub instance will pass metadata and get requests to link subscribers.
 * Normal subscribers do not receive these requests.
 *
 * The distributed PubSub tree hierarchy is formed by the topic subscriptions.
 * The server PubSub instance subscribes to all topics in client PubSub
 * instances.  Client PubSub instances subscribe to their topic_prefix along
 * with all topic_prefix for any PubSub instances for which they are a server.
 * The pubsub_port implementation provides hooks for this feature.
 */
int32_t embc_pubsub_subscribe_link(struct embc_pubsub_s * self, const char * topic,
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
 * When posting requests, including metadata requests,
 * providing src_fn will limit the response to only that callback.
 *
 * This modules supports two types of pointer types.  Values marked with
 * the EMBC_PUBSUB_DFLAG_CONST remain owned by the caller.  The values
 * must remain valid until the pubsub instance completes publishing.
 * If also marked with EMBC_PUBSUB_DFLAG_RETAIN, the value must remain
 * valid until a new value publishes.  We only recommend using this
 * method with "static const" values.  Note that properly freeing a
 * pointer type is not trivial, since publishing is asynchronous and
 * subscriber calling order is not guaranteed.
 *
 * One "trick" to freeing pointers is to publish two messages:
 * first one with the pointer and then one with NULL.
 * If the publisher also subscribes, then they can free the pointer
 * when they receive the NULL value.  However, this requires that
 * all subscribers only operated on the pointer during the subscriber
 * callback and do not hold on to it.
 *
 * The second pointer type is dynamically managed by the pubsub instance.
 * EMBC_PUBSUB_DFLAG_RETAIN is NOT allowed for these pointer types as
 * they are only temporarily allocated in a circular buffer.
 * If the item is too big to ever fit, this function returns
 * EMBC_ERROR_PARAMETER_INVALID.
 * If the circular buffer is full, this function returns
 * EMBC_ERROR_NOT_ENOUGH_MEMORY.  The caller can optionally wait and retry.
 */
int32_t embc_pubsub_publish(struct embc_pubsub_s * self,
        const char * topic, const struct embc_pubsub_value_s * value,
        embc_pubsub_subscribe_fn src_fn, void * src_user_data);

/**
 * @brief Convenience function to set the topic metadata.
 *
 * @param self The PubSub instance.
 * @param topic The topic name.
 * @param meta_json The JSON-formatted UTF-8 metadata string for the topic.
 * @return 0 or error code.
 *
 * Although you can use embc_pubsub_publish() with topic + '$' and
 * a const, retained JSON string, this function simplifies the
 * metadata call.
 */
int32_t embc_pubsub_meta(struct embc_pubsub_s * self, const char * topic, const char * meta_json);

/**
 * @brief Get the local, retained value for a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic name.
 * @param[out] value The current value for topic.  Since this request is
 *      handled in the caller's thread, it does not account
 *      for any updates queued for embc_pubsub_process().
 * @return 0 or error code.
 *
 * For a distributed PubSub implementation, use topic? to get the retained
 * value directly from the owning PubSub instance.
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
 * @brief Register functions to lock and unlock the send-side mutex.
 *
 * @param self The instance.
 * @param mutex The mutex instance for accessing self.
 */
void embc_pubsub_register_mutex(struct embc_pubsub_s * self, embc_os_mutex_t mutex);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PUBSUB_H__ */
