# Copyright 2020 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from libc.stdint cimport int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t


cdef extern from "embc/stream/framer.h":
    struct embc_framer_status_s:
        uint64_t total_bytes
        uint64_t ignored_bytes
        uint64_t resync

cdef extern from "embc/stream/pubsub.h":
    enum embc_pubsub_dtype_e:
        EMBC_PUBSUB_DTYPE_NULL = 0
        EMBC_PUBSUB_DTYPE_U32 = 1
        EMBC_PUBSUB_DTYPE_STR = 4
        EMBC_PUBSUB_DTYPE_JSON = 5
        EMBC_PUBSUB_DTYPE_BIN = 6

    enum embc_pubsub_dflag_e:
        EMBC_PUBSUB_DFLAG_NONE = 0
        EMBC_PUBSUB_DFLAG_RETAIN = (1 << 4)
        EMBC_PUBSUB_DFLAG_CONST = (1 << 5)

    union embc_pubsub_value_inner_u:
        const char * str
        const uint8_t * bin
        uint32_t u32

    struct embc_pubsub_value_s:
        uint8_t type
        embc_pubsub_value_inner_u value
        uint32_t size

    ctypedef uint8_t (*embc_pubsub_subscribe_fn)(void * user_data,
            const char * topic, const embc_pubsub_value_s * value)


cdef extern from "embc/stream/data_link.h":

    struct embc_dl_config_s:
        uint32_t tx_link_size
        uint32_t tx_window_size
        uint32_t tx_buffer_size
        uint32_t rx_window_size
        uint32_t tx_timeout

    struct embc_dl_tx_status_s:
        uint64_t bytes
        uint64_t msg_bytes
        uint64_t data_frames
        uint64_t retransmissions

    struct embc_dl_rx_status_s:
        uint64_t msg_bytes
        uint64_t data_frames

    struct embc_dl_status_s:
        uint32_t version
        uint32_t reserved
        embc_dl_rx_status_s rx
        embc_framer_status_s rx_framer
        embc_dl_tx_status_s tx

    enum embc_dl_event_e:
        EMBC_DL_EV_UNKNOWN
        EMBC_DL_EV_RX_RESET_REQUEST
        EMBC_DL_EV_TX_DISCONNECTED
        EMBC_DL_EV_INTERNAL_ERROR

    struct embc_dl_api_s:
        void *user_data
        void (*event_fn)(void *user_data, embc_dl_event_e event) nogil
        void (*recv_fn)(void *user_data, uint32_t metadata,
                        uint8_t *msg, uint32_t msg_size) nogil


cdef extern from "embc/host/comm.h":
    struct embc_comm_s
    embc_comm_s * embc_comm_initialize(const embc_dl_config_s * config,
                                       const char * device,
                                       uint32_t baudrate,
                                       embc_pubsub_subscribe_fn cbk_fn,
                                       void * cbk_user_data)
    void embc_comm_finalize(embc_comm_s * self)
    int32_t embc_comm_publish(embc_comm_s * self,
                              const char * topic, const embc_pubsub_value_s * value)
    int32_t embc_comm_query(embc_comm_s * self, const char * topic, embc_pubsub_value_s * value)
    int32_t embc_comm_status_get(embc_comm_s * self, embc_dl_status_s * status)
