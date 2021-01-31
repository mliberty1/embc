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

import weakref
from collections.abc import Mapping


class _Topic:

    def __init__(self, parent, topic, value=None):
        """Hold a single Topic entry for :class:`PubSub`.

        :param parent: The parent :class:`Topic` instance.
        :param topic: The topic string.
        :param value: The optional initial value for the topic.
        """
        self._topic = topic
        self._value = value
        if parent is not None:
            parent = weakref.ref(parent)
        self.parent = parent
        self.children: Mapping[str, _Topic] = {}
        self._subscribers = []

    def __str__(self):
        return f'Topic({self._topic}, value={self._value})'

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, x):
        self.publish(x)

    def publish(self, x, src_cbk=None):
        self._value = x
        topic = self
        while topic is not None:
            for subscriber in topic._subscribers:
                if subscriber == src_cbk:
                    continue
                subscriber(self._topic, self._value)
            topic = topic.parent
            if topic is not None:
                topic = topic()  # weakref to parent

    def _publish_new_subscriber(self, cbk):
        for child in self.children.values():
            child._publish_new_subscriber(cbk)
        if self._value is not None:
            cbk(self._topic, self._value)

    def subscribe(self, cbk):
        if not callable(cbk):
            raise ValueError('subscribers must be callable')
        if cbk not in self._subscribers:
            self._subscribers.append(cbk)
        self._publish_new_subscriber(cbk)

    def unsubscribe(self, cbk):
        self._subscribers.remove(cbk)


class PubSub:

    def __init__(self):
        """A trivial, local publish/subscribe implementation.

        This implementation uses retained values for EVERYTHING.
        Subscribing to a topic will automatically publish the
        retained values to the new subscriber.
        """
        self._root = _Topic(None, '')

    def _topic_find(self, topic, create=False):
        t = self._root
        if len(topic):
            parts_so_far = []
            parts = topic.split('/')
            for part in parts:
                parts_so_far.append(part)
                child = t.children.get(part)
                if child is None:
                    if create:
                        subtopic = '/'.join(parts_so_far)
                        child = _Topic(t, subtopic)
                        t.children[part] = child
                    else:
                        return None
                t = child
        return t

    def publish(self, topic, value, src_cbk=None):
        """Publish to a topic.

        :param topic: The topic name.
        :param value: The value for the topic.
        :param src_cbk: The subscriber that will not be updated.
        """
        t = self._topic_find(topic, create=True)
        return t.publish(value, src_cbk)

    def get(self, topic):
        """Get the retained value for the topic.

        :param topic: The topic name.
        :return: The topic retained value.
        :raise KeyError: If topic does not exist.
        """
        t = self._topic_find(topic, create=False)
        if t is None:
            raise KeyError(f'topic {topic} does not exist')
        return t.value

    def subscribe(self, topic, cbk):
        """Subscribe to a topic and its children.

        :param topic: The topic name.
        :param cbk: The callable(topic, value) called on value changes.
        """
        t = self._topic_find(topic, create=True)
        t.subscribe(cbk)

    def unsubscribe(self, topic, cbk):
        """Unsubscribe from a topic.

        :param topic: The topic name.
        :param cbk: The callable provided to subscribe.
        """
        t = self._topic_find(topic, create=True)
        t.unsubscribe(cbk)
