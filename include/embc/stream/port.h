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
 * @brief Port API.
 */

#ifndef EMBC_STREAM_PORT_API_H__
#define EMBC_STREAM_PORT_API_H__

#include <stdint.h>
#include <stdbool.h>
#include "transport.h"
#include "data_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_port Transport Port API
 *
 * @brief Transport port API.
 *
 * @{
 */


/**
 * @brief Initialize a port instance.
 *
 * @param user_data The arbitrary user data (the port instance).
 * @param pubsub The pubsub instance.  Most ports should use pubsub to
 *      communicate with the rest of the system.  However, fast data relay
 *      ports may provide a direct interface for higher performance.
 * @param topic_prefix The topic prefix, empty or ending with '/', that this
 *      port should use for all topics that it published to pubsub.
 * @param transport The transport instance for sending data.
 * @param port_id The port's port_id for sending data.
 * @param evm The event manager, which this port can use to schedule callback
 *      events.
 * @return 0 or error code.
 */
typedef int32_t embc_port_initialize_fn(void * user_data,
                                        struct embc_pubsub_s * pubsub,
                                        const char * topic_prefix,
                                        struct embc_transport_s * transport,
                                        uint8_t port_id,
                                        struct embc_evm_api_s * evm);

/**
 * @brief Finalize a port instance.
 *
 * @param user_data The arbitrary user data (the port instance).
 * @return 0 or error code.
 *
 * If the implementation supports it, free all resources.
 */
typedef int32_t embc_port_finalize(void * user_data);

/**
 * @brief The port API, used by the transport layer to interact
 *      with (potentially) dynamically instantiated ports.
 */
struct embc_port_api_s {
    /// The arbitrary port data.
    void * user_data;

    /**
     * @brief The JSON metadata structure defining the port.
     */
    const char * meta;
    embc_port_initialize_fn initialize;
    embc_port_finalize finalize;
    embc_transport_event_fn on_event;
    embc_transport_recv_fn on_recv;
};


#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PORT_API_H__ */
