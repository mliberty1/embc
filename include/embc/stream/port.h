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

struct embc_port_s {
    void * user_data;
    int32_t (*initialize)(void * user_data, struct embc_evm_api_s * evm);
    embc_transport_event_fn on_event;
    embc_transport_recv_fn on_recv;
};

typedef void (*embc_dl_event_callback)(void * user_data, int32_t event_id);

typedef int32_t (*embc_dl_event_schedule(void * user_data, int64_t timestamp,
                                         embc_dl_event_callback cbk_fn, void * cbk_user_data);

typedef int32_t (*embc_dl_event_cancel)(void * user_data, int32_t event_id);


struct embc_port_ll_s {
    void *user_data;
    embc_transport_ll_send send;
    embc_dl_event_schedule event_schedule;
    embc_dl_event_cancel event_cancel;
};

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PORT_API_H__ */
