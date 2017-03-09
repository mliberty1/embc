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
#include <string.h> // memset
#include <stddef.h>

static void send(struct embc_stream_producer_s * self,
                 struct embc_stream_transaction_s * transaction) {
    DBC_NOT_NULL(self);
    struct embc_stream_source_s * s = EMBC_CONTAINER_OF(self, struct embc_stream_source_s, producer);
    switch (transaction->type) {
        case EMBC_STREAM_IOCTL_OPEN:
            break;
        case EMBC_STREAM_IOCTL_WRITE:
            s->rx_offset += transaction->length;
            if (s->rx_offset >= s->source_length) {
                memset(transaction, 0, sizeof(*transaction));
                transaction->type = EMBC_STREAM_IOCTL_CLOSE;
                s->consumer->send(s->consumer, transaction);
                if (s->done_fn) {
                    s->done_fn(s->done_user_data);
                }
            }
            break;
        case EMBC_STREAM_EVENT_WRITE_REQUEST: {
            if (s->tx_offset >= s->source_length) {
                break;
            }
            uint16_t sz = transaction->length;
            uint32_t remaining = s->source_length - s->tx_offset;
            if (!sz) {
                if (s->transaction_length) {
                    sz = s->transaction_length;
                    transaction->data.ptr = s->transaction_buffer;
                } else {  // use transaction buffer ptr as the buffer!
                    sz = sizeof(s->transaction_buffer);
                    transaction->data.ptr = (uint8_t *) &s->transaction_buffer;
                }
            }
            if (remaining < sz) {
                sz = remaining;
            }
            transaction->type = EMBC_STREAM_IOCTL_WRITE;
            memcpy(transaction->data.ptr, s->source_buffer + s->tx_offset, sz);
            transaction->length = sz;
            s->tx_offset += sz;
            s->consumer->send(s->consumer, transaction);
            break;
        }
        default:
            break;
    }
}

void embc_stream_source_initialize(
        struct embc_stream_source_s * self,
        uint8_t * transaction_buffer,
        uint16_t transaction_length) {
    DBC_NOT_NULL(self);
    memset(self, 0, sizeof(*self));
    self->transaction_buffer = transaction_buffer;
    self->transaction_length = transaction_length;
    self->producer.send = send;
}

void embc_stream_source_set_consumer(
        struct embc_stream_source_s * self,
        struct embc_stream_consumer_s * consumer) {
    DBC_NOT_NULL(self);
    self->consumer = consumer;
}

void embc_stream_source_get_producer(
        struct embc_stream_source_s * self,
        struct embc_stream_producer_s ** producer) {
    DBC_NOT_NULL(self);
    if (producer) {
        *producer = &self->producer;
    }
}

void embc_stream_source_configure(
        struct embc_stream_source_s * self,
        uint8_t const * buffer,
        uint32_t length,
        void (*done_fn)(void *),
        void * done_user_data) {
    DBC_NOT_NULL(self);
    DBC_NOT_NULL(buffer);
    DBC_GT_ZERO(length);
    self->source_buffer = buffer;
    self->source_length = length;
    self->done_fn = done_fn;
    self->done_user_data = done_user_data;
    self->tx_offset = 0;
    self->rx_offset = 0;
}

void embc_stream_source_open(
        struct embc_stream_source_s * self) {
    self->tx_offset = 0;
    self->rx_offset = 0;
    struct embc_stream_transaction_s transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.type = EMBC_STREAM_IOCTL_OPEN;
    self->consumer->send(self->consumer, &transaction);
}
