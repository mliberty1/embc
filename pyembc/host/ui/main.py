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


from .open import OpenDialog
from pyembc.host.uart_data_link import UartDataLink
from PySide2 import QtCore, QtGui, QtWidgets
from pyembc import __version__, __url__
import ctypes
import json
import logging
import numpy as np
import sys


log = logging.getLogger(__name__)
STATUS_BAR_TIMEOUT_DEFAULT = 2500
PORT_COUNT = 32


ABOUT = """\
<html>
<head>
</head>
<body>
EMBC Delta Link UI<br/> 
Version {version}<br/>
<a href="{url}">{url}</a>

<pre>
Copyright 2018-2021 Jetperch LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
</pre>
</body>
</html>
"""


def menu_setup(d, parent=None):
    k = {}
    for name, value in d.items():
        name_safe = name.replace('&', '')
        if isinstance(value, dict):
            wroot = QtWidgets.QMenu(parent)
            wroot.setTitle(name)
            parent.addAction(wroot.menuAction())
            w = menu_setup(value, wroot)
            w['__root__'] = wroot
        else:
            w = QtWidgets.QAction(parent)
            w.setText(name)
            if callable(value):
                w.triggered.connect(value)
            parent.addAction(w)
        k[name_safe] = w
    return k


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


class TransportSeq:
    MIDDLE = 0
    STOP = 1
    START = 2
    SINGLE = 3


def transport_pack(port_id, seq, port_data):
    return (port_id & 0x1f) | ((seq & 0x03) << 6) | ((port_data & 0xffff) << 8)


class Port0:
    STATUS = 1
    ECHO = 2
    TIMESYNC = 3
    META = 4
    RAW = 5

    @staticmethod
    def pack_req(op, cmd_meta):
        return (op & 0x07) | 0x00 | ((cmd_meta & 0xff) << 8)

    @staticmethod
    def pack_rsp(op, cmd_meta):
        return (op & 0x07) | 0x08 | ((cmd_meta & 0xff) << 8)

    @staticmethod
    def parse(port_data):
        op = port_data & 0x07
        rsp = (port_data & 0x08) != 0
        cmd_data = (port_data >> 8) & 0xff
        return op, rsp, cmd_data


class EchoWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        self.send_fn = None
        self._length = 256  # must be multiple of 8
        self._tx_frame_id = 0
        self._rx_frame_id = 0
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

    def _on_button_toggled(self, checked):
        log.info('echo button  %s', checked)
        txt = 'Press to stop' if checked else 'Press to start'
        self._button.setText(txt)
        self.clear()
        if checked:
            self._send()

    def clear(self):
        self._tx_frame_id = 0
        self._rx_frame_id = 0

    def _send(self):
        if not callable(self.send_fn) or not self._button.isChecked():
            return
        count = int(self._outstanding_combo_box.currentText())
        while (self._tx_frame_id - self._rx_frame_id) < count:
            tx_u8 = np.zeros(self._length, dtype=np.uint8)
            tx_u64 = tx_u8[:8].view(dtype=np.uint64)
            tx_u64[0] = self._tx_frame_id
            self._tx_frame_id += 1
            metadata = transport_pack(0, TransportSeq.SINGLE, Port0.pack_req(Port0.ECHO, 0))
            rv = self.send_fn(metadata, tx_u8)
            if rv:
                log.warning('echo send returned %d', rv)

    def recv(self, msg):
        msg_len = len(msg)
        if msg_len != self._length or msg_len < 8:
            log.warning('unexpected message length %s', len(msg))
            return
        frame_id = msg[:8].view(dtype=np.uint64)[0]
        if frame_id != self._rx_frame_id:
            log.warning('echo frame_id mismatch: %d != %d', frame_id, self._rx_frame_id)
        self._rx_frame_id = frame_id + 1
        self._send()

    def process(self):
        self._send()


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
        for idx in range(PORT_COUNT):
            name_label = QtWidgets.QLabel(self)
            name_label.setObjectName(f'port_name_label_{idx}')
            name_label.setText(f'{idx}')
            self._layout.addWidget(name_label, idx, 1, 1, 1)
            type_label = QtWidgets.QLabel(self)
            type_label.setObjectName(f'port_type_label_{idx}')
            type_label.setText(' ')
            self._layout.addWidget(type_label, idx, 2, 1, 1)
            self._items.append([name_label, type_label])

    def recv(self, port_id, msg):
        if port_id != self._rx_port_id:
            log.warning('unexpected port_id %d != %d', port_id, self._rx_port_id)
        try:
            if len(msg) <= 1:
                s = '-'
            else:
                msg_str = msg[:-1].tobytes().decode('utf-8')
                meta = json.loads(msg_str)
                s = meta.get('type', 'unknown')
        except:
            s = 'invalid'
        self._items[port_id][1].setText(s)
        self._rx_port_id += 1
        self._send()

    def _send(self):
        if not callable(self._send_fn):
            return
        payload = np.zeros(1, dtype=np.uint8)
        while (self._tx_port_id < PORT_COUNT) and (self._tx_port_id - self._rx_port_id) < self._outstanding:
            metadata = transport_pack(0, TransportSeq.SINGLE, Port0.pack_req(Port0.META, self._tx_port_id))
            rv = self._send_fn(metadata, payload)
            if rv:
                log.warning('meta send returned %d for port_id=%d', rv, self._tx_port_id)
            self._tx_port_id += 1
        if self._tx_port_id == PORT_COUNT:
            self._send_fn = None

    def scan(self, send_fn):
        self._tx_port_id = 0
        self._rx_port_id = 0
        for idx in range(PORT_COUNT):
            self._items[idx][1].setText('-')
        if not callable(send_fn):
            log.warning('cannot scan, send_fn not callable')
            self._send_fn = None
            return
        self._send_fn = send_fn
        self._send()


class PubSubWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        super(PubSubWidget, self).__init__(parent)
        self.setObjectName('pubsub_widget')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(5)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('pubsub_widget_layout')

        self._items = {}

    def clear(self):
        while not self._layout.isEmpty():
            item = self._layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._items.clear()


class MainWindow(QtWidgets.QMainWindow):

    def __init__(self, app):
        self._device = None
        super(MainWindow, self).__init__()
        self.setObjectName('MainWindow')
        self.setWindowTitle('EMBC Data Link')

        self._central_widget = QtWidgets.QWidget(self)
        self._central_widget.setObjectName('central')
        self.setCentralWidget(self._central_widget)

        self._central_layout = QtWidgets.QHBoxLayout(self._central_widget)
        self._central_layout.setSpacing(6)
        self._central_layout.setContentsMargins(11, 11, 11, 11)
        self._central_layout.setObjectName('central_layout')

        self._port_widget = PortWidget(self)
        self._central_layout.addWidget(self._port_widget)

        self._status_widget = StatusWidget(self)
        self._central_layout.addWidget(self._status_widget)

        self._echo_widget = EchoWidget(self)
        self._central_layout.addWidget(self._echo_widget)

        # Status bar
        self._status_bar = QtWidgets.QStatusBar(self)
        self.setStatusBar(self._status_bar)
        self._status_indicator = QtWidgets.QLabel(self._status_bar)
        self._status_indicator.setObjectName('status_indicator')
        self._status_bar.addPermanentWidget(self._status_indicator)

        # status update timer
        self._status_update_timer = QtCore.QTimer(self)
        self._status_update_timer.setInterval(1000)  # milliseconds
        self._status_update_timer.timeout.connect(self._on_status_update_timer)

        self._menu_bar = QtWidgets.QMenuBar(self)
        self._menu_items = menu_setup(
            {
                '&File': {
                    '&Open': self._on_file_open,
                    '&Close': self._on_file_close,
                    'E&xit': self._on_file_exit,
                },
                '&Help': {
                    '&Credits': self._on_help_credits,
                    '&About': self._on_help_about,
                }
            },
            self._menu_bar)
        self.setMenuBar(self._menu_bar)
        self.show()
        self._device_close()
        self._on_file_open()

    def _on_device_event(self, event):
        log.info('_on_device_event(%s)', event)
        if self._device is not None and event == 3:
            self._port_widget.scan(self._device.send)

    def _on_port0(self, seq, port_data, msg):
        if seq != TransportSeq.SINGLE:
            log.warning('Port0 only supports seq single')
            return
        op, rsp, cmd_data = Port0.parse(port_data)
        if rsp:  # response
            if op == Port0.ECHO:
                self._echo_widget.recv(msg)
            elif op == Port0.META:
                self._port_widget.recv(cmd_data, msg)
        else:    # request
            pass

    def _on_device_recv(self, metadata, msg):
        port = metadata & 0x1f
        seq = (metadata >> 6) & 0x03
        port_data = (metadata >> 8) & 0xffff
        if 0 == port:
            self._on_port0(seq, port_data, msg)
        elif 1 == port:
            if len(msg) < 3:
                return
            payload_type = (msg[0] >> 6) & 0x03
            topic_len = (msg[0] & 0x1f) + 1
            topic = bytes(msg[1:topic_len]).decode('utf-8')
            log.info(f'topic={topic} payload_type={payload_type}')

    def _device_open(self, dev, baud):
        self._device_close()
        log.info('_device_open')
        try:
            self._device = UartDataLink(dev, self._on_device_event, self._on_device_recv, baudrate=baud)
            name = dev.split('/')[-1]
            self._status_indicator.setText(name)
            self._status_update_timer.start()
            self._echo_widget.send_fn = self._device.send
        except:
            log.warning('Could not open device')

    def _device_close(self):
        log.info('_device_close')
        self._status_update_timer.stop()
        self._echo_widget.send_fn = None
        if self._device is not None:
            device, self._device = self._device, None
            try:
                device.close()
            except:
                log.exception('Could not close device')
        self._status_widget.clear()
        self._status_indicator.setText('Not connected')

    def _on_file_open(self):
        log.info('_on_file_open')
        params = OpenDialog(self).exec_()
        if params is not None:
            log.info(f'open {params}')
            self._device_open(params['device'], params['baud'])

    def closeEvent(self, event):
        log.info('closeEvent()')
        self._device_close()
        return super(MainWindow, self).closeEvent(event)

    def _on_file_close(self):
        log.info('_on_file_close')
        self._device_close()

    def _on_file_exit(self):
        log.info('_on_file_exit')
        self._device_close()
        self.close()

    def _on_help_credits(self):
        log.info('_on_help_credits')

    def _on_help_about(self):
        log.info('_on_help_about')
        txt = ABOUT.format(version=__version__,
                           url=__url__)
        QtWidgets.QMessageBox.about(self, 'Delta Link UI', txt)

    def _on_status_update_timer(self):
        if self._device is None:
            return
        status = self._device.status()
        self._status_widget.update(status)

    @QtCore.Slot(str)
    def status_msg(self, msg, timeout=None, level=None):
        """Display a status message.

        :param msg: The message to display.
        :param timeout: The optional timeout in milliseconds.  0
            does not time out.
        :param level: The logging level for the message.  None (default)
            is equivalent to log.INFO.
        """
        level = logging.INFO if level is None else level
        log.log(level, msg)
        timeout = STATUS_BAR_TIMEOUT_DEFAULT if timeout is None else int(timeout)
        self._status_bar.showMessage(msg, timeout)

    @QtCore.Slot(str)
    def error_msg(self, msg, timeout=None):
        self.status_msg(msg, timeout, level=log.ERROR)


def _high_dpi_enable():
    # http://doc.qt.io/qt-5/highdpi.html
    # https://vicrucann.github.io/tutorials/osg-qt-high-dpi/
    if sys.platform.startswith('win'):
        ctypes.windll.user32.SetProcessDPIAware()
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling)


def run():
    _high_dpi_enable()
    app = QtWidgets.QApplication(sys.argv)
    ui = MainWindow(app)
    rc = app.exec_()
    del ui
    del app
    return rc
