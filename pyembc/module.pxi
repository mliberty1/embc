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


"""
EMBC initialization for a python module.
"""

from libc.stdint cimport intptr_t
from libc.stdlib cimport malloc, free
import logging
log = logging.getLogger(__name__)


LOG_LEVEL_MAP = {
    '!': logging.CRITICAL,
    'A': logging.CRITICAL,
    'C': logging.CRITICAL,
    'E': logging.ERROR,
    'W': logging.WARNING,
    'N': logging.INFO,
    'I': logging.INFO,
    'D': logging.DEBUG,
    '.': 0,
}

cdef extern from "stdarg.h":
    ctypedef struct va_list:
        pass
    ctypedef struct fake_type:
        pass
    void va_start(va_list, void* arg)
    void* va_arg(va_list, fake_type)
    void va_end(va_list)
    fake_type int_type "int"

cdef extern from "stdio.h":
    int vsnprintf (char * s, size_t n, const char * format, va_list arg )

cdef extern from "embc/platform.h":
    void embc_fatal(const char * file, int line, const char * msg)

cdef extern from "embc/log.h":
    ctypedef void(*embc_log_printf)(const char * format, ...)
    int embc_log_initialize(embc_log_printf handler)

cdef void log_printf(const char *fmt, ...):
    cdef va_list args
    cdef char[256] s
    va_start(args, <void*> fmt)
    vsnprintf(&s[0], 256, fmt, args)  # http://www.cplusplus.com/reference/cstdio/vsnprintf/
    va_end(args)
    msg = s.decode('utf-8')
    lvl = LOG_LEVEL_MAP[msg[0]]
    msg = msg[2:]
    if log.isEnabledFor(lvl):
        src_file, src_line, msg = msg.split(':', 2)
        msg = msg.strip()
        if src_file.startswith('src/'):
            src_file = src_file[4:]
        record = log.makeRecord(src_file, lvl, src_file, int(src_line), msg, [], None, None, None, None)
        log.handle(record)

embc_log_initialize(log_printf)


cdef extern from "embc/platform.h":
    void embc_fatal(const char * file, int line, const char * msg)
    ctypedef intptr_t embc_size_t
    void * embc_alloc(embc_size_t size_bytes)
    void embc_free(void * ptr)

cdef void embc_fatal(const char * file, int line, const char * msg):
    raise RuntimeError(f'embc_fatal({file}, {line}, {msg}')

cdef void * embc_alloc(embc_size_t size_bytes):
    return malloc(size_bytes)

cdef void embc_free(void * ptr):
    free(ptr)
