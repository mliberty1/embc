# Copyright 2017 Jetperch LLC

import os
from ctypes import cdll, Structure, POINTER, pointer, cast, \
    c_uint64, c_uint32, c_uint16, c_uint8
from ctypes.wintypes import DWORD, HANDLE, BOOL, LPVOID, LPWSTR
from embc.lib import dll as _dll


class embc_pattern_32a_tx_s(Structure):
    _fields_ = [
        ('shift32', c_uint32),
        ('counter', c_uint16),
        ('toggle', c_uint8),
    ]


class embc_pattern_32a_rx_s(Structure):
    _fields_ = [
        ('tx', embc_pattern_32a_tx_s),
        ('receive_count', c_uint64),
        ('missing_count', c_uint64),
        ('duplicate_count', c_uint64),
        ('error_count', c_uint64),
        ('resync_count', c_uint32),
        ('syncword1', c_uint32),
        ('state', c_uint8),
    ]


# void embc_pattern_32a_tx_initialize(
#       struct embc_pattern_32a_tx_s * self);
pattern_32a_tx_initialize = _dll.embc_pattern_32a_tx_initialize
pattern_32a_tx_initialize.restype = None
pattern_32a_tx_initialize.argtypes = [POINTER(embc_pattern_32a_tx_s)]

# uint32_t embc_pattern_32a_tx_next(
#        struct embc_pattern_32a_tx_s * self);
pattern_32a_tx_next = _dll.embc_pattern_32a_tx_next
pattern_32a_tx_next.restype = c_uint32
pattern_32a_tx_next.argtypes = [POINTER(embc_pattern_32a_tx_s)]

# void embc_pattern_32a_tx_buffer(
#        struct embc_pattern_32a_tx_s * self,
#        uint32_t * buffer,
#        uint32_t size);
pattern_32a_tx_buffer = _dll.embc_pattern_32a_tx_buffer
pattern_32a_tx_buffer.restype = None
pattern_32a_tx_buffer.argtypes = [POINTER(embc_pattern_32a_tx_s), POINTER(c_uint32), c_uint32]

# void embc_pattern_32a_rx_initialize(
#        struct embc_pattern_32a_rx_s * self);
pattern_32a_rx_initialize = _dll.embc_pattern_32a_rx_initialize
pattern_32a_rx_initialize.restype = None
pattern_32a_rx_initialize.argtypes = [POINTER(embc_pattern_32a_rx_s)]

# void embc_pattern_32a_rx_next(
#        struct embc_pattern_32a_rx_s * self,
#        uint32_t value);
pattern_32a_rx_next = _dll.embc_pattern_32a_rx_next
pattern_32a_rx_next.restype = None
pattern_32a_rx_next.argtypes = [POINTER(embc_pattern_32a_rx_s), c_uint32]

# void embc_pattern_32a_rx_buffer(
#        struct embc_pattern_32a_rx_s * self,
#        uint32_t const * buffer,
#        uint32_t size);
pattern_32a_rx_buffer = _dll.embc_pattern_32a_rx_buffer
pattern_32a_rx_buffer.restype = None
pattern_32a_rx_buffer.argtypes = [POINTER(embc_pattern_32a_rx_s), POINTER(c_uint32), c_uint32]


def allocate_pattern_buffer(length_words):
    """Allocate a pattern buffer

    :param length_words: The length of the buffer in bytes.
    :return: The pattern buffer.
    """
    return (c_uint32 * length_words)()


class PatternTx:

    def __init__(self):
        self._native = embc_pattern_32a_tx_s()
        self._p = pointer(self._native)
        pattern_32a_tx_initialize(self._p)
        pattern_32a_tx_initialize(self._p)

    def next_u32(self):
        return pattern_32a_tx_next(self._p)

    def next_buffer(self, buffer_u32):
        p = cast(buffer_u32, POINTER(c_uint32))
        v = len(buffer_u32) * 4
        pattern_32a_tx_buffer(self._p, p, v)


class PatternRx:

    def __init__(self):
        self._native = embc_pattern_32a_rx_s()
        self._p = pointer(self._native)
        pattern_32a_rx_initialize(self._p)
        pattern_32a_rx_initialize(self._p)

    @property
    def receive_count(self):
        return self._native.receive_count

    @property
    def missing_count(self):
        return self._native.missing_count

    @property
    def error_count(self):
        return self._native.error_count

    @property
    def resync_count(self):
        return self._native.resync_count

    def next_u32(self, data):
        pattern_32a_rx_next(self._p, c_uint32(data))

    def next_buffer(self, buffer_u32):
        if isinstance(buffer_u32, bytes):
            v = len(buffer_u32)
            assert 0 == (v & 0x3)
            z = (c_uint8 * v).from_buffer_copy(buffer_u32)
            p = cast(z, POINTER(c_uint32))
            pattern_32a_rx_buffer(self._p, p, v)
        elif isinstance(buffer_u32, list):
            v = len(buffer_u32)
            buffer_u32 = (c_uint32 * v)(*buffer_u32)
            p = cast(buffer_u32, POINTER(c_uint32))
            pattern_32a_rx_buffer(self._p, p, v * 4)
        else:  # presume ctypes.c_uint32 * k
            p = cast(buffer_u32, POINTER(c_uint32))
            v = len(buffer_u32) * 4
            pattern_32a_rx_buffer(self._p, p, v)
