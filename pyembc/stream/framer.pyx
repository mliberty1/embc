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


from .c_framer cimport *
from ..c_crc cimport *
import numpy as np
cimport numpy as np
include "../module.pxi"
import logging


log = logging.getLogger(__name__)


cdef class _Port:
    cdef object send_done_cbk
    cdef object recv_cbk

    def __init__(self):
        self.send_done_cbk = None
        self.recv_cbk = None

    def update(self, send_done_cbk, recv_cbk):
        self.send_done_cbk = send_done_cbk
        self.recv_cbk = recv_cbk

    @staticmethod
    cdef void on_send_done_cbk(void *user_data, uint8_t port_id, uint8_t message_id):
        cdef _Port self = <object> user_data
        if callable(self.send_done_cbk):
            self.send_done_cbk(port_id, message_id)
        else:
            log.warning('send_done_cbk not callable: %s', self.send_done_cbk)

    @staticmethod
    cdef void on_recv_cbk(void *user_data,
                     uint8_t port_id, uint8_t message_id,
                     uint8_t *msg_buffer, uint32_t msg_size):
        cdef _Port self = <object> user_data
        cdef np.uint8_t [::1] msg_c
        msg = np.zeros(msg_size, dtype=np.uint8)
        msg_c = msg
        for i in range(msg_size):
            msg_c[i] = msg_buffer[i]
        if callable(self.recv_cbk):
            self.recv_cbk(port_id, message_id, msg)


cdef class Framer:
    cdef embc_framer_s * _framer
    cdef object _event_cbk
    cdef object _ports
    cdef object _hal
    cdef object _ll
    cdef object _send_buffers
    cdef embc_framer_ul_s _ul

    def __init__(self, event_cbk, hal, ll_instance):
        """Initialize the Framer instance

        :param event_cbk: The fn(event_id) called for each new event.
        :param hal: TBD
        :param ll_instance: The lower-level communication instance which
            has the following methods:
            - open(recv_cbk, send_done_cbk) where recv_cbk(buffer) and send_done(buffer).
            - close()
            - send(buffer)
        """
        cdef embc_framer_config_s c_config
        cdef embc_framer_hal_s c_hal
        cdef embc_framer_ll_s c_ll

        self._hal = hal
        self._ll = ll_instance
        self._send_buffers = []

        self._event_cbk = event_cbk
        c_config.ports = 32
        c_config.send_frames = 32
        c_config.event_user_data = <void *> self
        c_config.event_cbk = Framer._c_event_cbk
        self._ports: List[_Port] = []
        for p in range(c_config.ports):
            self._ports.append(_Port())

        c_ll.ll_user_data = <void *> self
        c_ll.open = Framer._c_ll_open
        c_ll.close = Framer._c_ll_close
        c_ll.send = Framer._c_ll_send

        self._framer = embc_framer_initialize(&c_config, &c_hal, &c_ll)
        if not self._framer:
            raise ValueError('could not initialize framer')

    def __dealloc__(self):
        embc_framer_finalize(self._framer)

    @staticmethod
    cdef void _c_event_cbk(void * user_data, embc_framer_s * instance, embc_framer_event_s event):
        cdef Framer self = <object> user_data
        if callable(self._event_cbk):
            self._event_cbk(<int> event)

    def ul_recv(self, buffer):
        cdef np.uint8_t [::1] buffer_c
        buffer = np.ascontiguousarray(buffer, dtype=np.uint8)
        buffer_c = buffer
        if log.getEffectiveLevel() >= logging.INFO:
            log.info('received %s', ','.join(['0x%02x' % x for x in buffer]))
        self._ul.recv(self._ul.ul_user_data, &buffer_c[0], len(buffer))

    def ul_send_done(self, buffer):
        cdef intptr_t b_addr
        cdef uint8_t * b
        cdef uint32_t b_sz
        b_addr, b_sz = self._send_buffers.pop(0)
        b = <uint8_t *> b_addr;
        if b_sz != len(buffer):
            raise ValueError('send buffer size mismatch')
        self._ul.send_done(self._ul.ul_user_data, b, b_sz)

    @staticmethod
    cdef int32_t _c_ll_open(void * ll_user_data, embc_framer_ul_s * ul_instance):
        cdef Framer self = <object> ll_user_data
        self._ul = ul_instance[0]
        self._ll.open(self.ul_recv, self.ul_send_done)

    @staticmethod
    cdef int32_t _c_ll_close(void * ll_user_data):
        cdef Framer self = <object> ll_user_data
        self._ll.close()
        # self._ul = None

    @staticmethod
    cdef void _c_ll_send(void * ll_user_data, uint8_t * buffer, uint32_t buffer_size):
        cdef Framer self = <object> ll_user_data
        b = np.zeros(buffer_size, dtype=np.uint8)
        for i in range(buffer_size):
            b[i] = buffer[i]
        self._send_buffers.append([<intptr_t> buffer, buffer_size])
        self._ll.send(b)

    def port_register(self, port_id, send_done_cbk, recv_cbk):
        cdef embc_framer_port_s port_instance
        port: Port_ = self._ports[port_id]
        port.update(send_done_cbk, recv_cbk)
        port_instance.user_data = <void *> port
        port_instance.send_done_cbk = _Port.on_send_done_cbk
        port_instance.recv_cbk = _Port.on_recv_cbk
        embc_framer_port_register(self._framer, port_id, &port_instance)

    def send(self, priority, port_id, message_id, buffer):
        cdef const np.uint8_t [::1] buffer_c
        buffer = np.ascontiguousarray(buffer, dtype=np.uint8)
        buffer_c = buffer
        rv = embc_framer_send(self._framer, priority, port_id, message_id, &buffer_c[0], len(buffer))
        if rv:
            raise RuntimeError(f'Framer.send returned {rv}')

    def status(self):
        cdef embc_framer_status_s status
        cdef int32_t rv
        rv = embc_framer_status_get(self._framer, &status)
        if (rv):
            raise RuntimeError(f'Framer.status returned {rv}')
        return {
            'rx_total_bytes': status.rx.total_bytes,
            'rx_invalid_bytes': status.rx.invalid_bytes,
            'rx_data_frames': status.rx.data_frames,
            'rx_crc_errors': status.rx.crc_errors,
            'rx_frame_id_errors': status.rx.frame_id_errors,
            'rx_frames_missing': status.rx.frames_missing,
            'rx_resync': status.rx.resync,
            'rx_frame_too_big': status.rx.frame_too_big,

            'tx_bytes': status.tx.bytes,
            'tx_data_frames': status.tx.data_frames,

            'send_buffers_free': status.send_buffers_free,
        }


def construct_ack(frame_id):
    cdef np.uint8_t [::1] b_c
    b = np.zeros(4, dtype=np.uint8)
    b_c = b
    b[0] = 0x55
    b[1] = 0x98 | ((frame_id >> 8) & 0x07)
    b[2] = frame_id & 0xff
    b[3] = <uint8_t> (embc_crc32(0, &b_c[1], 2) & 0xff)
    return b
