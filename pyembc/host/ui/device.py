# Copyright 2021 Jetperch LLC
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


from pyembc.host.uart_data_link import UartDataLink
from pyembc.stream.data_link import PORTS_COUNT
from pyembc.stream.transport import Transport
from pyembc.stream.pubsub import PubSub
from pyembc.stream.port0_server import Port0Server
from pyembc.stream.pubsub_port import PubSubPort
import logging


log = logging.getLogger(__name__)


class Device:

    def __init__(self):
        self._udl = None
        self._transport = None
        self._pubsub = PubSub()
        self._pubsub.subscribe('/h/port/0/meta', self._on_port_meta)

    @property
    def pubsub(self):
        return self._pubsub

    def _on_event(self, event):
        log.info('_on_event(%s)', event)
        if self._transport is not None:
            self._transport.on_event(event)

    def _on_recv(self, metadata, msg):
        if self._transport is not None:
            self._transport.on_recv(metadata, msg)

    def _on_port_meta(self, topic, value):
        print(f'yes {value}')

    def open(self, dev, baud):
        self.close()
        log.info('device open')
        try:
            self._udl = UartDataLink(dev, self._on_event, self._on_recv, baudrate=baud)
            self._transport = Transport(self._udl.send)
            Port0Server(self._pubsub, self._transport, 0)
            return True
        except Exception:
            log.exception('Could not open device')
            return False

    def close(self):
        log.info('device close')
        if self._udl is not None:
            udl, self._udl = self._udl, None
            try:
                udl.close()
                for idx in range(PORTS_COUNT):
                    self._transport.register_port(idx, None)
                self._transport = None
            except Exception:
                log.exception('Could not close device')

    def status(self):
        if self._udl is not None:
            return self._udl.status()
        else:
            return {}
