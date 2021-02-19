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


from pyembc.host.comm import Comm
from .open import OpenDialog
from .device_widget import DeviceWidget
from PySide2 import QtCore, QtGui, QtWidgets
from pyembc import __version__, __url__
from pyembc.stream.pubsub import PubSub
import ctypes
import logging
import sys


log = logging.getLogger(__name__)
STATUS_BAR_TIMEOUT_DEFAULT = 2500
PORT_COUNT = 32


ABOUT = """\
<html>
<head>
</head>
<body>
EMBC Comm UI<br/> 
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


class Device(QtCore.QObject):

    def __init__(self, parent, pubsub, dev, baudrate=None):
        super(Device, self).__init__(parent)
        self._parent = parent
        self._pubsub = pubsub
        self._dev = dev
        self.comm = None
        self.widget = None
        self._prefix = None
        self.baudrate = baudrate

    def __str__(self):
        return f'Device({self._dev})'

    def open(self):
        self.close()
        try:
            self._prefix = 'd/'
            log.info('comm')
            self.comm = Comm(self._dev, self._prefix, self._pubsub.publish, baudrate=self.baudrate)
            log.info('widget')
            self.widget = DeviceWidget(self._parent, self, self._pubsub, self._prefix)
            log.info('go')
        except Exception:
            log.exception('Could not open device')

    def close(self):
        if self.comm is not None:
            self.comm.close()
            self.comm = None
        if self.widget is not None:
            self.widget.close()
            self.widget = None

    def status_refresh(self):
        if self.comm is not None and self.widget is not None:
            status = self.comm.status()
            self.widget.status_update(status)


class MainWindow(QtWidgets.QMainWindow):

    def __init__(self, app):
        self._devices = {}
        self._pubsub = PubSub()
        super(MainWindow, self).__init__()
        self.setObjectName('MainWindow')
        self.setWindowTitle('EMBC Comm')

        self._central_widget = QtWidgets.QWidget(self)
        self._central_widget.setObjectName('central')
        self.setCentralWidget(self._central_widget)

        self._central_layout = QtWidgets.QHBoxLayout(self._central_widget)
        self._central_layout.setSpacing(6)
        self._central_layout.setContentsMargins(11, 11, 11, 11)
        self._central_layout.setObjectName('central_layout')

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
        self._status_update_timer.start()

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
        self._on_file_open()

    def _on_publish(self, topic, value, retain=None, src_cbk=None):
        print(f'publish {topic} => {value}')
        self._pubsub.publish(topic, value, retain=retain, src_cbk=src_cbk)

    def _device_open(self, dev, baudrate):
        self._device_close(dev)
        log.info('_device_open')
        try:
            device = Device(self, self._pubsub, dev, baudrate)
            device.open()
            self._central_layout.addWidget(device.widget)
            self._devices[dev] = device
        except Exception:
            log.exception('Could not open device')

    def _device_close(self, dev):
        log.info('_device_close')
        if dev in self._devices:
            device = self._devices.pop(dev)
            self._central_layout.removeWidget(device.widget)
            try:
                device.close()
            except Exception:
                log.exception('Could not close device')

    def _device_close_all(self):
        devs = list(self._devices.keys())
        for dev in devs:
            self._device_close(dev)

    def _on_file_open(self):
        log.info('_on_file_open')
        params = OpenDialog(self).exec_()
        if params is not None:
            log.info(f'open {params}')
            self._device_open(params['device'], params['baud'])

    def _on_status_update_timer(self):
        for device in self._devices.values():
            device.status_refresh()

    def closeEvent(self, event):
        log.info('closeEvent()')
        self._device_close_all()
        return super(MainWindow, self).closeEvent(event)

    def _on_file_close(self):
        log.info('_on_file_close')
        self._device_close_all()

    def _on_file_exit(self):
        log.info('_on_file_exit')
        self._device_close_all()
        self.close()

    def _on_help_credits(self):
        log.info('_on_help_credits')

    def _on_help_about(self):
        log.info('_on_help_about')
        txt = ABOUT.format(version=__version__,
                           url=__url__)
        QtWidgets.QMessageBox.about(self, 'Delta Link UI', txt)

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
