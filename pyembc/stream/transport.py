# Copyright 2021 Jetperch LLC
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

from .data_link import PORTS_MAX, PAYLOAD_MAX, Event
import json
import logging
import numpy as np
import struct


log = logging.getLogger(__name__)


class PayloadType:
    NULL = 0
    U32 = 1
    STR = 4
    JSON = 5
    BIN = 6


RETAIN = (1 << 4)


def _to_null(x):
    return None


def _to_str(x):
    if len(x) <= 1:
        return ''
    try:
        x = x[:-1]
        if isinstance(x, np.ndarray):
            x = x.tobytes()
        return x.decode('utf-8')
    except Exception:
        log.warning('invalud string: %b', x)
        return ''


def _to_json(x):
    if len(x) <= 1:
        return None
    x = _to_str(x)
    try:
        return json.loads(x)
    except Exception:
        log.warning('invalid json: %s', x)
        return None


def _to_bin(x):
    return x


def _to_u32(x):
    if len(x) != 4:
        raise ValueError('invalid length')
    return struct.unpack('<I', x)[0]


_PAYLOAD_TYPE_FN = {
    PayloadType.NULL: _to_null,
    PayloadType.U32: _to_u32,
    PayloadType.STR: _to_str,
    PayloadType.JSON: _to_json,
    PayloadType.BIN: _to_bin,
}


def payload_encode(x):
    if x is None:
        return PayloadType.NULL, np.array([0], dtype=np.uint8)
    elif isinstance(x, int):
        if 0 <= x < (1 << 32):
            return PayloadType.U32, np.frombuffer(struct.pack('<I', x), dtype=np.uint8)
    elif isinstance(x, str):
        return PayloadType.STR, np.frombuffer(x.encode('utf-8') + b'\x00', dtype=np.uint8)
    elif isinstance(x, bytes):
        return PayloadType.BIN, np.frombuffer(x, dtype=np.uint8)
    elif isinstance(x, np.ndarray):
        return PayloadType.BIN, np.frombuffer(x, dtype=np.uint8)
    else:
        return PayloadType.JSON, np.frombuffer(json.dumps(x).encode('utf-8') + b'\x00', dtype=np.uint8)
    raise ValueError('Unsupported payload')


def payload_decode(dtype, x):
    payload_fn = _PAYLOAD_TYPE_FN[dtype]
    return payload_fn(x)


class _Port:

    def __init__(self, port=None):
        self.port = port
        self._msg = []

    def on_event(self, event):
        if self.port is not None and callable(self.port.on_event):
            self.port.on_event(event)

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
            if self.port is not None and callable(self.port.on_recv):
                self.port.on_recv(port_data, msg)


class Transport:

    def __init__(self, send_fn=None):
        self.send_fn = send_fn
        self._last_tx_event = Event.TX_DISCONNECTED
        self._ports = [_Port() for idx in range(PORTS_MAX + 1)]

    def on_event(self, event):
        if event == Event.TX_CONNECTED or event == Event.TX_DISCONNECTED:
            self._last_tx_event = event
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

    def register_port(self, port_id, port):
        """Register port handlers.

        :param port_id: The port identifier.
        :param port: The object that implements the :class:`PortApi`.
        """
        port_id = int(port_id)
        if not 0 <= port_id <= PORTS_MAX:
            raise ValueError('invalid port: %d', port_id)
        self._ports[port_id].port = port
        self._ports[port_id].on_event(self._last_tx_event)
