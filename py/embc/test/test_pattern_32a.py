# Copyright (c) 2017 Jetperch LLC.  All rights reserved.

import unittest
import embc


BYTES = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00]
INT = [0x00000000, 0xffff0000, 0x00000001]


class TestPattern(unittest.TestCase):

    def test_u32(self):
        tx = embc.PatternTx()
        rx = embc.PatternRx()
        for i in range(10):
            rx.next_u32(tx.next_u32())
        self.assertEqual(10, rx.receive_count)
        self.assertEqual(0, rx.missing_count)
        self.assertEqual(0, rx.error_count)
        self.assertEqual(0, rx.resync_count)

    def test_buffer(self):
        tx = embc.PatternTx()
        rx = embc.PatternRx()
        buf = embc.allocate_pattern_buffer(10)
        tx.next_buffer(buf)
        rx.next_buffer(buf)
        self.assertEqual(10, rx.receive_count)
        self.assertEqual(0, rx.missing_count)
        self.assertEqual(0, rx.error_count)
        self.assertEqual(0, rx.resync_count)

    def test_from_bytes(self):
        rx = embc.PatternRx()
        buf = bytes(BYTES)
        rx.next_buffer(buf)
        self.assertEqual(3, rx.receive_count)
        self.assertEqual(0, rx.missing_count)
        self.assertEqual(0, rx.error_count)
        self.assertEqual(0, rx.resync_count)

    def test_from_int(self):
        rx = embc.PatternRx()
        rx.next_buffer(INT)
        self.assertEqual(3, rx.receive_count)
        self.assertEqual(0, rx.missing_count)
        self.assertEqual(0, rx.error_count)
        self.assertEqual(0, rx.resync_count)
