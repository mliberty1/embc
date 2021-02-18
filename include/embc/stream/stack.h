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

struct embc_stack_s {
    struct embc_dl_s * dl;
    struct embc_transport_s * transport;
    struct embc_port0_s * port0;
    enum embc_port0_mode_e port0_mode;
    struct embc_pubsub_s * pubsub;
    struct embc_pubsubp_s * pubsub_port;
};

struct embc_stack_s * embc_stack_initialize(
        struct embc_dl_config_s const * config,
        enum embc_port0_mode_e port0_mode,
        const char * port0_topic_prefix,
        struct embc_evm_api_s const * evm,
        struct embc_dl_ll_s const * ll_instance,
        struct embc_pubsub_s * pubsub
);

int32_t embc_stack_finalize(struct embc_stack_s * self);

/**
 * @brief Process to handle retransmission.
 *
 * @param self The instance.
 */
void embc_stack_process(struct embc_stack_s * self);

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_STACK_H__ */
