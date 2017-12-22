import os
import sys
from ctypes import cdll, CFUNCTYPE, \
    c_void_p, c_size_t, c_char_p, c_int
import logging
log = logging.getLogger(__name__)

path = os.path.dirname(os.path.abspath(__file__))
dll = cdll.LoadLibrary(os.path.join(path, 'libembc.dll'))

# void embc_lib_initialize();
initialize = dll.embc_lib_initialize
initialize.restype = None
initialize.argtypes = []
initialize()

# void (*embc_lib_fatal_fn)(void * user_data, char const * file, int line, char const * msg);
FATAL_CBK = CFUNCTYPE(None, c_void_p, c_char_p, c_int, c_char_p)

# void (*embc_lib_print_fn)(void * user_data, char const * str);
PRINT_CBK = CFUNCTYPE(None, c_void_p, c_char_p)

# void embc_lib_fatal_set(embc_lib_fatal_fn fn, void * user_data);
fatal_set = dll.embc_lib_fatal_set
fatal_set.argtypes = [FATAL_CBK, c_void_p]
fatal_set.restype = None

# void embc_lib_print_set(embc_lib_print_fn fn, void * user_data);
print_set = dll.embc_lib_print_set
print_set.argtypes = [PRINT_CBK, c_void_p]
print_set.restype = None

# void * embc_lib_alloc(embc_size_t sz);
alloc = dll.embc_lib_alloc
alloc.argtypes = [c_size_t]
alloc.restype = c_void_p

# void embc_lib_free(void * ptr);
free = dll.embc_lib_free
free.argtypes = [c_void_p]
free.restype = None

def fatal(obj, filename, linenum, message):
    log.critical('%s %d %s', filename, linenum, message)
    assert(False)


def print_(obj, message):
    sys.stdout.write(message.decode('utf-8'))

_fatal_cbk = FATAL_CBK(fatal)
_print_cbk = PRINT_CBK(print_)
fatal_set(_fatal_cbk, None)
print_set(_print_cbk, None)
