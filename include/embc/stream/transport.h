/*
 * Copyright 2014-2020 Jetperch LLC
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
 * @brief Transport layer for byte streams.
 */

#ifndef EMBC_STREAM_TRANSPORT_H__
#define EMBC_STREAM_TRANSPORT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @ingroup embc
* @defgroup embc_transport Transport layer for byte streams
*
* @{
*/

enum embc_transport_seq_e {
EMBC_TRANSPORT_SEQ_MIDDLE = 0,
EMBC_TRANSPORT_SEQ_STOP = 1,
EMBC_TRANSPORT_SEQ_START = 2,
EMBC_TRANSPORT_SEQ_SINGLE = 3,
};

/**
* @brief The metadata fields
*/
struct embc_transport_metadata_s {
    uint8_t port_id;
    uint8_t seq;        // enum embc_transport_seq_e
    uint8_t message_id;
    uint8_t appdata;
};

#if 0
/**
 * @brief The commands defined for port 0.
 */
enum embc_framer_port0_cmd_e {
    EMBC_TRANSPORT_PORT0_IGNORE = 0x00,
    EMBC_TRANSPORT_PORT0_CONNECT = 0x01,
    EMBC_TRANSPORT_PORT0_CONNECT_ACK = 0x81,
    EMBC_TRANSPORT_PORT0_PING_REQ = 0x02,
    EMBC_TRANSPORT_PORT0_PING_RSP = 0x82,
    EMBC_TRANSPORT_PORT0_STATUS_REQ = 0x03,
    EMBC_TRANSPORT_PORT0_STATUS = 0x83,
    EMBC_TRANSPORT_PORT0_LOOPBACK_REQ = 0x05,
    EMBC_TRANSPORT_PORT0_LOOPBACK_RSP = 0x85,
};
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_TRANSPORT_H__ */
