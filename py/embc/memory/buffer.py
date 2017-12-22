# Copyright 2017 Jetperch LLC

import os
import ctypes
from ctypes import cdll, Structure, POINTER, pointer, cast, \
    c_uint64, c_uint32, c_uint16, c_uint8, c_void_p, c_size_t, c_char_p
from ctypes.wintypes import DWORD, HANDLE, BOOL, LPVOID, LPWSTR
from embc import dll as _dll
from ..collections.list import embc_list_s


class embc_buffer_s(Structure):
    _fields_ = [
        ('data', POINTER(c_uint8)),
        ('capacity', c_uint16),
        ('cursor', c_uint16),
        ('length', c_uint16),
        ('reserve', c_uint16),
        ('buffer_id', c_uint16),
        ('item', embc_list_s),
    ]

    @property
    def write_remaining(self):
        return self.capacity - self.cursor - self.reserve

    @property
    def read_reamaining(self):
        return self.length - self.cursor

    def cursor_set(self, index):
        assert(0 <= index <= self.length)
        self.cursor = index

    def reset(self):
        self.cursor = 0
        self.length = 0

    def read_remaining(self):
        return bytes(self.data[self.cursor:self.length])

    def read_all(self):
        return bytes(self.data[0:self.length])

    def write(self, b):
        """Append a bytes-like value

        :param b: The bytes-like value
        """
        v = ctypes.create_string_buffer(b)
        write(self, v, len(b))

    def erase(self, start, end):
        erase(self, start, end)

    def free(self):
        free(self)



# struct embc_buffer_allocator_s * embc_buffer_initialize(embc_size_t const * sizes, embc_size_t length);
initialize = _dll.embc_buffer_initialize
initialize.restype = c_void_p
initialize.argtypes = [POINTER(c_size_t), c_size_t]

# void embc_buffer_finalize(struct embc_buffer_allocator_s * self);
finalize = _dll.embc_buffer_finalize
finalize.restype = None
finalize.argtypes = [c_void_p]

# struct embc_buffer_s * embc_buffer_alloc(struct embc_buffer_allocator_s * self, embc_size_t size);
alloc = _dll.embc_buffer_alloc
alloc.restype = POINTER(embc_buffer_s)
alloc.argtypes = [c_void_p, c_size_t]

# void embc_buffer_free(struct embc_buffer_s * buffer);
free = _dll.embc_buffer_free
free.restype = None
free.argtypes = [POINTER(embc_buffer_s)]

# void embc_buffer_write(struct embc_buffer_s * buffer,
#                        void const * data,
#                        embc_size_t size);
write = _dll.embc_buffer_write
write.restype = None
write.argtypes = [POINTER(embc_buffer_s), c_void_p, c_size_t]

# void embc_buffer_copy(struct embc_buffer_s * destination,
#                       struct embc_buffer_s * source,
#                       embc_size_t size);
copy = _dll.embc_buffer_copy
copy.restype = None
copy.argtypes = [POINTER(embc_buffer_s), POINTER(embc_buffer_s), c_size_t]


# void embc_buffer_write_str(struct embc_buffer_s * buffer,
#                            char const * str);
write_str = _dll.embc_buffer_write_str
write_str.restype = None
write_str.argtypes = [POINTER(embc_buffer_s), c_char_p]

# bool embc_buffer_write_str_truncate(struct embc_buffer_s * buffer,
#                                     char const * str);
write_str_truncate = _dll.embc_buffer_write_str_truncate
write_str_truncate.restype = None
write_str_truncate.argtypes = [POINTER(embc_buffer_s), c_char_p]

# void embc_buffer_write_u8(struct embc_buffer_s * buffer, uint8_t value);
write_u8 = _dll.embc_buffer_write_u8
write_u8.restype = None
write_u8.argtypes = [POINTER(embc_buffer_s), c_uint8]

