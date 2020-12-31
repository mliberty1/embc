/*
 * Copyright 2014-2020 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define EMBC_LOG_LEVEL EMBC_LOG_LEVEL_DEBUG1

#include "uart.h"
#include "embc/cdef.h"
#include "embc/ec.h"
#include "embc/log.h"
#include "embc/collections/list.h"
#include "embc/platform.h"
#include <windows.h>


struct buf_s {
    OVERLAPPED overlapped;
    uint32_t size;
    struct embc_list_s item;
    uint8_t buf[];
};

struct uart_s {
    HANDLE handle;
    uart_recv_fn recv_fn;
    void * recv_user_data;
    struct embc_list_s buf_free;
    struct embc_list_s buf_write;
    struct embc_list_s buf_read;
    uint32_t send_remaining;
    uint32_t buffer_size;
    uint32_t recv_buffer_count;
    struct uart_status_s status;
};

// https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
// This functions fills a caller-defined character buffer (pBuffer)
// of max length (cchBufferLength) with the human-readable error message
// for a Win32 error code (dwErrorCode).
//
// Returns TRUE if successful, or FALSE otherwise.
// If successful, pBuffer is guaranteed to be NUL-terminated.
// On failure, the contents of pBuffer are undefined.
BOOL GetErrorMessage(DWORD dwErrorCode, char * pBuffer, DWORD cchBufferLength) {
    char* p = pBuffer;
    if (cchBufferLength == 0) {
        return FALSE;
    }
    pBuffer[0] = 0;

    DWORD cchMsg = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
                                  dwErrorCode,
                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  pBuffer,
                                  cchBufferLength,
                                  NULL);

    while (*p) {
        if ((*p == '\n') || (*p == '\r')) {
            *p = 0;
            break;
        }
        ++p;
    }
    return (cchMsg > 0);
}

#define WINDOWS_LOGE(format, ...) { \
    char error_msg_[64]; \
    DWORD error_ = GetLastError(); \
    GetErrorMessage(error_, error_msg_, sizeof(error_msg_)); \
    EMBC_LOGE(format ": %d: %s", __VA_ARGS__, (int) error_, error_msg_); \
}

static struct buf_s * buf_alloc(struct uart_s * self) {
    struct buf_s * buf;
    if (embc_list_is_empty(&self->buf_free)) {
        buf = embc_alloc_clr(sizeof(struct buf_s) + self->buffer_size);
        EMBC_ASSERT_ALLOC(buf);
        embc_list_initialize(&buf->item);
        buf->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        EMBC_ASSERT_ALLOC(buf->overlapped.hEvent);
    } else {
        struct embc_list_s * item = embc_list_remove_head(&self->buf_free);
        buf = EMBC_CONTAINER_OF(item, struct buf_s, item);
        /*
        buf->overlapped.Internal = 0;
        buf->overlapped.InternalHigh = 0;
        buf->overlapped.Offset = 0;
        buf->overlapped.OffsetHigh = 0;
        */
        ResetEvent(buf->overlapped.hEvent);
    }
    return buf;
}

static void buf_free(struct uart_s * self, struct buf_s * buf) {
    if (buf) {
        embc_list_add_tail(&self->buf_free, &buf->item);
    }
}

struct uart_s * uart_alloc() {
    struct uart_s * self = (struct uart_s *) embc_alloc(sizeof(struct uart_s));
    embc_memset(self, 0, sizeof(self));
    self->handle = INVALID_HANDLE_VALUE;
    embc_list_initialize(&self->buf_free);
    embc_list_initialize(&self->buf_write);
    embc_list_initialize(&self->buf_read);
    return self;
}

void uart_close(struct uart_s * self) {
    if (self->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(self->handle);
        self->handle = INVALID_HANDLE_VALUE;
    }
}

