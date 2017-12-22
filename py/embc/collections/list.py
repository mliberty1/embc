# Copyright 2017 Jetperch LLC

import os
from ctypes import cdll, Structure, POINTER, pointer, cast, \
    c_uint64, c_uint32, c_uint16, c_uint8
from ctypes.wintypes import DWORD, HANDLE, BOOL, LPVOID, LPWSTR

class embc_list_s(Structure):
    pass
    
embc_list_s._fields_ = [
    ('next', POINTER(embc_list_s)),
    ('prev', POINTER(embc_list_s))
]

def reset(i):
    i.next = pointer(i)
    i.prev = pointer(i)

def new():
    i = embc_list_s()
    reset(i)
