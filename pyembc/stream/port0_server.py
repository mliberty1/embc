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

from .data_link import PORTS_COUNT, Event
from .transport import PayloadType, payload_decode
from .ports import PORTS
from pyembc.time import now
import logging
import numpy as np
import struct
import weakref

log = logging.getLogger(__name__)


def _pack_req(op, cmd_meta):
    return (op & 0x07) | 0x00 | ((cmd_meta & 0xff) << 8)


def _pack_rsp(op, cmd_meta):
    return (op & 0x07) | 0x08 | ((cmd_meta & 0xff) << 8)


def _unpack(port_data):
    op = port_data & 0x07
    rsp = (port_data & 0x08) != 0
    cmd_meta = (port_data >> 8) & 0xff
    return op, rsp, cmd_meta


TX_META = {
    'dtype': 'u32',
    'brief': 'Data link TX state.',
    'default': 0,
    'options': [[0, 'disconnected'], [1, 'connected']],
    'flags': ['read_only'],
    'retain': 1,
}

EV_META = {
    'dtype': 'u32',
    'brief': 'Data link event',
    'default': 256,
    'options': [[0, 'unknown'], [1, 'rx_reset'], [2, 'tx_disconnected'], [3, 'tx_connected']],
    'flags': ['read_only'],
}

ECHO_ENABLE_META = {
    'dtype': 'bool',
    'brief': 'Enable echo',
    'default': 0,
    'retain': 1,
}

ECHO_OUTSTANDING_META = {
    'dtype': 'u32',
    'brief': 'Number of outstanding echo frames',
    'default': 8,
    'range': [1, 64],  # inclusive
    'retain': 1,
}

ECHO_LENGTH_META = {
    'dtype': 'u32',
    'brief': 'Length of each frame in bytes',
    'default': 256,
    'range': [8, 256],  # inclusive
    'retain': 1,
}


