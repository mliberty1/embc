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

RX_PING_DEPTH = 3  # default setting
TX_PING_DEPTH = 3  # default setting
PING_TIMEOUT = 5.0 # seconds
PING_DEPTH = 3  # default setting
DELAY_TIME = 1.0


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
        self.message_id_raw = message_id
        self.message_id = message_id & 0xff
        self.payload = payload
        # in case of lost ACK frames, can receive response before tx_done
        self.response_received = False
        self.tx_done_received = False
        self.time = None

    @property
    def is_done(self):
        return self.tx_done_received and self.response_received


class PingFrameGenerator:

    def __init__(self, count):
        self._count_total = count
        self._count_generated = 0
        self.payload_size = 240
        assert(0 == (self.payload_size % 4))
        self._pattern = embc.PatternTx()

    def is_done(self):
        if self._count_total < 0:
            return False
        if self._count_total == 0:
            return True
        return self._count_generated >= self._count_total

    def __iter__(self):
        return self

    def __next__(self):
        if self.is_done():
            raise StopIteration
        return self.next()

    def next(self):
        payload = []
        for k in range(0, self.payload_size, 4):
            word = self._pattern.next_u32()
            word_bytes = [word & 0xff,
                          (word >> 8) & 0xff,
                          (word >> 16) & 0xff,
                          (word >> 24) & 0xff]
            payload.extend(word_bytes)
        payload = bytes(payload)
        frame = PingFrame(self._count_generated, payload)
        self._count_generated += 1
        return frame


class PingQueue:

    def __init__(self, count, send):
        self.count = count
        self._generator = PingFrameGenerator(count)
        self._queue = []
        self._time_start = None
        self._time_last = None
        self._send = send
        self._frames_completed = 0
        self._frames_completed_last = 0
        self._frames_tx_error = 0
        self._frames_missing = 0
        self._frames_timeout = 0
        self._delay = time.time()

    def log_running_stats(self):
        t = time.time()
        dt = t - self._time_last
        if dt > 1.0:
            frames = self._frames_completed - self._frames_completed_last
            length = frames * self._generator.payload_size
            throughput = length / dt
            log.info('%d Bps in %d frames: total errors tx=%d, rx_missing=%d, timeout=%d',
                     throughput, frames,
                     self._frames_tx_error,
                     self._frames_missing,
                     self._frames_timeout)
            self._frames_completed_last = self._frames_completed
            self._time_last = t

    def log_total_stats(self):
        t = time.time()
        dt = t - self._time_start
        frames = self._frames_completed
        length = frames * self._generator.payload_size
        throughput = length / dt
        log.info('%d Bps in %d frames: total errors: tx=%d, rx_missing=%d, timeout=%d',
                 throughput, frames,
                 self._frames_tx_error,
                 self._frames_missing,
                 self._frames_timeout)

    def process(self):
        queue = []
        t = time.time()
        if self._time_start is None:
            self._time_start = t
            self._time_last = t
        for msg in self._queue:
            if msg.tx_done_received and msg.response_received:
                self._frames_completed += 1
            elif t - msg.time >= PING_TIMEOUT:
                log.info('remove frame due to timeout')
                self._frames_timeout += 1
                continue
            else:
                queue.append(msg)
        self._queue = queue

        if time.time() >= self._delay:
            rx_available = RX_PING_DEPTH - self._rx_pending_count()
            tx_available = TX_PING_DEPTH - self._tx_done_pending_count()
            available = min(rx_available, tx_available)
            for i in range(available):
                if self._generator.is_done():
                    break
                else:
                    msg = self._generator.next()
                    msg.time = t
                    self._queue.append(msg)
                    self._send(msg)
        self.log_running_stats()

    def _tx_done_pending_count(self):
        return len([msg for msg in self._queue if not msg.tx_done_received])

    def _rx_pending_count(self):
        return len([msg for msg in self._queue if not msg.response_received])

    def _ping_rx_resync(self, message_id, data):
        for idx in range(len(self._queue)):
            msg = self._queue[idx]
            if msg.message_id == message_id and msg.payload == data:
                log.info('resync message_id=%d to index %d', message_id, idx)
                self._frames_missing += idx
                del self._queue[:idx]
                return msg
        log.warning('rx resync failed message_id=%d (frame from previous ping session?)',
                    message_id)
        return None

    def rx(self, message_id, payload):
        if 0 == self._rx_pending_count(): # error
            log.warning("rx ping response %d, but not expected" % message_id)
            return
        msg = self._queue[0]
        if msg.message_id != message_id:
            log.debug('rx ping response message_id mismatch: expected %d != received %d',
                      msg.message_id, message_id)
            msg = self._ping_rx_resync(message_id, payload)
        elif msg.payload != payload:
            log.warning('rx ping response %d payload mismatch', message_id)
            msg = self._ping_rx_resync(message_id, payload)
        else:
            log.debug('rx ping response %d', message_id)
        if msg is not None:
            msg.response_received = True
            self.process()

    def _get_tx_done_message(self, message_id):
        for idx, msg in enumerate(self._queue):
            if not msg.tx_done_received and msg.message_id == message_id:
                return idx, msg
        return 0, None

    def tx_done(self, message_id, status):
        idx, msg = self._get_tx_done_message(message_id)
        if msg is None:
            log.warning('tx_done ping %d with no matching tx message', message_id)
            return
        msg.tx_done_received = True
        if 0 == status:
            log.debug('tx_done ping %d', message_id)
        else:  # error
            self._frames_tx_error += 1
            self._queue.pop(idx)
            log.info('tx_done ping error %s: message_id=%d, tx_done_pending=%s, rx_pending=%d',
                     embc.ec.num_to_name.get(status, 'unknown'), message_id,
                     self._tx_done_pending_count(),
                     self._rx_pending_count())
            # delay to prevent far-end overflow during loss of 1 direction
            self._delay = time.time() + DELAY_TIME
        self.process()


