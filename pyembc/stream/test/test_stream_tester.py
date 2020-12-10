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

import unittest
from pyembc.stream.stream_tester import StreamTester
import numpy as np


class StreamFramer(unittest.TestCase):

    def setUp(self):
        self.s = StreamTester()
        self.s.a.open(self.on_recv_a, None)
        self.s.b.open(self.on_recv_b, None)
        self.s.buffer_drop_rate = 0
        self.s.buffer_insert_rate = 0
        self.s.byte_drop_rate = 0
        self.s.byte_insert_rate = 0
        self.s.bit_error_rate = 0
        self.recv_a = []
        self.recv_b = []

    def on_recv_a(self, buffer):
        self.recv_a.append(buffer)

    def on_recv_b(self, buffer):
        self.recv_b.append(buffer)

    def test_no_permute(self):
        send_a = []
        send_b = []
        for x in range(256):
            buf = np.array([0, x], dtype=np.uint8)
            send_a.append(buf)
            self.s.a.send(buf)
        for x in range(256):
            buf = np.array([1, x], dtype=np.uint8)
            send_b.append(buf)
            self.s.b.send(buf)

        self.assertEqual(0, len(self.recv_a))
        self.assertEqual(0, len(self.recv_b))

        self.s.process()
        self.assertEqual(256, len(self.recv_a))
        self.assertEqual(256, len(self.recv_b))
        np.testing.assert_equal(np.array(send_a), np.array(self.recv_b))
        np.testing.assert_equal(np.array(send_b), np.array(self.recv_a))

    def test_drop_none(self):
        self.s.a.send(np.array([1, 2, 3], dtype=np.uint8))
        self.s.process()
        self.assertEqual(1, len(self.recv_b))

    def test_drop_all(self):
        self.s.buffer_drop_rate = 1.0
        self.s.a.send(np.array([1, 2, 3], dtype=np.uint8))
        self.s.process()
        self.assertEqual(0, len(self.recv_b))

    def test_insert_all(self):
        self.s.buffer_insert_rate = 1.0
        self.s.a.send(np.array([1, 2, 3], dtype=np.uint8))
        self.s.process()
        self.assertEqual(2, len(self.recv_b))

    def test_byte_drop_rate(self):
        self.s.byte_drop_rate = 1
        self.s.a.send(np.array([1, 2, 3], dtype=np.uint8))
        self.s.process()
        self.assertEqual(1, len(self.recv_b))
        self.assertEqual(0, len(self.recv_b[0]))

    def test_byte_insert_rate(self):
        self.s.byte_insert_rate = 1
        self.s.a.send(np.array([1, 2, 3], dtype=np.uint8))
        self.s.process()
        self.assertEqual(1, len(self.recv_b))
        self.assertEqual(6, len(self.recv_b[0]))

    def test_bit_error_rate(self):
        self.s.bit_error_rate = 1
        self.s.a.send(np.array([0, 1, 0xaa, 0x55], dtype=np.uint8))
        self.s.process()
        self.assertEqual(1, len(self.recv_b))
        np.testing.assert_equal([0xFF, 0xFE, 0x55, 0xaa], self.recv_b[0])
