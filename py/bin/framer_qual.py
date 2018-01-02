#!/usr/bin/env python3
# Copyright 2017 Jetperch LLC.  All rights reserved.

"""
A python EMBC framer for PCs.

This framer executable is used to test and qualify microcontroller
implementations.
"""


import argparse
import sys
import os
import time
import signal
import serial

MYPATH = os.path.abspath(os.path.dirname(__file__))
LIBPATH = os.path.dirname(MYPATH)
sys.path.insert(0, LIBPATH)

import embc


def get_parser():
    parser = argparse.ArgumentParser(description='Test EMBC framer implementations.')
    parser.add_argument('--port',
                        help='The serial comm port / device to open')
    parser.add_argument('--baudrate',
                        default=115200,
                        help='The baud rate (defaults to 115200)')
    parser.add_argument('--ping',
                        action='store_true',
                        help='Send ping frames')
    return parser


class MasterFramer:

    def __init__(self, port, baudrate, ping=None):
        self._ping = ping
        # port=None defers open until explict open().
        self._serial = serial.Serial(port=None, baudrate=baudrate, timeout=0.001)
        self._serial.port = port
        self._framer = embc.stream.framer.Framer()
        self._framer.hal_tx = self._serial.write
        self.message_id = 0
        for i in range(0, 16):
            self._framer.register_port(i, self.rx, self.tx_done)

    def rx(self, port, message_id, port_def, data):
        print("%s %s %s %s" % (port, message_id, port_def, data))

    def send_ping(self):
        if self._ping:
            message_id = self.message_id & 0x7f
            self.message_id += 1
            msg = b'hello %d' % message_id
            print('send_ping "%s"' % msg)
            self._framer.send(0, message_id, embc.stream.framer.Port0.PING_REQ, msg)

    def tx_done(self, port, message_id, status):
        if 0 == status:
            print('tx_done success')
        else:
            print('tx_done error %s' % embc.ec.num_to_name.get(status, 'unknown'))
        if port == 0:
            self.send_ping()

    def open(self):
        self._serial.open()
        self.send_ping()

    def process(self):
        b = self._serial.read()
        if len(b):
            self._framer.hal_rx(b)
        self._framer.process()


def run():
    args = get_parser().parse_args()
    quit = False
    framer = MasterFramer(port=args.port, baudrate=args.baudrate, ping=args.ping)
    framer.open()

    def do_quit(*args, **kwargs):
        nonlocal quit
        print('quit')
        quit = True

    signal.signal(signal.SIGINT, do_quit)

    print('start')
    while not quit:
        framer.process()
        time.sleep(0.001)
    print('stop')

    return 0


if __name__ == "__main__":
    sys.exit(run())
