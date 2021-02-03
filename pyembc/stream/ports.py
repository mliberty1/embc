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


from .pubsub_port import PubSubPort


PORTS = {
    # meta['type'] to PortApi implementation
    'pubsub': PubSubPort,
}


class PortApi:

    def __init__(self, pubsub, transport, port_id):
        pass

    def on_event(self, event):
        raise NotImplementedError()

    def on_recv(self, port_data, msg):
        raise NotImplementedError()
