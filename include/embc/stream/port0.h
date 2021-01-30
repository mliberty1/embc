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
 * @brief Port 0 definitions.
 */

#ifndef EMBC_STREAM_PORT0_H__
#define EMBC_STREAM_PORT0_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_port0 Transport Port 0
 *
 * @brief Transport port 0.
 *
 *
 * Port 0 allocates port_data:
 *      port_data[15:8]: cmd_meta defined by each command
 *      port_data[7]: 0=request or unused, 1=response
 *      port_data[6:3]: reserved, set to 0
 *      port_data[2:0]: The embc_port0_op_e operation.
 * @{
 */

enum embc_port0_op_e {
    EMBC_PORT0_OP_UNKNOWN = 0,
    EMBC_PORT0_OP_STATUS = 1,       // cmd_meta=0, rsp_payload=embc_dl_status_s
    EMBC_PORT0_OP_ECHO = 2,         // cmd_meta=0
    EMBC_PORT0_OP_TIMESYNC = 3,     // cmd_meta=0, payload=3x64-bit times: [src tx, tgt rx, tgt tx]
    EMBC_PORT0_OP_META = 4,         // cmd_meta=port_id
    EMBC_PORT0_OP_RAW = 5,          // raw UART loopback mode request
};



#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* EMBC_STREAM_PORT0_H__ */
