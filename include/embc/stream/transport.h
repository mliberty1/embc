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
#include "embc/stream/data_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup embc
 * @defgroup embc_transport Transport layer for byte streams
 *
 * @brief Provide port multiplexing and segmentation / reassembly.
 *
 * @{
 */


#define EMBC_TRANSPORT_PORT_MAX (0x3FU)

enum embc_transport_seq_e {
    EMBC_TRANSPORT_SEQ_MIDDLE = 0,
    EMBC_TRANSPORT_SEQ_STOP = 1,
    EMBC_TRANSPORT_SEQ_START = 2,
    EMBC_TRANSPORT_SEQ_SINGLE = 3,
};


/**
 * @brief The API event callbacks to the upper layer.
 */
struct embc_transport_api_s {
    /// The arbitrary user data.
    void *user_data;

    /**
     * @brief The function called when the remote host issues a reset.
     *
     * @param user_data The arbitrary user data.
     * @param event The signaled event.
     */
    void (*event_fn)(void *user_data, enum embc_dl_event_e event);

    /**
     * @brief The function called upon message receipt.
     *
     * @param user_data The arbitrary user data.
     * @param port_id The port id for this port.
     * @param seq The frame reassembly information.
     * @param port_data The arbitrary 16-bit port data.  Each port is
     *      free to assign meaning to this value.
     * @param msg The buffer containing the message.
     *      This buffer is only valid for the duration of the callback.
     * @param msg_size The size of msg_buffer in bytes.
     */
    void (*recv_fn)(void *user_data,
                    uint8_t port_id,
                    enum embc_transport_seq_e seq,
                    uint16_t port_data,
                    uint8_t *msg, uint32_t msg_size);
};


/**
 * @brief The low-level abstract driver implementation.
 */
struct embc_transport_ll_s {
    /// The arbitrary user data.
    void *user_data;

    int32_t (*send)(struct embc_dl_s * self, uint32_t metadata,
                    uint8_t const *msg, uint32_t msg_size);
};

struct embc_transport_s {
    struct embc_transport_ll_s ll;
    struct embc_transport_api_s ports[EMBC_TRANSPORT_PORT_MAX];
};

/**
 * @brief The function called by the lower level when the remote host issues a reset.
 *
 * @param user_data The embc_transport_s instance.
 * @param event The signaled event.
 */
void embc_transport_ll_on_event(void *user_data, enum embc_dl_event_e event);

/**
 * @brief The function called upon message receipt by the lower level.
 *
 * @param user_data The embc_transport_s instance.
 * @param metadata The arbitrary 24-bit metadata associated with the message.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 */
void embc_transport_ll_on_recv(void *user_data,
                               uint8_t *msg, uint32_t msg_size);

/**
 * @brief Initialize the instance.
 *
 * @param self The already allocated instance to initialize.
 * @return 0 or error code.
 */
int32_t embc_transport_init(struct embc_transport_s * self);

/**
 * @brief Send a message.
 *
 * @param self The instance.
 * @param port_id The port id for this port.
 * @param port_data The arbitrary 16-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The msg_buffer containing the message.  The data link layer
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 *
 * The port send_done_cbk callback will be called when the send completes.
 */
int32_t embc_transport_send(struct embc_transport_s * self,
                            uint8_t port_id,
                            uint16_t port_data,
                            uint8_t const *msg, uint32_t msg_size);


#if 0
/**
 * @brief The commands defined for port 0.
 */
enum embc_framer_port0_cmd_e {
    EMBC_TRANSPORT_PORT0_IGNORE = 0x00,
    EMBC_TRANSPORT_PORT0_CONNECT = 0x01,
    EMBC_TRANSPORT_PORT0_CONNECT_ACK = 0x81,
    EMBC_TRANSPORT_PORT0_INFO_REQ = 0x02,
    EMBC_TRANSPORT_PORT0_INFO_RSP = 0x82,
    EMBC_TRANSPORT_PORT0_PING_REQ = 0x03,
    EMBC_TRANSPORT_PORT0_PING_RSP = 0x83,
    EMBC_TRANSPORT_PORT0_STATUS_REQ = 0x04,
    EMBC_TRANSPORT_PORT0_STATUS = 0x84,
    EMBC_TRANSPORT_PORT0_LOOPBACK_REQ = 0x05,
    EMBC_TRANSPORT_PORT0_LOOPBACK_RSP = 0x85,
};
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_TRANSPORT_H__ */
