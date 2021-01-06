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
 * @defgroup embc_pubsub Publish-Subscribe for small embedded systems
 *
 * @brief Provide a simple, opinionated publish-subscribe architecture.
 *
 * Alternatives include:
 * - [pubsub-c](https://github.com/jaracil/pubsub-c) but uses dynamic memory.
 * - [ZCM](https://zerocm.github.io/zcm/)
 *
 * @{
 */

enum embc_pubsub_type_e {
    EMBC_PUBSUB_TYPE_NULL = 0,
    EMBC_PUBSUB_TYPE_CSTR,
    EMBC_PUBSUB_TYPE_U32,
};

typedef const char * const embc_pubsub_alt_list_t[];

struct embc_pubsub_option_s {
    uint32_t value;
    embc_pubsub_alt_list_t alt;
};

typedef const struct embc_pubsub_option_s * const embc_pubsub_options_list_t[];

struct embc_pubsub_meta_s {
    const char * topic;
    const char * brief;
    const char * detail;
    enum embc_pubsub_type_e type;
    embc_pubsub_options_list_t options;
};

struct embc_pubsub_value_s {
    enum embc_pubsub_type_e type;
    union {
        const char * cstr;
        uint32_t u32;
    } value;
};

struct embc_pubsub_s;

typedef void (*embc_pubsub_subscribe_fn)(void * user_data, const char * topic, const struct embc_pubsub_value_s * value);

struct embc_pubsub_s * embc_pubsub_initialize();

void embc_pubsub_finalize(struct embc_pubsub_s * self);

int32_t embc_pubsub_register(struct embc_pubsub_s * self, const struct embc_pubsub_meta_s * meta, const struct embc_pubsub_value_s * value);

int32_t embc_pubsub_register_cstr(struct embc_pubsub_s * self, const struct embc_pubsub_meta_s * meta, const char * value);

int32_t embc_pubsub_register_u32(struct embc_pubsub_s * self, const struct embc_pubsub_meta_s * meta, uint32_t value);

int32_t embc_pubsub_subscribe(struct embc_pubsub_s * self, const char * topic, embc_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

int32_t embc_pubsub_publish(struct embc_pubsub_s * self, const char * topic, const struct embc_pubsub_value_s * value);

int32_t embc_pubsub_publish_u32(struct embc_pubsub_s * self, const char * topic, uint32_t value);

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

void embc_pubsub_print(struct embc_pubsub_s * self);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PUBSUB_H__ */
