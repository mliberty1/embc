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


from .c_uart_data_link cimport *
import numpy as np
cimport numpy as np
include "../module.pxi"
import logging


log = logging.getLogger(__name__)


cdef void _process_cbk(void * user_data) nogil:
    return  # do nothing


cdef void _recv_fn_inner(void *user_data, uint32_t metadata, uint8_t *msg, uint32_t msg_size) with gil:
    cdef uint32_t i
    cdef UartDataLink self = <object> user_data
    b = np.zeros(msg_size, dtype=np.uint8)
    for i in range(msg_size):
        b[i] = msg[i]
    self._on_recv(metadata, b)


cdef void _recv_fn(void *user_data, uint32_t metadata, uint8_t *msg, uint32_t msg_size) nogil:
    _recv_fn_inner(user_data, metadata, msg, msg_size)


cdef void _event_cbk_inner(void * user_data, embc_dl_event_e event) with gil:
    cdef UartDataLink self = <object> user_data
    self._on_event(<int> event)


cdef void _event_cbk(void * user_data, embc_dl_event_e event) nogil:
    _event_cbk_inner(user_data, event)


cdef class UartDataLink:
    cdef embc_udl_s * _udl
    cdef object _recv_cbk
    cdef object _event_cbk

    def __init__(self,
            uart_device,
            recv_callback,
            event_callback,
            baudrate=None,
            tx_link_size=None,
            tx_window_size=None,
            tx_buffer_size=None,
            rx_window_size=None,
            tx_timeout_ms=None):

        cdef embc_dl_api_s api
        cdef embc_dl_config_s config

        self._recv_cbk = recv_callback
        self._event_cbk = event_callback
        baudrate = 3000000 if baudrate is None else int(baudrate)
        config.tx_link_size = 8 if tx_link_size is None else int(tx_link_size)
        config.tx_window_size = 8 if tx_window_size is None else int(tx_window_size)
        config.tx_buffer_size = 1 << 12 if tx_buffer_size is None else int(tx_buffer_size)
        config.tx_buffer_size = 64 if rx_window_size is None else int(rx_window_size)
        config.tx_timeout_ms = 15 if tx_timeout_ms is None else int(tx_timeout_ms)
        config.tx_link_size = 64 if tx_link_size is None else int(tx_link_size)
        self._udl = embc_udl_initialize(&config, uart_device.encode('utf-8'), baudrate)
        if not self._udl:
            raise RuntimeError('Could not allocate instance')

        api.user_data = <void *> self
        api.event_fn = _event_cbk
        api.recv_fn = _recv_fn
        if (0 != embc_udl_start(self._udl, &api, _process_cbk, <void *> self)):
            raise RuntimeError('Could not start uart data link')

    cdef void _on_event(self, event):
        if callable(self._event_cbk):
            self._event_cbk(event)

    cdef void _on_recv(self, metadata, msg):
        if callable(self._recv_cbk):
            self._recv_cbk(metadata, msg)

    def send(self, metadata, msg):
        cdef np.uint8_t [::1] msg_c
        msg = np.ascontiguousarray(msg, dtype=np.uint8)
        msg_c = msg
        return embc_udl_send(self._udl, int(metadata), &msg_c[0], len(msg))

    def reset(self):
        embc_udl_reset(self._udl)

    def close(self):
        embc_udl_finalize(self._udl)

    def status(self):
        cdef embc_dl_status_s status
        embc_udl_status_get(self._udl, &status)
        return {
            'version': status.version,
            'rx': {
                'msg_bytes': status.rx.msg_bytes,
                'data_frames': status.rx.data_frames,
            },
            'rx_framer': {
                'total_bytes': status.rx_framer.total_bytes,
                'ignored_bytes': status.rx_framer.ignored_bytes,
                'resync': status.rx_framer.resync,
            },
            'tx': {
                'bytes': status.tx.bytes,
                'msg_bytes': status.tx.msg_bytes,
                'data_frames': status.tx.data_frames,
                'retransmissions': status.tx.retransmissions,
            },
            'send_buffers_free': status.send_buffers_free,
        }
