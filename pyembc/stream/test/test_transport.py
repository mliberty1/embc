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

import unittest
from pyembc.stream.transport import payload_encode, payload_decode, PayloadType, Transport
from pyembc.stream.data_link import Event
import numpy as np


class TransportEncoderTest(unittest.TestCase):

    def test_null_encode(self):
        dtype, v = payload_encode(None)
        self.assertEqual(PayloadType.NULL, dtype)
        self.assertEqual(1, len(v))
        self.assertIsNone(payload_decode(PayloadType.NULL, v))

    def test_u32_encode(self):
        dtype, v = payload_encode(42)
        self.assertEqual(PayloadType.U32, dtype)
        self.assertEqual(4, len(v))
        self.assertEqual(42, payload_decode(PayloadType.U32, v))

    def test_str_encode(self):
        msg = 'hello world'
        dtype, v = payload_encode(msg)
        self.assertEqual(PayloadType.STR, dtype)
        self.assertEqual(len(msg) + 1, len(v))
        self.assertEqual(msg, payload_decode(PayloadType.STR, v))

    def test_json_encode(self):
        msg = {'hello': 'world'}
        dtype, v = payload_encode(msg)
        self.assertEqual(PayloadType.JSON, dtype)
        self.assertEqual(msg, payload_decode(PayloadType.JSON, v))

    def test_bin_encode(self):
        msg = np.frombuffer(b'hello world', dtype=np.uint8)
        dtype, v = payload_encode(msg)
        self.assertEqual(PayloadType.BIN, dtype)
        self.assertEqual(len(msg), len(v))
        np.testing.assert_array_equal(msg, payload_decode(PayloadType.BIN, v))


class TransportTest(unittest.TestCase):

    def setUp(self):
        self.event = []
        self.recv = []
        self.send = []
        self.t = Transport(self.on_send)
        self.t.register_port(2, self)

    def on_event(self, event):
        self.event.append(event)

    def on_recv(self, port_data, msg):
        self.recv.append((port_data, msg))

    def on_send(self, metadata, payload):
        self.send.append((metadata, payload))

    def test_send(self):
        self.t.send(2, 0x1234, b'hello world')
        self.assertEqual([(0x1234c2, b'hello world')], self.send)

    def test_recv(self):
        msg = np.frombuffer(b'hello world', dtype=np.uint8)
        self.t.on_recv(0x1234c2, msg)
        self.assertEqual(1, len(self.recv))
        port_data, msg_out = self.recv[0]
        self.assertEqual(0x1234, port_data)
        np.testing.assert_array_equal(msg, msg_out)

    def test_on_event(self):
        self.t.on_event(Event.RX_RESET_REQUEST)
        self.assertEqual([Event.TX_DISCONNECTED, Event.RX_RESET_REQUEST], self.event)

    def test_on_event_when_connected(self):
        self.t.register_port(2, None)
        self.event.clear()
        self.t.on_event(Event.TX_CONNECTED)
        self.t.register_port(2, self)
        self.assertEqual([Event.TX_CONNECTED], self.event)

    def test_segment(self):
        msg = np.zeros(1025, dtype=np.uint8)
        for idx in range(len(msg)):
            msg[idx] = (idx & 0xff) + (idx >> 8)
        self.t.send(2, 0x1234, msg)
        self.assertEqual(5, len(self.send))
        for metadata, data in self.send:
            self.t.on_recv(metadata, data)
        self.assertEqual(1, len(self.recv))
        port_data, data = self.recv[0]
        self.assertEqual(0x1234, port_data)
        np.testing.assert_array_equal(msg, data)

