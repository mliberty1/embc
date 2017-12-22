# Copyright (c) 2017 Jetperch LLC.  All rights reserved.

import unittest
from unittest.mock import MagicMock, call
import embc


def buffer_to_hex(buffer):
    return ' '.join(['%02x' % x for x in buffer])


class TestFramer(unittest.TestCase):

    def setUp(self):
        self.f1 = embc.stream.framer.Framer()
        self.f2 = embc.stream.framer.Framer()
        self.f1.hal_tx = self.f1_to_f2
        self.f2.hal_tx = self.f2_to_f1

        self.f1_to_f2_queue = []
        self.f2_to_f1_queue = []

        self.f1_rx = MagicMock()
        self.f2_rx = MagicMock()
        self.f1_tx_done = MagicMock()
        self.f2_tx_done = MagicMock()

        for i in range(0, 16):
            self.f1.register_port(i, self.f1_rx, self.f1_tx_done)
            self.f2.register_port(i, self.f2_rx, self.f2_tx_done)

    def tearDown(self):
        del self.f1
        self.f1 = None

    def f1_to_f2(self, x):
        # print(' -> %s' % buffer_to_hex(x))
        self.f1_to_f2_queue.append(x)

    def f2_to_f1(self, x):
        # print(' <- %s' % buffer_to_hex(x))
        self.f2_to_f1_queue.append(x)

    def process(self):
        processing = True
        while processing:
            processing = False
            if len(self.f1_to_f2_queue):
                x = self.f1_to_f2_queue.pop(0)
                self.f2.hal_rx(x)
                processing = True
            if len(self.f2_to_f1_queue):
                x = self.f2_to_f1_queue.pop(0)
                self.f1.hal_rx(x)
                processing = True

    def test_normal(self):
        # print(self.f1.status)
        self.f1.send(1, 1, 3, b'hello 1')
        self.process()
        self.f1.send(1, 2, 3, b'hello 2')
        self.process()
        self.f1.send(1, 3, 3, b'hello 3')
        self.process()
        # print(self.f1.status)
        # print(self.f2.status)
        self.f2_rx.assert_has_calls([
            call(1, 1, 3, b'hello 1'),
            call(1, 2, 3, b'hello 2'),
            call(1, 3, 3, b'hello 3')])
        self.f1_tx_done.assert_has_calls([
            call(1, 1, 0),
            call(1, 2, 0),
            call(1, 3, 0)])

    def test_ping(self):
        # print('test_ping')
        expected = []
        for i in range(128):
            message_id = i & 0xff
            payload = b'hello %d' % i
            self.f1.send(0, message_id, embc.stream.framer.Port0.PING_REQ, payload)
            expected.append(call(0, message_id, embc.stream.framer.Port0.PING_RSP, payload))
            self.process()
        print(self.f1.status)
        print(self.f2.status)
        self.f1_rx.assert_has_calls(expected)
