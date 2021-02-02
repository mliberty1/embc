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
import logging
from .transport import RETAIN, payload_encode, payload_decode


log = logging.getLogger(__name__)


class PubSubPort:

    def __init__(self, pubsub, transport, port_id):
        """
        Implement the port compatible with the transport layer.

        :param pubsub: The :class:`PubSub` instance.
        :param transport: The :class:`Transport` instance.
        :param port_id: The port_id.
        """
        self._pubsub = pubsub
        self._transport = transport
        self._port_id = port_id
        transport.register_port(port_id, self)
        pubsub.subscribe('', self.send, skip_retained=True)

    def on_event(self, event):
        pass

    def send(self, topic, value):
        if topic.startswith('h/'):
            return
        topic_bytes = topic.encode('utf-8') + b'\x00'
        topic_len = len(topic_bytes)
        if topic_len > 32:
            raise ValueError(f'topic too long: {topic_len}')
        payload_type, payload = payload_encode(value)
        sz = 1 + topic_len + 1 + len(payload)
        if sz > 256:
            raise ValueError(f'message too long: {sz}')
        port_data = (payload_type & 0x0f) << 8
        msg = np.empty(sz, dtype=np.uint8)
        msg[0] = (topic_len - 1)
        msg[1:topic_len + 1] = memoryview(topic_bytes)
        msg[1 + topic_len] = len(payload)
        if len(payload):
            msg[2 + topic_len:] = memoryview(payload)
        self._transport.send(self._port_id, port_data, msg)

    def on_recv(self, port_data, msg):
        topic_len = (msg[0] & 0x1f) + 1
        dtype = (port_data >> 8) & 0x0f
        retain = ((port_data >> 8) & RETAIN) != 0
        topic = msg[1:topic_len].tobytes().decode('utf-8')
        payload_len = msg[1 + topic_len]
        payload = msg[2 + topic_len:]
        if len(payload) != payload_len:
            log.warning('Invalid payload')
            return
        x = payload_decode(dtype, payload)
        log.info("recv(topic='%s', port_data=0x%04x, value=%s)", topic, port_data, x)
        self._pubsub.publish(topic, x, retain=retain, src_cbk=self.send)
