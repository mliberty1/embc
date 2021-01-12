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

import logging
import numpy as np


log = logging.getLogger(__name__)
PORTS_MAX = 0x1f
PAYLOAD_MAX = 256


class _Port:

    def __init__(self):
        self._on_event = None
        self._on_recv = None
        self._msg = []

    def on_event(self, event):
        if callable(self._on_event):
            self._on_event(event)

    def on_recv(self, metadata, msg):
        seq = (metadata >> 6) & 0x03
        port_data = (metadata >> 8) & 0xffff
        if 0 != (seq & 2) and len(self._msg):
            log.warning('seq error: msg not empty but start')
            self._msg = []
        if 0 == (seq & 2) and not len(self._msg):
            log.warning('seq error: msg empty but continue')
        self._msg.append(msg)
        if 0 != (seq & 1):
            msg = np.concatenate(self._msg)
            self._msg.clear()
            if callable(self._on_recv):
                self._on_recv(port_data, msg)

    def register(self, on_event, on_recv):
        self._on_event = on_event
        self._on_recv = on_recv


class Transport:

    def __init__(self, send_fn=None):
        self.send_fn = send_fn
        self._ports = [_Port() for idx in range(PORTS_MAX + 1)]

    def on_event(self, event):
        for port in self._ports:
            port.on_event(event)

    def on_recv(self, metadata, msg):
        port_id = metadata & PORTS_MAX
        self._ports[port_id].on_recv(metadata, msg)

    def send(self, port_id, port_data, msg):
        port_id = int(port_id)
        if not 0 <= port_id <= PORTS_MAX:
            raise ValueError('invalid port: %d', port_id)
        port_data = int(port_data) & 0xffff
        if not callable(self.send_fn):
            log.warning('send but no handler')
            return
        seq = 2
        while len(msg):
            if len(msg) > PAYLOAD_MAX:
                payload, msg = msg[:PAYLOAD_MAX], msg[PAYLOAD_MAX:]
            else:
                seq |= 1
                payload = msg
                msg = []
            metadata = (port_data << 8) | port_id | (seq << 6)
            self.send_fn(metadata, payload)
            seq = 0

    def register_port(self, port_id, on_event, on_recv):
        """Register port handlers.

        :param port_id: The port identifier.
        :param on_event: The function(event) to call on new events.
        :param on_recv: The function(port_data, msg) to call on messages.
        """
        port_id = int(port_id)
        if not 0 <= port_id <= PORTS_MAX:
            raise ValueError('invalid port: %d', port_id)
        self._ports[port_id].register(on_event, on_recv)

