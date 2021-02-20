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


from .c_comm cimport *
import numpy as np
cimport numpy as np
include "../module.pxi"
import json
import logging


# From embc/time.h
EMBC_TIME_Q = 30
EMBC_TIME_SECOND = (1 << EMBC_TIME_Q)
EMBC_TIME_MILLISECOND = ((EMBC_TIME_SECOND + 500) // 1000)

log = logging.getLogger(__name__)
TX_TIMEOUT_DEFAULT = 16 * EMBC_TIME_MILLISECOND


cdef _value_pack(embc_pubsub_value_s * value, data, retain=None):
    s = None
    if data is None:
        value[0].type = EMBC_PUBSUB_DTYPE_NULL
        value[0].size = 0
    elif isinstance(data, int):
        value[0].type = EMBC_PUBSUB_DTYPE_U32
        value[0].value.u32 = data
        value[0].size = 4
    elif isinstance(data, str):
        s = data.encode('utf-8')
        value[0].type = EMBC_PUBSUB_DTYPE_STR
        value[0].value.str = s
        value[0].size = len(s) + 1
    elif isinstance(data, bytes):
        value[0].type = EMBC_PUBSUB_DTYPE_BIN
        value[0].value.bin = data
        value[0].size = len(s)
    else:
        s = json.dumps(data).encode('utf-8')
        value[0].type = EMBC_PUBSUB_DTYPE_JSON
        value[0].value.str = s
        value[0].size = len(s) + 1
    if bool(retain):
        value[0].type |= EMBC_PUBSUB_DFLAG_RETAIN
    return s  # so that caller can keep valid until used.


cdef _value_unpack(embc_pubsub_value_s * value):
    retain = (0 != (value[0].type & EMBC_PUBSUB_DFLAG_RETAIN))
    dtype = value[0].type & 0x0f
    if dtype == EMBC_PUBSUB_DTYPE_NULL:
        v = None
    elif dtype == EMBC_PUBSUB_DTYPE_U32:
        v = value[0].value.u32
    elif dtype == EMBC_PUBSUB_DTYPE_STR:
        if value[0].size <= 1:
            v = None
        else:
            v = value[0].value.str[:(value[0].size - 1)].decode('utf-8')
    elif dtype == EMBC_PUBSUB_DTYPE_BIN:
        v = value[0].value.bin[value[0].size]
    elif dtype == EMBC_PUBSUB_DTYPE_JSON:
        if value[0].size <= 1:
            v = None
        else:
            s = value[0].value.str[:(value[0].size - 1)].decode('utf-8')
            v = json.loads(s)
    else:
        raise RuntimeError(f'Unsupported value type: {dtype}')
    return v, retain


cdef _dl_status_decode(embc_dl_status_s * status):
    return {
        'version': status[0].version,
        'rx': {
            'msg_bytes': status[0].rx.msg_bytes,
            'data_frames': status[0].rx.data_frames,
        },
        'rx_framer': {
            'total_bytes': status[0].rx_framer.total_bytes,
            'ignored_bytes': status[0].rx_framer.ignored_bytes,
            'resync': status[0].rx_framer.resync,
        },
        'tx': {
            'bytes': status[0].tx.bytes,
            'msg_bytes': status[0].tx.msg_bytes,
            'data_frames': status[0].tx.data_frames,
            'retransmissions': status[0].tx.retransmissions,
        },
    }


cdef class Comm:
    """A Communication Device using the EMBC stack.

    :param device: The device string.
    :param topic_prefix: The prefix to prepend/remove from the main
        pubsub tree.
    :param subscriber: The subscriber callback(topic, value, retain, src_cbk)
        for topic updates.
    :param baudrate: The baud rate for COM / UART ports.
    """

    cdef embc_comm_s * _comm
    cdef object topic_prefix
    cdef object _subscriber

    def __init__(self, device, topic_prefix, subscriber,
            baudrate=None,
            tx_link_size=None,
            tx_window_size=None,
            tx_buffer_size=None,
            rx_window_size=None,
            tx_timeout=None):

        cdef embc_dl_config_s config
        log.info('Comm.__init__ start')
        self.topic_prefix = topic_prefix
        self._subscriber = subscriber

        baudrate = 3000000 if baudrate is None else int(baudrate)
        config.tx_link_size = 8 if tx_link_size is None else int(tx_link_size)
        config.tx_window_size = 8 if tx_window_size is None else int(tx_window_size)
        config.tx_buffer_size = (1 << 12) if tx_buffer_size is None else int(tx_buffer_size)
        config.rx_window_size = 64 if rx_window_size is None else int(rx_window_size)
        config.tx_timeout = TX_TIMEOUT_DEFAULT if tx_timeout is None else int(tx_timeout)
        config.tx_link_size = 64 if tx_link_size is None else int(tx_link_size)
        device_str = device.encode('utf-8')
        log.info('comm_initialize(%s, %s)', device_str, baudrate)
        self._comm = embc_comm_initialize(&config, device_str, baudrate, Comm._subscriber_cbk, <void *> self, '')
        if not self._comm:
            raise RuntimeError('Could not allocate instance')

    @staticmethod
    cdef uint8_t _subscriber_cbk(void * user_data, const char * topic, const embc_pubsub_value_s * value) with gil:
        cdef Comm self = <object> user_data
        v, retain = _value_unpack(value)
        topic_str = topic.decode('utf-8')
        if self.topic_prefix is not None:
            topic_str = self.topic_prefix + topic_str
        try:
            self._subscriber(topic_str, v, retain, self.publish)
        except Exception:
            log.exception(f'_subscriber_cbk({topic})')

    def close(self):
        embc_comm_finalize(self._comm)

    def publish(self, topic: str, value, retain=None, src_cbk=None):
        # ignore src_cbk since already implemented in comm
        cdef int32_t rc
        cdef embc_pubsub_value_s v
        if self.topic_prefix is not None:
            if not topic.startswith(self.topic_prefix):
                return
            topic = topic[len(self.topic_prefix):]
        s = _value_pack(&v, value, retain)
        rc = embc_comm_publish(self._comm, topic, &v)
        if rc:
            raise RuntimeError(f'publish({topic}) failed with {rc}')

    def query(self, topic):
        cdef int32_t rc
        cdef embc_pubsub_value_s v
        rc = embc_comm_query(self._comm, topic, &v)
        if rc:
            raise RuntimeError(f'query({topic}) failed with {rc}')
        return _value_unpack(&v)

    def status(self):
        cdef embc_dl_status_s status
        embc_comm_status_get(self._comm, &status)
        return _dl_status_decode(&status)
