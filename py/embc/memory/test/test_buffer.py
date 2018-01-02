# Copyright (c) 2017 Jetperch LLC.  All rights reserved.

import unittest
import embc

import ctypes


class TestBuffer(unittest.TestCase):

    def test_basic(self):
        sizes = (ctypes.c_size_t * 5)(8, 8, 8, 8, 8)
        a = embc.memory.buffer.initialize(sizes, len(sizes))
        bptr = embc.memory.buffer.alloc(a, 32)
        b = bptr[0]
        b.write(b'hello')
        self.assertEqual(b'hello', b.read_all())
        self.assertEqual(5, b.length)
        b.erase(1, 4)
        self.assertEqual(2, b.length)
        b.cursor = 0
        self.assertEqual(b'ho', b.read_all())
        embc.memory.buffer.free(bptr)
        embc.memory.buffer.finalize(a)

    def get_buffer_addresses(self, allocator, size, count):
        # alloc all buffers
        addrs = []
        buffers = []
        for i in range(count):
            b = embc.memory.buffer.alloc(allocator, size)
            addrs.append(ctypes.addressof(b.contents))
            buffers.append(b)
        # free all buffers
        for b in buffers:
            b[0].free()
        return addrs

    def test_alloc_free(self):
        count = 8
        sizes = (ctypes.c_size_t * 1)(count)
        a = embc.memory.buffer.initialize(sizes, len(sizes))
        addr1 = self.get_buffer_addresses(a, 32, count)
        addr2 = []
        # alloc one and immediately free
        for i in range(count):
            b = embc.memory.buffer.alloc(a, 32)
            addr2.append(ctypes.addressof(b.contents))
            embc.memory.buffer.free(b)
        self.assertEqual(addr1, addr2)
        embc.memory.buffer.finalize(a)

    def test_alloc_all(self):
        count = 4
        sizes = (ctypes.c_size_t * 1)(count)
        a = embc.memory.buffer.initialize(sizes, len(sizes))
        addr1 = self.get_buffer_addresses(a, 32, count)
        buffers = []
        read_index = 0
        for write_index in range(3 * count):
            b = embc.memory.buffer.alloc(a, 32)
            self.assertIn(ctypes.addressof(b.contents), addr1)
            b[0].write(bytes([write_index]))
            buffers.append(b)
            if write_index >= (count - 1):
                b = buffers.pop(0)
                self.assertEqual(bytes([read_index]), b[0].read_all())
                b[0].free()
                read_index = read_index + 1
