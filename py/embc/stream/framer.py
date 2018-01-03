# Copyright 2017 Jetperch LLC

import os
import ctypes
from ctypes import cdll, Structure, POINTER, pointer, cast, CFUNCTYPE, \
    c_uint64, c_uint32, c_uint16, c_uint8, c_void_p, c_size_t, c_char_p, \
    c_int32
from ctypes.wintypes import DWORD, HANDLE, BOOL, LPVOID, LPWSTR
from embc.lib import dll as _dll
from embc import lib as embc_lib
from ..collections.list import embc_list_s
from ..memory import buffer as embc_buffer
import time


embc_buffer_s = embc_buffer.embc_buffer_s
RX_FN = CFUNCTYPE(None, c_void_p, c_uint8, c_uint8, c_uint16, POINTER(embc_buffer_s))
TX_DONE_FN = CFUNCTYPE(None, c_void_p, c_uint8, c_uint8, c_uint16, c_int32)
MAX_RETRIES = 16


class Port0:
    DISCARD = 2
    PING_REQ = 4
    PING_RSP = 5
    STATUS_REQ = 6
    STATUS_RSP = 7


class embc_framer_status_s(Structure):
    _fields_ = [
        ('version', c_uint32),
        ('rx_count', c_uint32),
        ('rx_data_count', c_uint32),
        ('rx_ack_count', c_uint32),
        ('rx_deduplicate_count', c_uint32),
        ('rx_synchronization_error', c_uint32),
        ('rx_mic_error', c_uint32),
        ('rx_frame_id_error', c_uint32),
        ('tx_count', c_uint32),
        ('tx_retransmit_count', c_uint32),
    ]
    
    def __str__(self):
        fields = []
        for field, _ in self._fields_:
            fields.append('%s=%s' % (field, getattr(self, field)))
        return 'embc_framer_status_s(%s)' % (', '.join(fields))


class embc_framer_port_callbacks_s(Structure):
    _fields_ = [
        ('rx_fn', RX_FN),
        ('rx_user_data', c_void_p),
        ('tx_done_fn', TX_DONE_FN),
        ('tx_done_user_data', c_void_p),
    ]

HAL_TX_FN = CFUNCTYPE(None, c_void_p, POINTER(embc_buffer_s))
TIMER_DONE_FN = CFUNCTYPE(None, c_void_p, c_uint32)
HAL_TIMER_SET_FN = CFUNCTYPE(c_int32, c_void_p, c_uint64, TIMER_DONE_FN, c_void_p, POINTER(c_uint32))
HAL_TIMER_CANCEL_FN = CFUNCTYPE(c_int32, c_void_p, c_uint32)

class embc_framer_hal_callbacks_s(Structure):
    _fields_ = [
        ('tx_fn', HAL_TX_FN),
        ('tx_user_data', c_void_p),
        ('timer_set_fn', HAL_TIMER_SET_FN),
        ('timer_set_user_data', c_void_p),
        ('timer_cancel_fn', HAL_TIMER_CANCEL_FN),
        ('timer_cancel_user_data', c_void_p),
    ]

# embc_size_t embc_framer_instance_size(void);
instance_size = _dll.embc_framer_instance_size
instance_size.restype = c_size_t
instance_size.argtypes = []
    
# void embc_framer_initialize(
#         struct embc_framer_s * self,
#         struct embc_buffer_allocator_s * buffer_allocator,
#         struct embc_framer_hal_callbacks_s * callbacks);
initialize = _dll.embc_framer_initialize
initialize.restype = c_void_p
initialize.argtypes = [c_void_p, c_void_p, POINTER(embc_framer_hal_callbacks_s)]

# void embc_framer_register_port_callbacks(
#         struct embc_framer_s * self,
#         uint8_t port,
#         struct embc_framer_port_callbacks_s * callbacks);
register_port_callbacks = _dll.embc_framer_register_port_callbacks
register_port_callbacks.restype = None
register_port_callbacks.argtypes = [c_void_p, c_uint8, POINTER(embc_framer_port_callbacks_s)]
        
# void embc_framer_finalize(struct embc_framer_s * self);
finalize = _dll.embc_framer_finalize
finalize.restype = None
finalize.argtypes = [c_void_p]

# void embc_framer_hal_rx_byte(struct embc_framer_s * self, uint8_t byte);
hal_rx_byte = _dll.embc_framer_hal_rx_byte
hal_rx_byte.restype = None
hal_rx_byte.argtypes = [c_void_p, c_uint8]

# void embc_framer_hal_rx_buffer(
#        struct embc_framer_s * self,
#        uint8_t const * buffer, embc_size_t length);
hal_rx_buffer = _dll.embc_framer_hal_rx_buffer
hal_rx_buffer.restype = None
hal_rx_buffer.argtypes = [c_void_p, POINTER(c_uint8), c_size_t]

# void embc_framer_hal_tx_done(
#        struct embc_framer_s * self,
#        struct embc_buffer_s * buffer);
hal_tx_done = _dll.embc_framer_hal_tx_done
hal_tx_done.restype = None
hal_tx_done.argtypes = [c_void_p, POINTER(embc_buffer_s)]

