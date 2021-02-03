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


from PySide2 import QtCore, QtGui, QtWidgets
from pyembc.stream.data_link import PORTS_COUNT
import logging
import weakref


log = logging.getLogger(__name__)


class DeviceWidget(QtWidgets.QWidget):

    def __init__(self, parent, device):
        self.device = device
        super(DeviceWidget, self).__init__(parent)

        self._layout = QtWidgets.QHBoxLayout(self)
        self._layout.setSpacing(6)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('device_layout')

        self._port_widget = PortWidget(self)
        self._layout.addWidget(self._port_widget)
        self.device.pubsub.subscribe('h/port/0/meta', self._port_widget.on_port_meta)

        self._status_widget = StatusWidget(self)
        self._layout.addWidget(self._status_widget)

        self._echo_widget = EchoWidget(self, device.pubsub)
        self._layout.addWidget(self._echo_widget)

        self._pubsub_widget = PubSubWidget(self, device.pubsub)
        self._layout.addWidget(self._pubsub_widget)

    def status_update(self, status):
        self._status_widget.update(status)


class PortWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        self._outstanding = 8
        self._tx_port_id = 0
        self._rx_port_id = 0
        self._send_fn = None
        super(PortWidget, self).__init__(parent)
        self.setObjectName('port_widget')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(5)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('port_widget_layout')

        self._items = []
        for idx in range(PORTS_COUNT):
            name_label = QtWidgets.QLabel(self)
            name_label.setObjectName(f'port_name_label_{idx}')
            name_label.setText(f'{idx}')
            self._layout.addWidget(name_label, idx, 1, 1, 1)
            type_label = QtWidgets.QLabel(self)
            type_label.setObjectName(f'port_type_label_{idx}')
            type_label.setText(' ')
            self._layout.addWidget(type_label, idx, 2, 1, 1)
            self._items.append([name_label, type_label])

    def on_port_meta(self, topic, value):
        for meta, (_, type_label) in zip(value, self._items):
            if meta is None:
                txt = '-'
            else:
                txt = meta.get('type', 'unknown')
            type_label.setText(txt)


class StatusWidget(QtWidgets.QWidget):

    def __init__(self, parent):
        super(StatusWidget, self).__init__(parent)

        self.setObjectName('status')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(6)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('status_layout')

        self._prev = None
        self._items = {}

    def clear(self):
        while not self._layout.isEmpty():
            item = self._layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._items.clear()
        self._prev = None

    def _statistic_get(self, name):
        try:
            return self._items[name]
        except KeyError:
            row = self._layout.rowCount()
            n = QtWidgets.QLabel(self)
            n.setText(name)
            v = QtWidgets.QLabel(self)
            self._layout.addWidget(n, row, 0)
            self._layout.addWidget(v, row, 1)
            value = (n, v)
            self._items[name] = value
            return value

    def _statistic_update(self, name, value):
        _, v = self._statistic_get(name)
        v.setText(str(value))

    def update(self, status):
        for top_key, top_obj in status.items():
            if isinstance(top_obj, dict):
                for key, obj in top_obj.items():
                    self._statistic_update(f'{top_key}.{key}', obj)
            else:
                self._statistic_update(top_key, top_obj)
        if self._prev is not None:
            rx_bytes = status['rx']['msg_bytes'] - self._prev['rx']['msg_bytes']
            tx_bytes = status['tx']['msg_bytes'] - self._prev['tx']['msg_bytes']
            self._statistic_update('Δrx.msg_bytes', rx_bytes)
            self._statistic_update('Δtx.msg_bytes', tx_bytes)
        self._prev = status


class EchoWidget(QtWidgets.QWidget):

    def __init__(self, parent, pubsub):
        self._pubsub = weakref.ref(pubsub)
        super(EchoWidget, self).__init__(parent)

        self.setObjectName('echo')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(6)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('echo_layout')

        self._outstanding_label = QtWidgets.QLabel(self)
        self._outstanding_label.setObjectName('echo_outstanding_label')
        self._outstanding_label.setText('Outstanding frames')
        self._layout.addWidget(self._outstanding_label, 0, 0, 1, 1)

        self._outstanding_combo_box = QtWidgets.QComboBox(self)
        self._outstanding_combo_box.setObjectName('echo_outstanding_combobox')
        for frame in [1, 2, 4, 8, 16, 32]:
            self._outstanding_combo_box.addItem(str(frame))
        self._outstanding_combo_box.setEditable(True)
        self._outstanding_combo_box.setCurrentIndex(3)
        self._outstanding_validator = QtGui.QIntValidator(0, 256, self)
        self._outstanding_combo_box.setValidator(self._outstanding_validator)
        self._layout.addWidget(self._outstanding_combo_box, 0, 1, 1, 1)

        self._button = QtWidgets.QPushButton(self)
        self._button.setCheckable(True)
        self._button.setText('Press to start')
        self._button.toggled.connect(self._on_button_toggled)
        self._layout.addWidget(self._button, 1, 0, 2, 0)

    def _on_echo_enabled(self, topic, value):
        self._button.setChecked(value)

    def _on_echo_outstanding(self, topic, value):
        self._outstanding_combo_box.setCurrentText(str(value))

    def _on_button_toggled(self, checked):
        log.info('echo button  %s', checked)
        txt = 'Press to stop' if checked else 'Press to start'
        self._button.setText(txt)
        pubsub = self._pubsub()
        if pubsub:
            value = 1 if checked else 0
            pubsub.publish('h/port/0/echo/enable', value, retain=True, src_cbk=self._on_echo_enabled)


class PubSubWidget(QtWidgets.QWidget):

    def __init__(self, parent, pubsub):
        self._pubsub = weakref.ref(pubsub)
        super(PubSubWidget, self).__init__(parent)
        self.setObjectName('pubsub_widget')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(5)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('pubsub_widget_layout')

        self._items = {}
        pubsub.subscribe('', self._on_update)

    def clear(self):
        while not self._layout.isEmpty():
            item = self._layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._items.clear()

    def _on_update(self, topic, value):
        print(f'{topic} : {value}')
        if topic == 'h/port/0/meta' and not len(self._items):
            pubsub = self._pubsub()
            if pubsub is not None:
                pubsub.publish('$', 0)
