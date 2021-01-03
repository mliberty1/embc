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

cdef extern from "embc/stream/data_link.h":

    struct embc_dl_config_s:
        uint32_t tx_link_size
        uint32_t tx_window_size
        uint32_t tx_buffer_size
        uint32_t rx_window_size
        uint32_t tx_timeout_ms

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
        uint64_t send_buffers_free

    enum embc_dl_event_e:
        EMBC_DL_EV_UNKNOWN
        EMBC_DL_EV_REMOTE_RESET
        EMBC_DL_EV_REMOTE_UNRESPONSIVE
        EMBC_DL_EV_INTERNAL_ERROR

    struct embc_dl_api_s:
        void *user_data;
        void (*event_fn)(void *user_data, embc_dl_event_e event);
        void (*recv_fn)(void *user_data, uint32_t metadata,
                        uint8_t *msg, uint32_t msg_size);


cdef extern from "embc/host/uart_data_link.h":
    struct embc_udl_s
    embc_udl_s * embc_udl_initialize(const embc_dl_config_s * config,
                                     const char * uart_device,
                                     uint32_t baudrate)
    ctypedef void (*embc_udl_process_fn)(void * user_data)
    int32_t embc_udl_start(embc_udl_s * self,
                           const embc_dl_api_s * ul,
                           embc_udl_process_fn process_fn,
                           void * process_user_data);

    int32_t embc_udl_send(embc_udl_s * self, uint32_t metadata,
                         const uint8_t *msg, uint32_t msg_size);
    void embc_udl_reset(embc_udl_s * self);

    int32_t embc_udl_finalize(embc_udl_s * self);

    int32_t embc_udl_status_get(
            embc_udl_s * self,
            embc_dl_status_s * status);

    uint32_t embc_udl_send_available(embc_udl_s * self);