# void embc_framer_send(
#        struct embc_framer_s * self,
#        uint8_t port, uint8_t message_id, uint16_t port_def,
#        struct embc_buffer_s * buffer);
send = _dll.embc_framer_send
send.restype = None
send.argtypes = [c_void_p, c_uint8, c_uint8, c_uint16, POINTER(embc_buffer_s)]

# void embc_framer_send_payload(
#        struct embc_framer_s * self,
#        uint8_t port, uint8_t message_id, uint16_t port_def,
#        uint8_t const * data, uint8_t length);
send_payload = _dll.embc_framer_send_payload
send_payload.restype = None
send_payload.argtypes = [c_void_p, c_uint8, c_uint8, c_uint16, POINTER(c_uint8), c_uint8]

# void embc_framer_resync(struct embc_framer_s * self)
resync = _dll.embc_framer_resync
resync.restype = None
resync.argtypes = [c_void_p]

# struct embc_buffer_s * embc_framer_alloc(
#        struct embc_framer_s * self);
alloc = _dll.embc_framer_alloc
alloc.restype = POINTER(embc_buffer_s)
alloc.argtypes = [c_void_p]

# struct embc_framer_status_s embc_framer_status_get(
#        struct embc_framer_s * self);
status_get = _dll.embc_framer_status_get
status_get.restype = embc_framer_status_s
status_get.argtypes = [c_void_p]


def _timer_cbk_default(user_data, timer_id):
    pass


timer_cbk_default = TIMER_DONE_FN(_timer_cbk_default)


class Framer:

    def __init__(self):
        sizes = (c_size_t * 5)(8, 8, 8, 8, 8)
        self.allocator = embc_buffer.initialize(sizes, len(sizes))
        self._timer_cbk = [timer_cbk_default, None]

        self.timeout = None
        self.hal_tx = lambda x: None
        self._pyport = [None, None] * 256

        # define HAL callbacks
        self.__hal_tx = HAL_TX_FN(self._hal_tx)
        self.__hal_timer_set = HAL_TIMER_SET_FN(self._hal_timer_set)
        self.__hal_timer_cancel = HAL_TIMER_CANCEL_FN(self._hal_timer_cancel)
        self._hal = embc_framer_hal_callbacks_s()
        self._hal.tx_fn = self.__hal_tx
        self._hal.timer_set_fn = self.__hal_timer_set
        self._hal.timer_cancel_fn = self.__hal_timer_cancel

        # define port callbacks
        self.__port_rx = RX_FN(self._port_rx)
        self.__port_tx_done = TX_DONE_FN(self._port_tx_done)
        self._cport = embc_framer_port_callbacks_s()
        self._cport.rx_fn = self.__port_rx
        self._cport.tx_done_fn = self.__port_tx_done

        sz = instance_size()
        self.framer = embc_lib.alloc(sz)
        initialize(self.framer, self.allocator, pointer(self._hal))
        for i in range(0, 16):
            register_port_callbacks(self.framer, i, pointer(self._cport))

    def __del__(self):
        finalize(self.framer)
        embc_lib.free(self.framer)
        embc_buffer.finalize(self.allocator)

    @property
    def status(self):
        return status_get(self.framer)

    def hal_rx(self, buffer_bytes):
        payload_len = len(buffer_bytes)
        msg_char = ctypes.create_string_buffer(buffer_bytes)
        msg = cast(msg_char, POINTER(c_uint8))
        hal_rx_buffer(self.framer, msg, payload_len)

    def _hal_tx(self, user_data, buffer):
        self.hal_tx(buffer[0].read_all())
        hal_tx_done(self.framer, buffer)

    def _hal_timer_set(self, user_data, duration, cbk_fn, cbk_user_data, timer_id):
        dt = duration * (2 ** -30)
        self.timeout = time.time() + dt
        if timer_id is not None:
            timer_id[0] = 1
        self._timer_cbk = [cbk_fn, cbk_user_data]
        return 0

    def _hal_timer_cancel(self, user_data, timer_id):
        self.timeout = None
        self._timer_cbk = [timer_cbk_default, None]
        return 0

    def _port_rx(self, user_data, port, message_id, port_def, buffer):
        rx, _ = self._pyport[port]
        data = buffer[0].read_remaining()
        buffer[0].free()
        rx(port, message_id, port_def, data)

    def _port_tx_done(self, user_data, port, message_id, port_def, status):
        _, tx_done = self._pyport[port]
        tx_done(port, message_id, port_def, status)

    def register_port(self, port, rx, tx_done):
        """Register callbacks for a port

        :param port: The port number from 1 to EMBC_FRAMER_PORTS.
        :param rx: The callable(port, message_id, port_def, data)
        :param tx_done: The callable(port, message_id, port_def, status).
        """
        port = int(port)
        assert(0 <= port < 256)
        self._pyport[port] = (rx, tx_done)

    def send(self, port, message_id, port_def, payload):
        payload_len = len(payload)
        msg_char = ctypes.create_string_buffer(payload)
        msg = cast(msg_char, POINTER(c_uint8))
        send_payload(self.framer, port, message_id, port_def, msg, payload_len)

    def resync(self):
        resync(self.framer)

    def process(self):
        if self.timeout is None:
            return
        if time.time() > self.timeout:
            cbk_fn, cbk_user_data = self._timer_cbk
            self.timeout = None
            self._timer_cbk = [timer_cbk_default, None]
            cbk_fn(cbk_user_data, 1)