class Port0Server:
    OP_STATUS = 1
    OP_ECHO = 2
    OP_TIMESYNC = 3
    OP_META = 4
    OP_RAW = 5

    ST_INIT = 0
    ST_META = 1
    ST_DISCONNECTED = 2
    ST_CONNECTED = 3

    def __init__(self, pubsub, transport, port_id=None):
        self._state = self.ST_INIT

        self._echo_enable = ECHO_ENABLE_META['default']
        self._echo_outstanding = ECHO_OUTSTANDING_META['default']
        self._echo_length = ECHO_LENGTH_META['default']
        self._echo_tx_frame_id = 0
        self._echo_rx_frame_id = 0

        self._meta = [None] * PORTS_COUNT  # Metadata for each possible port
        self._meta_outstanding = 8
        self._meta_tx_port_id = 0
        self._meta_rx_port_id = 0
        self._transport = weakref.ref(transport)
        self._pubsub = weakref.ref(pubsub)
        self._recv_op = {
            self.OP_STATUS: self._recv_status,
            self.OP_ECHO: self._recv_echo,
            self.OP_TIMESYNC: self._recv_timesync,
            self.OP_META: self._recv_meta,
            self.OP_RAW: self._recv_raw,
        }
        transport.register_port(0, self)
        pubsub.meta('h/port/0/conn/tx', TX_META)
        pubsub.meta('h/port/0/conn/ev', EV_META)
        pubsub.create('h/port/0/echo/enable', ECHO_ENABLE_META, self._on_echo_enable)
        pubsub.create('h/port/0/echo/outstanding', ECHO_OUTSTANDING_META, self._on_echo_outstanding)
        pubsub.create('h/port/0/echo/length', ECHO_LENGTH_META, self._on_echo_length)

    def _on_echo_enable(self, topic, value):
        log.info('echo enable %s', value)
        self._echo_enable = value
        if value:
            self._echo_send()
        else:
            self._echo_rx_frame_id = 0
            self._echo_tx_frame_id = 0

    def _on_echo_outstanding(self, topic, value):
        self._echo_outstanding = int(value)
        self._echo_send()

    def _on_echo_length(self, topic, value):
        self._echo_length = int(value)
        self._echo_send()

    def _publish(self, topic, value, retain=None):
        pubsub = self._pubsub()
        if pubsub is not None:
            return pubsub.publish(topic, value, retain)

    def on_event(self, event):
        print(f'event {event}')
        self._publish('h/port/0/conn/ev', event)
        if event != Event.TX_CONNECTED:
            # reset echo
            self._echo_tx_frame_id = 0
            self._echo_rx_frame_id = 0

        if event == Event.TX_DISCONNECTED:
            self._publish('h/port/0/conn/tx', 0)
            if self._state == self.ST_CONNECTED:
                self._state = self.ST_DISCONNECTED
            elif self._state == self.ST_META:
                self._meta_tx_port_id = 0
                self._meta_rx_port_id = 0
        elif event == Event.TX_CONNECTED:
            self._publish('h/port/0/conn/tx', 1)
            if self._state == self.ST_INIT:
                self._state = self.ST_META
                log.info('starting port metadata scan')
                self._meta_scan()
            else:
                self._state = self.ST_CONNECTED
                self._echo_send()

    def _send(self, port_data, payload):
        transport = self._transport
        if transport is None:
            log.warning('send, but transport invalid')
            return 1
        transport = transport()
        if transport is None:
            log.warning('send, but transport invalid')
            return 1
        rv = transport.send(0, port_data, payload)
        if rv:
            log.warning('transport.send 0x%04x returned %d', port_data, rv)
        return rv

    def _recv_status(self, rsp, cmd_meta, msg):
        log.warning('recv_status(%d, %d, %s)', rsp, cmd_meta, msg)

    def _echo_send(self):
        while self._state == self.ST_CONNECTED and self._echo_enable and (self._echo_tx_frame_id - self._echo_rx_frame_id) < self._echo_outstanding:
            tx_u8 = np.zeros(self._echo_length, dtype=np.uint8)
            tx_u64 = tx_u8[:8].view(dtype=np.uint64)
            tx_u64[0] = self._echo_tx_frame_id
            self._echo_tx_frame_id += 1
            port_data = _pack_req(self.OP_ECHO, 0)
            rv = self._send(port_data, tx_u8)
            if rv:
                log.warning('echo send returned %d', rv)

    def _recv_echo(self, rsp, cmd_meta, msg):
        if rsp:
            msg_len = len(msg)
            if msg_len != self._echo_length or msg_len < 8:
                log.warning('unexpected message length %s', len(msg))
                return
            frame_id = msg[:8].view(dtype=np.uint64)[0]
            if frame_id != self._echo_rx_frame_id:
                log.warning('echo frame_id mismatch: %d != %d', frame_id, self._echo_rx_frame_id)
            self._echo_rx_frame_id = frame_id + 1
            self._echo_send()
        else:
            port_data = _pack_rsp(self.OP_ECHO, cmd_meta)
            self._send(port_data, msg)

    def _recv_timesync(self, rsp, cmd_meta, msg):
        if rsp:
            log.warning('unexpected timesync response')
        else:
            if len(msg) < 8:
                log.warning('Unexpected timesync length: %d', len(msg))
                return
            else:
                t0 = struct.unpack('<q', msg[:8])[0]
                t1 = now()
                t2 = t1
                t3 = 0
                msg = struct.pack('<qqqq', t0, t1, t2, t3)
                port_data = _pack_rsp(self.OP_TIMESYNC, cmd_meta)
                self._send(port_data, msg)

    def _meta_scan(self):
        payload = np.zeros(1, dtype=np.uint8)
        while (self._meta_tx_port_id < PORTS_COUNT) and (self._meta_tx_port_id - self._meta_rx_port_id) < self._meta_outstanding:
            port_data = _pack_req(self.OP_META, self._meta_tx_port_id)
            rv = self._send(port_data, payload)
            if rv:
                log.warning('meta send returned %d for port_id=%d', rv, self._meta_tx_port_id)
            self._meta_tx_port_id += 1

    def _recv_meta(self, rsp, cmd_meta, msg):
        if rsp:
            port_id = cmd_meta
            if port_id != self._meta_rx_port_id:
                log.warning('unexpected port_id %d != %d', port_id, self._meta_rx_port_id)
            self._meta[port_id] = payload_decode(PayloadType.JSON, msg)
            self._meta_rx_port_id = min(port_id + 1, self._meta_tx_port_id)
            if self._meta_rx_port_id >= PORTS_COUNT:
                if self._state == self.ST_META:
                    self._state = self.ST_CONNECTED
                self._meta_done()
            else:
                self._meta_scan()
        else:
            log.warning('received unexpected meta request')

    def _meta_done(self):
        transport = self._transport()
        pubsub = self._pubsub()
        if transport is not None:
            for port_id, meta in enumerate(self._meta):
                if port_id == 0 or meta is None:
                    continue
                port_type = meta['type']
                p = PORTS[port_type]
                transport.register_port(port_id, p(pubsub, transport, port_id))
        self._publish('h/port/0/meta', self._meta, retain=True)

    def _recv_raw(self, rsp, cmd_meta, msg):
        log.warning('recv_raw(%d, %d, %s)', rsp, cmd_meta, msg)

    def on_recv(self, port_data, msg):
        op, rsp, cmd_meta = _unpack(port_data)
        fn = self._recv_op.get(op)
        if fn is None:
            log.warning('unsupported op %d', op)
        else:
            fn(rsp, cmd_meta, msg)
