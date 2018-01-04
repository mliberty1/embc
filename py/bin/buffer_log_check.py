#!/usr/bin/env python3
# Copyright 2018 Jetperch LLC.  All rights reserved.

"""
Check the log files for buffer alloc/free pairs.
"""

import argparse
import sys


def get_parser():
    parser = argparse.ArgumentParser(description='Check log file for alloc/free pairs.')
    parser.add_argument('logfile',
                        type=argparse.FileType('rt'),
                        help='The path to the log filename')
    return parser


class Alloc:

    def __init__(self, line, ptr):
        self.alloc_line = line
        self.free_line = None
        self.ptr = ptr

    def __str__(self):
        return '%s: alloc on line %s, free on line %s' % (self.ptr, self.alloc_line, self.free_line)


ALLOC = 'embc_buffer_alloc'
FREE = 'embc_buffer_free'


def run():
    args = get_parser().parse_args()
    ptr_history = {}
    buffers = {}
    line_num = 0
    for line in args.logfile:
        line_num += 1
        alloc_idx = line.find(ALLOC)
        if alloc_idx >= 0:
            ptr = line[alloc_idx + len(ALLOC) + 1:-1]
            b = buffers.get(ptr)
            if b is not None:
                print('line %d: alloc without free: %s' % (line_num, b))
            b = Alloc(line_num, ptr)
            buffers[ptr] = b
            ptr_history[ptr] = b
            continue
        free_idx = line.find(FREE)
        if free_idx >= 0:
            ptr = line[free_idx + len(FREE) + 1:-1]
            b = buffers.get(ptr)
            if b is None:
                b = ptr_history.get(ptr)
                print('line %d: free without alloc: %s' % (line_num, b))
            else:
                del buffers[ptr]
            continue
    if buffers:
        print('Alloc without free')
        for b in buffers.values():
            print(b)

    return 0


if __name__ == "__main__":
    sys.exit(run())
