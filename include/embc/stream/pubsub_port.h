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
 * @brief Port adapter for publish-subscribe.
 */

#ifndef EMBC_STREAM_PUBSUB_PORT_H__
#define EMBC_STREAM_PUBSUB_PORT_H__

#include "transport.h"
#include "pubsub.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_pubsub_port Publish-Subscribe port adapter.
 *
 * @brief Connect Publish-Subscribe to a transport port.
 *
 * @{
 */

/// The opaque PubSub port instance.
struct embc_pubsubp_s;

/**
 * @brief Create and initialize a new PubSub port instance.
 *
 * @return The new PubSub port instance.
 */
struct embc_pubsubp_s * embc_pubsubp_initialize();

/**
 * @brief Finalize the instance and free resources.
 *
 * @param self The PubSub port instance.
 */
void embc_pubsubp_finalize(struct embc_pubsubp_s * self);

/**
 * @brief Register the pubsub instance.
 * @param self The PubSub port instance.
 * @param publish_fn Normally embc_pubsub_publish
 * @param pubsub The pubsub instance.
 */
void embc_pubsubp_pubsub_register(struct embc_pubsubp_s * self,
                                  embc_pubsub_publish_fn publish_fn, struct embc_pubsub_s * pubsub);

/**
 * @brief Register the transport instance.
 *
 * @param self The PubSub port instance.
 * @param port_id The port id.
 * @param send_fn The send function, normally embc_transport_send.
 * @param transport The transport instance.
 */
void embc_pubsubp_transport_register(struct embc_pubsubp_s * self,
                                     uint8_t port_id,
                                     embc_transport_send_fn send_fn, struct embc_transport_s * transport);

/**
 * @brief The function called on events.
 *
 * @param self The pubsub port instance.
 * @param event The signaled event.
 *
 * Can safely cast to embc_transport_event_fn.
 */
void embc_pubsubp_on_event(struct embc_pubsubp_s *self, enum embc_dl_event_e event);

/**
 * @brief The function called upon message receipt.
 *
 * @param self The pubsub port instance.
 * @param port_id The port id for this port.
 * @param seq The frame reassembly information.
 * @param port_data The arbitrary 16-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 *
 * Can safely cast to embc_transport_recv_fn.
 */
void embc_pubsubp_on_recv(struct embc_pubsubp_s *self,
                          uint8_t port_id,
                          enum embc_transport_seq_e seq,
                          uint16_t port_data,
                          uint8_t *msg, uint32_t msg_size);

/**
 * @brief Function called on topic updates.
 *
 * @param user_data The arbitrary user data.
 * @param topic The topic for this update.
 * @param value The value for this update.
 * @return 0 or error code.
 *
 * Can safely cast to embc_pubsub_subscribe_fn.
 */
uint8_t embc_pubsubp_on_update(struct embc_pubsubp_s *self,
                               const char * topic, const struct embc_pubsub_value_s * value);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PUBSUB_PORT_H__ */
