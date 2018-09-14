/*
 * Copyright 2014-2017 Jetperch LLC
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

#include "embc/stream/async_source.h"
#include "embc.h"

static void send(struct embc_stream_producer_s * self,
                 struct embc_stream_transaction_s * transaction) {
    EMBC_DBC_NOT_NULL(self);
    struct embc_stream_source_s * s = EMBC_CONTAINER_OF(self, struct embc_stream_source_s, producer);
    switch (transaction->type) {
        case EMBC_STREAM_IOCTL_OPEN:
            break;
        case EMBC_STREAM_IOCTL_CLOSE:
            if (s->done_fn) {
                s->done_fn(s->done_user_data, transaction->status);
            }
            break;
        case EMBC_STREAM_IOCTL_WRITE:
            s->rx_offset += transaction->length;
            if (s->rx_offset >= s->source_length) {
                EMBC_STRUCT_PTR_INIT(transaction);
                transaction->type = EMBC_STREAM_IOCTL_CLOSE;
                s->consumer->send(s->consumer, transaction);
            }
            break;
        case EMBC_STREAM_EVENT_WRITE_REQUEST: {
            if (s->tx_offset >= s->source_length) {
                break;
            }
            uint16_t sz = transaction->length;
            uint32_t remaining = s->source_length - s->tx_offset;
            uint8_t * buffer;
            if (!sz) {
                if (s->transaction_length) {
                    sz = s->transaction_length;
                    buffer = s->transaction_buffer;
                } else {  // use transaction buffer ptr as the buffer!
                    sz = sizeof(s->transaction_buffer);
                    buffer = (uint8_t *) &s->transaction_buffer;
                }
            } else {
                buffer = transaction->data.event_write_request_consumer_buffer.ptr;
            }
            if (remaining < sz) {
                sz = remaining;
            }
            transaction->type = EMBC_STREAM_IOCTL_WRITE;
            embc_memcpy(buffer, s->source_buffer + s->tx_offset, sz);
            transaction->data.ioctl_write.ptr = buffer;
            transaction->length = sz;
            s->tx_offset += sz;
            s->consumer->send(s->consumer, transaction);
            break;
        }
        case EMBC_STREAM_EVENT_ABORT:
            transaction->type = EMBC_STREAM_IOCTL_CLOSE;
            transaction->status = EMBC_ERROR_ABORTED;
            s->consumer->send(s->consumer, transaction);
            break;
        default:
            break;
    }
}

void embc_stream_source_initialize(
        struct embc_stream_source_s * self,
        uint8_t * transaction_buffer,
        uint16_t transaction_length) {
    EMBC_DBC_NOT_NULL(self);
    EMBC_STRUCT_PTR_INIT(self);
    self->transaction_buffer = transaction_buffer;
    self->transaction_length = transaction_length;
    self->producer.send = send;
}

void embc_stream_source_set_consumer(
        struct embc_stream_source_s * self,
        struct embc_stream_consumer_s * consumer) {
    EMBC_DBC_NOT_NULL(self);
    self->consumer = consumer;
}

struct embc_stream_producer_s * embc_stream_source_get_producer(
        struct embc_stream_source_s * self) {
    EMBC_DBC_NOT_NULL(self);
    return &self->producer;
}

void embc_stream_source_configure(
        struct embc_stream_source_s * self,
        uint8_t const * buffer,
        uint32_t length,
        void (*done_fn)(void *, uint8_t),
        void * done_user_data) {
    EMBC_DBC_NOT_NULL(self);
    EMBC_DBC_NOT_NULL(buffer);
    EMBC_DBC_GT_ZERO(length);
    self->source_buffer = buffer;
    self->source_length = length;
    self->done_fn = done_fn;
    self->done_user_data = done_user_data;
    self->tx_offset = 0;
    self->rx_offset = 0;
}

void embc_stream_source_open(
        struct embc_stream_source_s * self) {
    embc_stream_source_open_ex(self, 0, 0);
}

EMBC_API void embc_stream_source_open_ex(
        struct embc_stream_source_s * self,
        void * ptr,
        uint16_t length) {
    self->tx_offset = 0;
    self->rx_offset = 0;
    struct embc_stream_transaction_s transaction;
    EMBC_STRUCT_INIT(transaction);
    transaction.type = EMBC_STREAM_IOCTL_OPEN;
    transaction.length = length;
    transaction.data.ptr = ptr;
    self->consumer->send(self->consumer, &transaction);
}
