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
import logging
log = logging.getLogger()

MYPATH = os.path.abspath(os.path.dirname(__file__))
LIBPATH = os.path.dirname(MYPATH)
sys.path.insert(0, LIBPATH)

import embc


PING_DEPTH = 3  # default setting


def get_parser():
    parser = argparse.ArgumentParser(description='Test EMBC framer implementations.')
    parser.add_argument('--port',
                        help='The serial comm port / device to open')
    parser.add_argument('--baudrate',
                        default=115200,
                        help='The baud rate (defaults to 115200)')
    parser.add_argument('--ping',
                        default=0,
                        type=int,
                        help='Send ping frames')
    return parser


class PingFrame:

    def __init__(self, message_id, payload):
        self.tx_pending = True
        self.tx_done_pending = True
        self.rx_pending = True
        self.message_id_raw = message_id
        self.message_id = message_id & 0xff
        self.payload = payload


class MasterFramer:

    def __init__(self, port, baudrate):
        # port=None defers open until explict open().
        self._serial = serial.Serial(port=None, baudrate=baudrate, timeout=0.002)
        self._serial.port = port
        self._framer = embc.stream.framer.Framer()
        self._framer.hal_tx = self._serial.write
        self.message_id = 0
        self._ping_queue = []
        self._ping_start_time = None
        self._ping_payload_size = None
        for i in range(0, 16):
            self._framer.register_port(i, self.rx, self.tx_done)

    def _ping_remove(self, idx):
        msg = self._ping_queue[idx]
        if not msg.tx_done_pending and not msg.rx_pending:
            self._ping_queue.pop(idx)
        if not self._ping_queue:
            stop_time = time.time()
            dt = stop_time - self._ping_start_time
            throughput = self._ping_payload_size / dt
            throughput_max = self._serial.baudrate / 10
            overhead = 1.0 - throughput / throughput_max
            print('ping stop: %d Bps bidirectional of %d (%.2f%% total overhead)' % (throughput, throughput_max, overhead * 100))
            print(self._framer.status)
        else:
            self.send_ping()

    def rx(self, port, message_id, port_def, data):
        if 0 == port and embc.stream.framer.Port0.PING_RSP == port_def:
            if not self._ping_queue:  # error
                # print("rx ping response %d, but not expected" % message_id)
                return
            msg = self._ping_queue[0]
            if msg.message_id != message_id:
                log.debug('rx ping response message_id mismatch: %d != %d',
                          msg.message_id, message_id)
                # todo resync?
                return
            if msg.payload != data:
                log.warning('rx ping response %d payload mismatch', message_id)
            else:
                log.debug('rx ping response %d', message_id)
            msg.rx_pending = False
            self._ping_remove(0)
        else:
            log.info('%s %s %s %s', port, message_id, port_def, data)

    def start_ping(self, count):
        count = int(count)
        tx = embc.PatternTx()
        self._ping_queue = []
        self._ping_payload_size = 240 * count
        for message_id in range(count):
            payload = []
            for k in range(0, 240, 4):
                word = tx.next_u32()
                word_bytes = [word & 0xff,
                              (word >> 8) & 0xff,
                              (word >> 16) & 0xff,
                              (word >> 24) & 0xff]
                payload.extend(word_bytes)
            payload = bytes(payload)
            self._ping_queue.append(PingFrame(message_id, payload))
        print('ping start')
        self._ping_start_time = time.time()
        self.send_ping()

    def send_ping(self):
        awaiting = 0
        for msg in self._ping_queue:
            if awaiting >= PING_DEPTH:
                break
            if not msg.tx_pending:
                awaiting += 1
            else:
                msg.tx_pending = False
                awaiting += 1
                log.debug('tx ping %d', msg.message_id)
                msg.tx_pending = False
                self._framer.send(0, msg.message_id, embc.stream.framer.Port0.PING_REQ, msg.payload)

    def _get_tx_done_message(self, message_id):
        for idx, msg in enumerate(self._ping_queue):
            if msg.tx_pending:
                return 0, None
            if not msg.tx_pending and msg.tx_done_pending and msg.message_id == message_id:
                return idx, msg

    def tx_done(self, port, message_id, port_def, status):
        if 0 == port and embc.stream.framer.Port0.PING_REQ == port_def:
            idx, msg = self._get_tx_done_message(message_id)
            if msg is None:
                log.warning('tx_done ping %d with no matching tx message', message_id)
                return
            if 0 == status:
                msg.tx_done_pending = False
                log.debug('tx_done ping %d', message_id)
            else:  # error, just skip
                msg.tx_done_pending = False
                msg.rx_pending = False
                log.info('tx_done ping error %s: message_id=%d',
                         embc.ec.num_to_name.get(status, 'unknown'), message_id)
            self._ping_remove(idx)
        elif status:
            log.info('tx_done error %s: port=%d, message_id=%d, port_def=0x%04x',
                     embc.ec.num_to_name.get(status, 'unknown'), port, message_id, port_def)

    def open(self):
        self._serial.open()

    def close(self):
        self._serial.close()

    def process(self):
        b = self._serial.read()
        if len(b):
            self._framer.hal_rx(b)
        self._framer.process()


def run():
    logging.basicConfig(level=logging.INFO)
    args = get_parser().parse_args()
    quit = False
    framer = MasterFramer(port=args.port, baudrate=args.baudrate)
    framer.open()

    def do_quit(*args, **kwargs):
        nonlocal quit
        print('quit')
        quit = True

    signal.signal(signal.SIGINT, do_quit)

    print('start')
    if args.ping > 0:
        framer.start_ping(args.ping)
    while not quit:
        framer.process()
    framer.close()
    print('stop')

    return 0


if __name__ == "__main__":
    sys.exit(run())