class MasterFramer:

    def __init__(self, port, baudrate):
        # port=None defers open until explict open().
        self._serial = serial.Serial(port=None, baudrate=baudrate, timeout=0.002)
        self._serial.port = port
        self._last_time = None
        self._framer = embc.stream.framer.Framer()
        self._framer.hal_tx = self._serial.write
        self.message_id = 0
        self._ping = PingQueue(0, self._send_ping)
        self._framer.register_port(0, self.rx_port0, self.tx_done_port0)
        for i in range(1, 16):
            self._framer.register_port(i, self.rx, self.tx_done)

    def _send_ping(self, msg):
        self._framer.send(0, msg.message_id, embc.stream.framer.Port0.PING_REQ, msg.payload)

    def start_ping(self, count):
        self._ping = PingQueue(count, self._send_ping)
        self._ping.process()

    def rx_port0(self, port, message_id, port_def, data):
        if embc.stream.framer.Port0.PING_RSP == port_def:
            self._ping.rx(message_id, data)
        elif embc.stream.framer.Port0.STATUS_RSP == port_def:
            pass
        else:
            log.info('rx_port0 message_id=%d port_def=0x%04x', message_id, port_def)

    def tx_done_port0(self, port, message_id, port_def, status):
        if embc.stream.framer.Port0.PING_REQ == port_def:
            self._ping.tx_done(message_id, status)
        elif embc.stream.framer.Port0.RESYNC == port_def:
            pass
        elif embc.stream.framer.Port0.STATUS_REQ == port_def:
            pass
        else:
            log.info('tx_done_port0 message_id=%d port_def=0x%04x status=%d',
                     message_id, port_def, status)

    def rx(self, port, message_id, port_def, data):
        log.info('%s %s %s %s', port, message_id, port_def, data)

    def tx_done(self, port, message_id, port_def, status):
        if status:
            log.info('tx_done error %s: port=%d, message_id=%d, port_def=0x%04x',
                     embc.ec.num_to_name.get(status, 'unknown'), port, message_id, port_def)

    def open(self):
        self._serial.open()
        self._last_time = time.time()
        #self._framer.resync()

    def close(self):
        self._serial.close()

    def process(self):
        b = self._serial.read()
        if len(b):
            self._framer.hal_rx(b)
        self._framer.process()
        if self._ping.count != 0:
            self._ping.process()
        else:
            t = time.time()
            if t - self._last_time > 1.0:
                self._last_time = t
                print(self._framer.status)


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
    if args.ping != 0:
        framer.start_ping(args.ping)
    while not quit:
        framer.process()
    framer.close()
    print('stop')

    return 0


if __name__ == "__main__":
    sys.exit(run())
