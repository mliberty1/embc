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
 * @brief Stream stack.
 */

#ifndef EMBC_STREAM_STACK_H__
#define EMBC_STREAM_STACK_H__

#include <stdint.h>
#include <stdbool.h>
#include "embc/stream/data_link.h"
#include "embc/stream/transport.h"
#include "embc/stream/port0.h"
#include "embc/stream/pubsub.h"
#include "embc/stream/pubsub_port.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_stack Full stream stack
 *
 * @brief Full stream stack.
 *
 * @{
 */

/**
 * @brief The stack object.
 *
 * The stack is intentionally transparent so that applications
 * can reach the pieces parts if needed.  However, use direct
 * access with care as it makes your code more brittle.
 */
struct embc_stack_s {
    struct embc_dl_s * dl;
    struct embc_transport_s * transport;
    struct embc_port0_s * port0;
    struct embc_pubsub_s * pubsub;
    struct embc_pubsubp_s * pubsub_port;
};

/**
 * Initialize the communication stack.
 *
 * @param config The data-link layer configuration.
 * @param port0_mode The communication link mode: host or client.
 * @param port0_topic_prefix The prefix for port0 updates.
 * @param ll_instance The lower-level communication implementation.
 * @param evm_api The event manager API.
 * @param pubsub The pubsub instance for this device.
 * @return The stack instance or NULL on error.
 */
struct embc_stack_s * embc_stack_initialize(
        struct embc_dl_config_s const * config,
        enum embc_port0_mode_e port0_mode,
        const char * port0_topic_prefix,
        struct embc_evm_api_s * evm_api,
        struct embc_dl_ll_s const * ll_instance,
        struct embc_pubsub_s * pubsub
);

/**
 * Finalize the communication stack and free all resources.
 *
 * @param self The stack instance.
 * @return 0 or error code.
 */
int32_t embc_stack_finalize(struct embc_stack_s * self);

/**
 * @brief Process to handle retransmission.
 *
 * @param self The instance.
 *
 * todo eliminate this function, use embc_evm_schedule() and embc_evm_process()
 */
void embc_stack_process(struct embc_stack_s * self);

/**
 * @brief Set the mutex used by the stack.
 *
 * @param self The stack instance.
 * @param mutex The mutex to use.  Provide NULL to clear.
 */
void embc_stack_mutex_set(struct embc_stack_s * self, embc_os_mutex_t mutex);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_STACK_H__ */
