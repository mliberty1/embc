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


/**
 * @file
 *
 * @brief UART abstraction.
 */

#ifndef EMBC_STREAM_UART_H__
#define EMBC_STREAM_UART_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*uart_recv_fn)(void *user_data, uint8_t *buffer, uint32_t buffer_size);

struct uart_config_s {
    uint32_t baudrate;
    uint32_t send_size_total;
    uint32_t buffer_size;
    uint32_t recv_buffer_count;
    uart_recv_fn recv_fn;
    void *recv_user_data;
};

struct uart_status_s {
    uint64_t write_bytes;
    uint64_t write_buffer_count;
    uint64_t read_bytes;
    uint64_t read_buffer_count;
};

struct uart_s;

struct uart_s *uart_alloc();

int32_t uart_open(struct uart_s *self, const char *device_path, struct uart_config_s const * config);

void uart_close(struct uart_s *self);

int32_t uart_write(struct uart_s *self, uint8_t const *buffer, uint32_t buffer_size);

uint32_t uart_send_available(struct uart_s *self);

void uart_process(struct uart_s *self);

void uart_handles(struct uart_s *self, uint32_t * handle_count, void ** handles);

int32_t uart_status_get(struct uart_s *self, struct uart_status_s * stats);

#ifdef __cplusplus
}
#endif

#endif  /* EMBC_STREAM_UART_H__ */
