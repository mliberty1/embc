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
        status_expected = dict(
            version=0, rx_count=256, rx_data_count=128, rx_ack_count=128,
            rx_deduplicate_count=0, rx_synchronization_error=0, rx_mic_error=0,
            rx_frame_id_error=0, tx_count=128, tx_retransmit_count=0)
        for key, value in status_expected.items():
            self.assertEqual(value, getattr(self.f1.status, key))
            self.assertEqual(value, getattr(self.f2.status, key))
        self.f1_rx.assert_has_calls(expected)

    def test_remote_status(self):
        self.f1.send(0, 0, embc.stream.framer.Port0.PING_REQ, b'ping1')
        self.f1.send(0, 1, embc.stream.framer.Port0.PING_REQ, b'ping2')
        self.f1.send(0, 2, embc.stream.framer.Port0.STATUS_REQ, b'data?')
        self.process()
        self.assertEqual(3, self.f1_rx.call_count)
        args = self.f1_rx.call_args[0]
        self.assertEqual((0, 2, embc.stream.framer.Port0.STATUS_RSP), args[:3])
        payload = args[3]
        self.assertEqual(payload,
                         b'\x00\x00\x00\x00\x03\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')

    def test_timeout(self):
        self.f1.send(1, 2, 3, b'hello 1')
        for i in range(embc.stream.framer.MAX_RETRIES + 1):
            self.f1_tx_done.assert_not_called()
            self.assertEqual(1, len(self.f1_to_f2_queue))
            self.f1_to_f2_queue = []
            self.assertIsNotNone(self.f1.timeout)
            self.f1.timeout -= 1.0
            self.f1.process()
        self.f1_tx_done.assert_called_once_with(1, 2, embc.ec.TIMED_OUT)
