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
import sys


log = logging.getLogger(__name__)
STATUS_BAR_TIMEOUT_DEFAULT = 2500


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

        self._status_widget = QtWidgets.QWidget(self)
        self._status_widget.setObjectName('status')
        self._status_widget.setGeometry(QtCore.QRect(0, 0, 294, 401))
        self._central_layout.addWidget(self._status_widget)

        self._status_layout = QtWidgets.QGridLayout(self._status_widget)
        self._status_layout.setSpacing(6)
        self._status_layout.setContentsMargins(11, 11, 11, 11)
        self._status_layout.setObjectName('status_layout')

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
        self._status_prev = None
        self._status_items = {}

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

    def _on_device_recv(self, metadata, msg):
        port = metadata & 0x1f
        seq = (metadata >> 6) & 0x03
        port_data = (metadata >> 8) & 0xffff
        if 1 == port:
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
            self._device = UartDataLink(dev, self._on_device_event, self._on_device_recv, baud)
            name = dev.split('/')[-1]
            self._status_indicator.setText(name)
            self._status_update_timer.start()
        except:
            log.warning('Could not open device')

    def _device_close(self):
        log.info('_device_close')
        self._status_update_timer.stop()
        if self._device is not None:
            device, self._device = self._device, None
            try:
                device.close()
            except:
                log.exception('Could not close device')
        self._status_clear()
        self._status_prev = None
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

    def _status_clear(self):
        while not self._status_layout.isEmpty():
            item = self._status_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._status_items.clear()

    def _statistic_get(self, name):
        try:
            return self._status_items[name]
        except KeyError:
            row = self._status_layout.rowCount()
            n = QtWidgets.QLabel(self)
            n.setText(name)
            v = QtWidgets.QLabel(self)
            self._status_layout.addWidget(n, row, 0)
            self._status_layout.addWidget(v, row, 1)
            value = (n, v)
            self._status_items[name] = value
            return value

    def _statistic_update(self, name, value):
        _, v = self._statistic_get(name)
        v.setText(str(value))

    def _on_status_update_timer(self):
        if self._device is None:
            return
        status = self._device.status()
        for top_key, top_obj in status.items():
            if isinstance(top_obj, dict):
                for key, obj in top_obj.items():
                    self._statistic_update(f'{top_key}.{key}', obj)
            else:
                self._statistic_update(top_key, top_obj)
        status_json = json.dumps(status, separators=(',', ':'))
        print(f'{len(status_json)} : {status_json}')
        if self._status_prev is not None:
            rx_bytes = status['rx']['msg_bytes'] - self._status_prev['rx']['msg_bytes']
            tx_bytes = status['tx']['msg_bytes'] - self._status_prev['tx']['msg_bytes']
            self._statistic_update('Δrx.msg_bytes', rx_bytes)
            self._statistic_update('Δtx.msg_bytes', tx_bytes)
        self._status_prev = status

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
