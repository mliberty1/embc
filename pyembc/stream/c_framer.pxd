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

    struct embc_framer_s

    struct embc_framer_rx_status_s:
        uint64_t total_bytes
        uint64_t invalid_bytes
        uint64_t data_frames
        uint64_t crc_errors
        uint64_t frame_id_errors
        uint64_t frames_missing
        uint64_t resync
        uint64_t frame_too_big

    struct embc_framer_tx_status_s:
        uint64_t bytes
        uint64_t data_frames

    struct embc_framer_status_s:
        uint32_t version
        uint32_t reserved
        embc_framer_rx_status_s rx
        embc_framer_tx_status_s tx
        uint64_t send_buffers_free

    enum embc_framer_sequence_e:
        EMBC_FRAMER_SEQUENCE_MIDDLE = 0x0
        EMBC_FRAMER_SEQUENCE_END = 0x1
        EMBC_FRAMER_SEQUENCE_START = 0x2
        EMBC_FRAMER_SEQUENCE_SINGLE = 0x3

    enum embc_framer_event_s:
        EMBC_FRAMER_EV_UNDEFINED = 0
        EMBC_FRAMER_EV_CONNECT = 1

    ctypedef void (*embc_framer_event_cbk)(void * user_data, embc_framer_s * instance, embc_framer_event_s event)

    struct embc_framer_config_s:
        uint32_t ports
        uint32_t send_frames
        embc_framer_event_cbk event_cbk
        void * event_user_data

    struct embc_framer_port_s:
        void * user_data
        void (*send_done_cbk)(void *user_data,
                         uint8_t port_id, uint8_t message_id)
        void (*recv_cbk)(void *user_data,
                         uint8_t port_id, uint8_t message_id,
                         const uint8_t *msg_buffer, uint32_t msg_size)

    int32_t embc_framer_port_register(embc_framer_s * self,
                                      uint8_t port_id,
                                      embc_framer_port_s * port_instance);
    int32_t embc_framer_send(embc_framer_s * self,
                             uint8_t priority, uint8_t port_id, uint8_t message_id,
                             uint8_t *msg_buffer, uint32_t msg_size)

    struct embc_framer_ul_s:
        void *ul_user_data
        void (*recv)(void *ul_user_data, const uint8_t * buffer, uint32_t buffer_size)
        void (*send_done)(void *ul_user_data, uint8_t * buffer, uint32_t buffer_size)

    struct embc_framer_ll_s:
        void * ll_user_data
        int32_t (*open)(void * ll_user_data, embc_framer_ul_s * ul_instance)
        int32_t (*close)(void * ll_user_data)
        void (*send)(void * ll_user_data, uint8_t * buffer, uint32_t buffer_size)
        # note: recv from the lower-level driver using uart_ul_s.recv.

    struct embc_framer_hal_s:
        void * hal
        int64_t (*time_get)(void * hal)
        int32_t (*timer_set_fn)(void * hal, int64_t timeout,
                                void (*cbk_fn)(void *, uint32_t), void * cbk_user_data,
                                uint32_t * timer_id)
        int32_t (*timer_cancel_fn)(void * hal, uint32_t timer_id)

    embc_framer_s * embc_framer_initialize(
            const embc_framer_config_s * config,
            embc_framer_hal_s * hal,
            embc_framer_ll_s * ll_instance)
    int32_t embc_framer_finalize(embc_framer_s * self)
    int32_t embc_framer_status_get(
            embc_framer_s * self,
            embc_framer_status_s * status);