# void embc_buffer_write_u16_le(struct embc_buffer_s * buffer, uint16_t value);
write_u16_le = _dll.embc_buffer_write_u16_le
write_u16_le.restype = None
write_u16_le.argtypes = [POINTER(embc_buffer_s), c_uint16]

# void embc_buffer_write_u32_le(struct embc_buffer_s * buffer, uint32_t value);
write_u32_le = _dll.embc_buffer_write_u32_le
write_u32_le.restype = None
write_u32_le.argtypes = [POINTER(embc_buffer_s), c_uint32]

# void embc_buffer_write_u64_le(struct embc_buffer_s * buffer, uint64_t value);
write_u64_le = _dll.embc_buffer_write_u64_le
write_u64_le.restype = None
write_u64_le.argtypes = [POINTER(embc_buffer_s), c_uint64]

# void embc_buffer_write_u16_be(struct embc_buffer_s * buffer, uint16_t value);
write_u16_be = _dll.embc_buffer_write_u16_be
write_u16_be.restype = None
write_u16_be.argtypes = [POINTER(embc_buffer_s), c_uint16]

# void embc_buffer_write_u32_be(struct embc_buffer_s * buffer, uint32_t value);
write_u32_be = _dll.embc_buffer_write_u32_be
write_u32_be.restype = None
write_u32_be.argtypes = [POINTER(embc_buffer_s), c_uint32]

# void embc_buffer_write_u64_be(struct embc_buffer_s * buffer, uint64_t value);
write_u64_be = _dll.embc_buffer_write_u64_be
write_u64_be.restype = None
write_u64_be.argtypes = [POINTER(embc_buffer_s), c_uint64]

# void embc_buffer_read(struct embc_buffer_s * buffer,
#                       void * data,
#                       embc_size_t size);
read = _dll.embc_buffer_read
read.restype = None
read.argtypes = [POINTER(embc_buffer_s), c_void_p, c_size_t]

# uint8_t embc_buffer_read_u8(struct embc_buffer_s * buffer);
read_u8 = _dll.embc_buffer_read_u8
read_u8.restype = c_uint8
read_u8.argtypes = [POINTER(embc_buffer_s)]

# uint16_t embc_buffer_read_u16_le(struct embc_buffer_s * buffer);
read_u16_le = _dll.embc_buffer_read_u16_le
read_u16_le.restype = c_uint16
read_u16_le.argtypes = [POINTER(embc_buffer_s)]

# uint32_t embc_buffer_read_u32_le(struct embc_buffer_s * buffer);
read_u32_le = _dll.embc_buffer_read_u32_le
read_u32_le.restype = c_uint32
read_u32_le.argtypes = [POINTER(embc_buffer_s)]

# uint64_t embc_buffer_read_u64_le(struct embc_buffer_s * buffer);
read_u64_le = _dll.embc_buffer_read_u64_le
read_u64_le.restype = c_uint64
read_u64_le.argtypes = [POINTER(embc_buffer_s)]

# uint16_t embc_buffer_read_u16_be(struct embc_buffer_s * buffer);
read_u16_be = _dll.embc_buffer_read_u16_be
read_u16_be.restype = c_uint16
read_u16_be.argtypes = [POINTER(embc_buffer_s)]

# uint32_t embc_buffer_read_u32_be(struct embc_buffer_s * buffer);
read_u32_be = _dll.embc_buffer_read_u32_be
read_u32_be.restype = c_uint32
read_u32_be.argtypes = [POINTER(embc_buffer_s)]

# uint64_t embc_buffer_read_u64_be(struct embc_buffer_s * buffer);
read_u64_be = _dll.embc_buffer_read_u64_be
read_u64_be.restype = c_uint64
read_u64_be.argtypes = [POINTER(embc_buffer_s)]

# void embc_buffer_erase(struct embc_buffer_s * buffer,
#                        embc_size_t start,
#                        embc_size_t end);
erase = _dll.embc_buffer_erase
erase.restype = c_void_p
erase.argtypes = [POINTER(embc_buffer_s), c_size_t, c_size_t]

