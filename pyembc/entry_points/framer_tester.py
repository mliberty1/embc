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

import logging
import numpy as np
import json
import signal
import logging


log = logging.getLogger(__name__)


def parser_config(p):
    """Run the stream framer tester."""
    p.add_argument('--seed',
                   help='The optional random number seed.')
    return on_cmd


class Simulator:

    def __init__(self, seed, **kwargs):
        from pyembc.stream.framer import Framer
        from pyembc.stream.stream_tester import StreamTester

        self.s = StreamTester(seed=seed)
        # todo : remove these
        self.s.buffer_drop_rate = 0
        self.s.buffer_insert_rate = 0.001
        self.s.byte_drop_rate = 0
        self.s.byte_insert_rate = 0
        self.s.bit_error_rate = 0

        for key, value in kwargs:
            if hasattr(self.s, key):
                setattr(self.s, key, value)
            else:
                raise ValueError(key)

        self.a = Framer(self._on_event_a, None, self.s.a)
        self.b = Framer(self._on_event_b, None, self.s.b)

        self.expect_recv_a = {}  # map port_id to [[message_id, msg], ...]
        self.expect_recv_b = {}  # map port_id to [[message_id, msg], ...]

        for port_id in range(1, 32):
            self.expect_recv_a[port_id] = []
            self.a.port_register(port_id, self._on_send_done_a, self._on_recv_a)
            self.expect_recv_b[port_id] = []
            self.b.port_register(port_id, self._on_send_done_b, self._on_recv_b)

    def _on_event_a(self, event):
        print(f'Event callback a: {event}')

    def _on_event_b(self, event):
        print(f'Event callback b: {event}')

    def _on_send_done_a(self, port_id, message_id):
        pass

    def _on_send_done_b(self, port_id, message_id):
        pass

    def _on_recv_a(self, port_id, message_id, msg):
        expect_msg_id, expect_msg = self.expect_recv_a[port_id].pop(0)
        if expect_msg_id != message_id:
            raise RuntimeError(f'message_id mismatch: port_id={port_id}, expect={expect_msg_id}, actual={message_id}')
        if not np.array_equal(expect_msg, msg):
            raise RuntimeError(f'message mismatch:\n    {expect_msg}\n    msg')
        log.info('on_recv_a(port_id=%d, message_id=%d, len=%d)', port_id, message_id, len(msg))

    def _on_recv_b(self, port_id, message_id, msg):
        expect_msg_id, expect_msg = self.expect_recv_b[port_id].pop(0)
        if expect_msg_id != message_id:
            raise RuntimeError(f'message_id mismatch: port_id={port_id}, expect={expect_msg_id}, actual={message_id}')
        if not np.array_equal(expect_msg, msg):
            raise RuntimeError(f'message mismatch:\n    {expect_msg}\n    msg')
        log.info('on_recv_b(port_id=%d, message_id=%d, len=%d)', port_id, message_id, len(msg))

    def run_one(self):
        # todo option, timeout
        r = self.s.random
        paths = [['a->b', self.a, self.expect_recv_b], ['b->a', self.b, self.expect_recv_a]]
        idx = r.integers(0, len(paths))
        descr, src, dst = paths[idx]
        port_id = r.integers(1, 32)
        msg_id = r.integers(0, 256)
        msg_len = r.integers(1, 257)
        msg = np.frombuffer(r.bytes(msg_len), dtype=np.uint8)
        log.info('test %s: payload_len=%s', descr, len(msg))
        dst[port_id].append([msg_id, msg])
        src.send(1, port_id, msg_id, msg)
        self.s.process()

    def close(self):
        print('Simulator: close')
        a_status = json.dumps(self.a.status(), indent=2)
        b_status = json.dumps(self.b.status(), indent=2)
        print(f'\nStatus for a:\n{a_status}')
        print(f'\nStatus for b:\n{b_status}')


def on_cmd(args):
    quit_ = False

    def do_quit(*args, **kwargs):
        nonlocal quit_
        quit_ = 'quit from SIGINT'

    signal.signal(signal.SIGINT, do_quit)
    s = Simulator(seed=args.seed)

    try:
        while not quit_:
            s.run_one()
    except Exception as ex:
        logging.getLogger().exception('While getting statistics')
        print('Data streaming failed')
        return 1
    finally:
        s.close()
