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
from pyembc.stream.framer import Framer


log = logging.getLogger(__name__)


class StreamInterface:

    def __init__(self):
        self.recv_cbk = None
        self.send_done_cbk = None
        self.send_queue = []

    def open(self, recv_cbk, send_done_cbk):
        self.recv_cbk = recv_cbk
        self.send_done_cbk = send_done_cbk

    def close(self):
        self.recv_cbk = None
        self.send_done_cbk = None

    def send(self, buffer):
        self.send_queue.append(buffer)

    def on_send_done(self, buffer):
        if callable(self.send_done_cbk):
            self.send_done_cbk(buffer)

    def on_recv(self, buffer):
        if callable(self.recv_cbk):
            self.recv_cbk(buffer)


class StreamTester:
    """Inject errors into data streams"""

    def __init__(self, seed=None):
        if seed is None:
            self.random = np.random.default_rng()
        else:
            self.random = np.random.Generator(np.random.PCG64(seed))

        self.a = StreamInterface()
        self.b = StreamInterface()
        self.buffer_drop_rate = 0
        self.buffer_insert_rate = 0
        self.byte_drop_rate = 0
        self.byte_insert_rate = 0
        self.bit_error_rate = 0

    def _is_buffer_drop(self):
        return self.random.random() < self.buffer_drop_rate

    def _is_buffer_insert(self):
        return self.random.random() < self.buffer_insert_rate

    def _permute_buffer(self, buffer):
        drop = self.random.random(len(buffer))
        buffer = np.delete(buffer, drop < self.byte_drop_rate)
        insert_bool = self.random.random(len(buffer)) < self.byte_insert_rate
        insert_idx = np.argwhere(insert_bool).flatten()
        if len(insert_idx):
            insert_data = np.array(self.random.bytes(len(insert_idx)))
            insert_data = np.frombuffer(insert_data, dtype=np.uint8)
            buffer = np.insert(buffer, insert_idx, insert_data)
        bit_bool = self.random.random(len(buffer) * 8) < self.bit_error_rate
        bit_errors = np.argwhere(bit_bool).flatten()
        for idx in bit_errors:
            byte = idx // 8
            bit = 1 << (idx - byte * 8)
            buffer[byte] = buffer[byte] ^ bit  # flip the bit
        return buffer

    def process(self):
        while len(self.a.send_queue) or len(self.b.send_queue):
            for src, dst in [(self.a, self.b), (self.b, self.a)]:
                if not len(src.send_queue):
                    continue
                if self._is_buffer_drop():
                    log.info('DROP BUFFER')
                    buffer = src.send_queue.pop(0)
                    src.on_send_done(buffer)
                    continue
                if self._is_buffer_insert():
                    log.info('INSERT BUFFER')
                    count = self.random.integers(1, 256)
                    buffer = np.frombuffer(self.random.bytes(count), dtype=np.uint8).copy()
                    dst.on_recv(buffer)
                buffer = src.send_queue.pop(0)
                src.on_send_done(buffer)
                buffer = self._permute_buffer(buffer)
                dst.on_recv(buffer)
