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
import time


log = logging.getLogger(__name__)


def parser_config(p):
    p.add_argument('--port',
                   help='The comm port device identifier.')
    p.add_argument('--baud',
                   help='The baud rate.')
    """Run the uart data link."""
    return on_cmd


class Host:

    def __init__(self, dev, baudrate):
        from pyembc.host.uart_data_link import UartDataLink
        self.udl = UartDataLink(dev, self._on_recv, self._on_event, baudrate=baudrate)

    def _on_recv(self, metadata, msg):
        pass

    def _on_event(self, event):
        print(f'Event callback: {event}')


def on_cmd(args):
    quit_ = False

    def do_quit(*args, **kwargs):
        nonlocal quit_
        quit_ = 'quit from SIGINT'

    signal.signal(signal.SIGINT, do_quit)
    h = Host(args.port, args.baud)
    status_prev = None

    try:
        time_prev = time.time()
        while not quit_:
            time.sleep(0.001)
            time_now = time.time()
            if time_now - time_prev < 1.0:
                continue
            status = h.udl.status()
            tx_retry = status['tx']['retransmissions']
            rx_resync = status['rx_framer']['resync']
            tx_bytes = status['tx']['msg_bytes']
            rx_bytes = status['rx']['msg_bytes']
            if status_prev is not None:
                tx_bytes -= status_prev['tx']['msg_bytes']
                rx_bytes -= status_prev['rx']['msg_bytes']
            print(f'tx_retry={tx_retry}, rx_resync={rx_resync}, tx_bytes={tx_bytes}, rx_bytes={rx_bytes}')
            status_prev = status
            time_prev += 1.0

    except Exception as ex:
        logging.getLogger().exception('While getting statistics')
        print('Data streaming failed')
        return 1
    finally:
        h.udl.close()
