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
from unittest.mock import MagicMock
from pyembc.stream.framer import Framer, construct_ack
import numpy as np
import logging

logging.basicConfig(level=logging.DEBUG,
                    format="%(levelname)s:%(asctime)s:%(filename)s:%(lineno)d:%(name)s:%(message)s")


class TestFramer(unittest.TestCase):

    def setUp(self):
        self.recv = None
        self.send_done = None
        self.f = Framer(self.on_event, self, self)
        self.sent = []
        self.events = []
        logging.getLogger(__name__).info('setUp')

    def teardown(self):
        f, self.f = self.f, None
        del f

    def on_event(self, event):
        self.event.append(event)

    def open(self, recv, send_done):
        self.recv = recv
        self.send_done = send_done

    def close(self):
        self.recv = None
        self.send_done = None

    def send(self, buffer):
        self.sent.append(buffer)

    def test_initialization(self):
        self.assertIsNotNone(self.recv)
        self.assertIsNotNone(self.send_done)
        self.assertEqual(0, len(self.events))
        self.assertEqual(0, len(self.sent))

    def respond_with_ack(self, frame_id):
        ack = construct_ack(frame_id)
        self.f.ul_recv(ack)

    def test_send_one(self):
        send_done_cbk = MagicMock()
        recv_cbk = MagicMock()
        self.f.port_register(2, send_done_cbk, recv_cbk)
        payload = np.arange(8, dtype=np.uint8)
        self.f.send(0, 2, 4, payload)
        self.assertEqual(1, len(self.sent))
        np.testing.assert_equal(payload, self.sent[0][6:-4])
        self.respond_with_ack(0)
        send_done_cbk.assert_called()
