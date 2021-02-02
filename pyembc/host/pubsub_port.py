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

import numpy as np
import struct
import logging


log = logging.getLogger(__name__)


class PayloadType:
    NULL = 0
    CSTR = 1
    U32 = 2


def _to_null(x):
    return None


def _to_cstr(x):
    return x[:-1].tobytes.decode('utf-8')


def _to_u32(x):
    if len(x) != 4:
        raise ValueError('invalid length')
    return struct.unpack('<I', x)[0]


_PAYLOAD_TYPE_FN = {
    PayloadType.NULL: _to_null,
    PayloadType.CSTR: _to_cstr,
    PayloadType.U32: _to_u32,
}


def _payload_encode(x):
    if x is None:
        return PayloadType.NULL, b''
    elif isinstance(x, str):
        return PayloadType.CSTR, x.encode('utf-8') + b'\x00'
    elif isinstance(x, int):
        if 0 <= x < (1 << 32):
            return PayloadType.U32, struct.pack('<I', x)
    raise ValueError('Unsupported payload')


class PubSubPort:

    def __init__(self, send, listener):
        """
        Implement the port compatible with the transport layer.

        :param send: The callable(port_data, msg) function to send
            data to the lower transport layer.
        :param listener: The callable(status, topic, value, port_data)
            to provide data to the higher layer.

        Provide :meth:`on_event` and :meth:`on_recv` to the
        Transport.register_port().
        """
        self._send = send
        self.listener = listener  # callable(status, topic, value, port_data)

    def on_event(self, event):
        pass

    def send(self, topic, value):
        topic_bytes = topic.encode('utf-8') + b'\x00'
        topic_len = len(topic_bytes)
        if topic_len > 32:
            raise ValueError(f'topic too long: {topic_len}')
        payload_type, payload = _payload_encode(value)
        sz = 1 + 1 + topic_len + 1 + len(payload)
        if sz > 256:
            raise ValueError(f'message too long: {sz}')
        port_data = (payload_type & 0x0f) << 8
        msg = np.empty(sz, dtype=np.uint8)
        msg[0] = (topic_len - 1)
        msg[1:topic_len + 1] = memoryview(topic_bytes)
        msg[1 + topic_len] = len(payload)
        if len(payload):
            msg[2 + topic_len:] = memoryview(payload)
        self._send(port_data, msg)

    def on_recv(self, port_data, msg):
        topic_len = (msg[0] & 0x1f) + 1
        payload_type = (port_data >> 8) & 0x0f
        topic = msg[1:topic_len].tobytes().decode('utf-8')
        payload_len = msg[1 + topic_len]
        payload = msg[2 + topic_len:]
        if len(payload) != payload_len:
            log.warning('Invalid payload')
            return
        payload_fn = _PAYLOAD_TYPE_FN[payload_type]
        x = payload_fn(payload)
        log.info("recv(topic='%s', value=%s, port_data=%s)", topic, x, port_data)
        if callable(self.listener):
            self.listener(topic, x)