int32_t uart_open(struct uart_s * self, const char *device_path, struct uart_config_s const * config) {
    uart_close(self);
    self->recv_fn = config->recv_fn;
    self->recv_user_data = config->recv_user_data;
    self->buffer_size = config->buffer_size;
    self->send_remaining = config->send_size_total;
    self->recv_buffer_count = config->recv_buffer_count;

    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb
    DCB dcb = {
            .DCBlength = sizeof(DCB),
            .BaudRate = config->baudrate,
            .fBinary = TRUE,
            .fParity = FALSE,
            .fOutxCtsFlow = FALSE,
            .fOutxDsrFlow = FALSE,
            .fDtrControl = DTR_CONTROL_DISABLE,
            .fDsrSensitivity = FALSE,
            .fTXContinueOnXoff = FALSE,
            .fOutX = FALSE,
            .fInX = FALSE,
            .fErrorChar = FALSE,
            .fNull = FALSE,
            .fRtsControl = RTS_CONTROL_DISABLE,
            .fAbortOnError = FALSE,
            .fDummy2 = 0,
            .wReserved = 0,
            .XonLim = 0,
            .XoffLim = 0,
            .ByteSize = 8,
            .Parity = NOPARITY,
            .StopBits = ONESTOPBIT,
            .XonChar = 0,
            .XoffChar = 0,
            .ErrorChar = 0,
            .EofChar = 0,
            .EvtChar = 0,
            .wReserved1 = 0,
    };

    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts
    COMMTIMEOUTS timeouts = {
            .ReadIntervalTimeout = 4,
            .ReadTotalTimeoutMultiplier = 0,
            .ReadTotalTimeoutConstant = 8,
            .WriteTotalTimeoutMultiplier = 0,
            .WriteTotalTimeoutConstant = 100,
    };

    self->handle = CreateFileA(device_path,
                               GENERIC_READ | GENERIC_WRITE,
                               0,       // no file sharing
                               NULL,    // no security attributes
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                               NULL);
    if (self->handle == INVALID_HANDLE_VALUE) {
        return EMBC_ERROR_NOT_FOUND;
    }
    if (!SetCommState(self->handle, &dcb)) {
        uart_close(self);
        return EMBC_ERROR_IO;
    }
    if (!SetCommTimeouts(self->handle, &timeouts)) {
        uart_close(self);
        return EMBC_ERROR_IO;
    }
    if (!FlushFileBuffers(self->handle)) {
        uart_close(self);
        return EMBC_ERROR_IO;
    }
    if (!PurgeComm(self->handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        uart_close(self);
        return EMBC_ERROR_IO;
    }

    DWORD comm_errors = 0;
    ClearCommError(self->handle, &comm_errors, NULL);
    ClearCommBreak(self->handle);

    return 0;
}

int32_t uart_write(struct uart_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    uint32_t sz;
    struct buf_s * buf;
    if (buffer_size > self->send_remaining) {
        EMBC_LOGE("uart_write(%d bytes), but only %d remaining", (int) buffer_size, (int) self->send_remaining);
        return EMBC_ERROR_NOT_ENOUGH_MEMORY;
    }
    while (buffer_size) {
        sz = buffer_size;
        if (sz > self->buffer_size) {
            sz = self->buffer_size;
        }

        uint8_t * b;
        uint32_t b_sz = sz;
        DWORD write_count = 0;

        while (b_sz) {
            buf = buf_alloc(self);
            embc_memcpy(buf->buf, buffer, sz);
            b = buf->buf;
            buf->size = sz;
            buffer_size -= sz;
            buffer += sz;

            if (!WriteFile(self->handle, b, b_sz, &write_count, &buf->overlapped)) {
                break;
            }
            EMBC_LOGW("uart_write overlapped completed immediately");
            b += write_count;
            b_sz -= write_count;
            buf_free(self, buf);
            buf = 0;
        }
        if (!b_sz) {
            // wrote the entire message synchronously, ok.
        } else if (GetLastError() != ERROR_IO_PENDING) {
            WINDOWS_LOGE("%s", "uart_write overlapped failed");
            buf_free(self, buf);
            return EMBC_ERROR_IO;
        } else {
            embc_list_add_tail(&self->buf_write, &buf->item);
            EMBC_LOGD3("write pend: %d", (int) sz);
            self->send_remaining -= sz;
        }
    }

    return 0;
}

uint32_t uart_send_available(struct uart_s *self) {
    return self->send_remaining;
}

static void read_pend(struct uart_s * self) {
    DWORD read_count = 0;
    struct buf_s * buf = buf_alloc(self);
    while (ReadFile(self->handle, buf->buf, self->buffer_size, &read_count, &buf->overlapped)) {
        EMBC_LOGD1("read_pend sync: %d", (int) read_count);
        self->recv_fn(self->recv_user_data, buf->buf, read_count);
        buf_free(self, buf);
        buf = buf_alloc(self);
    }
    DWORD last_error = GetLastError();
    if (last_error != ERROR_IO_PENDING) {
        WINDOWS_LOGE("%s", "ReadFile not pending");
        buf_free(self, buf);
    } else {
        embc_list_add_tail(&self->buf_read, &buf->item);
    }
}

static void process_read(struct uart_s *self) {
    struct embc_list_s * item;
    struct buf_s * buf;
    while (!embc_list_is_empty(&self->buf_read)) {
        DWORD read_count = 0;
        item = embc_list_peek_head(&self->buf_read);
        buf = EMBC_CONTAINER_OF(item, struct buf_s, item);
        BOOL rc = GetOverlappedResult(self->handle, &buf->overlapped, &read_count, FALSE);
        DWORD last_error = rc ? NO_ERROR : GetLastError();
        if (last_error == ERROR_IO_INCOMPLETE) {
            break;  // still in progress
        }
        embc_list_remove_head(&self->buf_read);
        if (rc) {
            if (read_count) {
                self->status.read_bytes += read_count;
                ++self->status.read_buffer_count;
                self->recv_fn(self->recv_user_data, buf->buf, read_count);
            }
        } else if (last_error == ERROR_TIMEOUT) {
            // no worries!
        } else {
            WINDOWS_LOGE("%s", "process_read error");
        }
        buf_free(self, buf);
        read_pend(self);
        EMBC_LOGD3("read %d", (int) read_count);
    }

    for (uint32_t count = embc_list_length(&self->buf_read); count < self->recv_buffer_count; ++count) {
        read_pend(self);
    }
}

static void process_write(struct uart_s *self) {
    struct embc_list_s * item;
    struct buf_s * buf;
    while (!embc_list_is_empty(&self->buf_write)) {
        DWORD bytes = 0;
        item = embc_list_peek_head(&self->buf_write);
        buf = EMBC_CONTAINER_OF(item, struct buf_s, item);
        BOOL rc = GetOverlappedResult(self->handle, &buf->overlapped, &bytes, FALSE);
        DWORD last_error = rc ? NO_ERROR : GetLastError();
        if (last_error == ERROR_IO_INCOMPLETE) {
            break;  // still in progress
        }
        self->send_remaining += buf->size;
        self->status.write_bytes += buf->size;
        ++self->status.write_buffer_count;
        embc_list_remove_head(&self->buf_write);
        buf_free(self, buf);
        EMBC_LOGD3("write complete");
    }
}

void uart_handles(struct uart_s *self, uint32_t * handle_count, void ** handles) {
    DWORD count = 0;
    struct embc_list_s * item;
    struct buf_s * buf;

    item = embc_list_peek_head(&self->buf_write);
    if (item) {
        buf = EMBC_CONTAINER_OF(item, struct buf_s, item);
        handles[count++] = buf->overlapped.hEvent;
    }

    item = embc_list_peek_head(&self->buf_read);
    if (item) {
        buf = EMBC_CONTAINER_OF(item, struct buf_s, item);
        handles[count++] = buf->overlapped.hEvent;
    }
    *handle_count += count;
}

void uart_process(struct uart_s *self) {
    process_read(self);
    process_write(self);
}

uint32_t uart_time_get_ms(struct uart_s *self) {
    (void) self;
    //SYSTEMTIME time;
    //GetSystemTime(&time);
    //return (time.wSecond * 1000) + time.wMilliseconds;

    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    counter.QuadPart *= 1000;
    counter.QuadPart /= frequency.QuadPart;
    return (uint32_t) counter.QuadPart;
}

int32_t uart_status_get(struct uart_s *self, struct uart_status_s * stats) {
    *stats = self->status;
    return 0;
}
