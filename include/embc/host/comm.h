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
 * @brief UART data link.
 */

#ifndef EMBC_HOST_COMM_H__
#define EMBC_HOST_COMM_H__

#include <stdint.h>
#include "embc/stream/data_link.h"
#include "embc/stream/pubsub.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The opaque communications object.
struct embc_comm_s;

/**
 * @brief Initialize a new host communications interface.
 *
 * @param config The data link configuration.
 * @param device The device name.
 * @param baudrate The baud rate, for UART devices.
 * @param cbk_fn The function to call on topic updates.  This function
 *      must remain valid until embc_comm_finalize().
 * @param cbk_user_data The arbitrary data for cbk_fn().
 * @param topics The list of topic prefixes to forward
 *      from pubsub out the communication interface.  Each topic
 *      must be separated by \x1F (unit separator).  The stack
 *      subscribes upon connection.
 * @return The new instance or NULL.
 */
struct embc_comm_s * embc_comm_initialize(struct embc_dl_config_s const * config,
                                          const char * device,
                                          uint32_t baudrate,
                                          embc_pubsub_subscribe_fn cbk_fn,
                                          void * cbk_user_data,
                                          const char * topics);

/**
 * @brief Finalize the comm instance and free all resources.
 *
 * @param self The communications instance.
 */
void embc_comm_finalize(struct embc_comm_s * self);

/**
 * @brief Publish to a topic.
 *
 * @param self The communications instance.
 * @param topic The topic to update.
 * @param value The new value for the topic.
 * @return 0 or error code.
 */
int32_t embc_comm_publish(struct embc_comm_s * self,
                          const char * topic, const struct embc_pubsub_value_s * value);

/**
 * @brief Get the retained value for a topic.
 *
 * @param self The communications instance.
 * @param topic The topic name.
 * @param[out] value The current value for topic.  Since this request is
 *      handled in the caller's thread, it does not account
 *      for any updates queued.
 * @return 0 or error code.
 */
int32_t embc_comm_query(struct embc_comm_s * self, const char * topic, struct embc_pubsub_value_s * value);

/**
 * @brief Get the status for the data link.
 *
 * @param self The data link instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
int32_t embc_comm_status_get(
        struct embc_comm_s * self,
        struct embc_dl_status_s * status);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_HOST_COMM_H__ */
