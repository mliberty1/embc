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

#include "embc/stream/async_sink.h"
#include "embc.h"


static void send_write_request(struct embc_stream_sink_s * self) {
    struct embc_stream_transaction_s t;
    EMBC_STRUCT_INIT(t);
    t.type = EMBC_STREAM_EVENT_WRITE_REQUEST;
    t.file_id = 1;
    if (self->transaction_buffer) {
        t.consumer_transaction_id = 1;
        t.length = self->transaction_length;
        t.data.event_write_request_consumer_buffer.ptr =
                self->transaction_buffer;
    } else {
        t.data.event_write_request_producer_buffer.mtu = 16;
        t.data.event_write_request_producer_buffer.transactions_max = 1;
    }
    if (self->producer) {
        self->producer->send(self->producer, &t);
    }
}

static void send_event(struct embc_stream_sink_s * self, uint8_t ev) {
    struct embc_stream_transaction_s t;
    EMBC_STRUCT_INIT(t);
    t.type = ev;
    if (self->producer) {
        self->producer->send(self->producer, &t);
    }
}

static void send(struct embc_stream_consumer_s * self,
                 struct embc_stream_transaction_s * transaction) {
    DBC_NOT_NULL(self);
    struct embc_stream_sink_s * s = EMBC_CONTAINER_OF(self, struct embc_stream_sink_s, consumer);
    switch (transaction->type) {
        case EMBC_STREAM_IOCTL_OPEN: {
            transaction->file_id = 1;
            s->mode = 1;
            s->offset = 0;
            s->producer->send(s->producer, transaction);
            if (s->dst_buffer) {
                send_write_request(s);
            } else {
                send_event(s, EMBC_STREAM_EVENT_DISCONNECT);
            }
            break;
        }
        case EMBC_STREAM_IOCTL_WRITE:
            if (s->write_fn) {
                s->write_fn(s->write_user_data, transaction->data.ioctl_write.ptr, transaction->length);
            }
            if (s->dst_buffer) {
                if (s->dst_length < (s->offset + transaction->length)) {
                    EMBC_LOGW("async_sink buffer overflow");
                } else {
                    embc_memcpy(s->dst_buffer + s->offset,
                           transaction->data.ioctl_write.ptr,
                           transaction->length);
                    s->offset += transaction->length;
                }
            }
            s->producer->send(s->producer, transaction);
            send_write_request(s);
            break;
        case EMBC_STREAM_IOCTL_CLOSE:
            s->mode = 1;
            if (s->done_fn) {
                s->done_fn(s->done_user_data, s->dst_buffer, s->offset);
            }
            if (s->producer) {
                s->producer->send(s->producer, transaction);
            }
            break;
        default:
            break;
    }
}

void embc_stream_sink_initialize(
        struct embc_stream_sink_s * self,
        uint8_t * transaction_buffer,
        uint16_t transaction_length) {
    DBC_NOT_NULL(self);
    EMBC_STRUCT_PTR_INIT(self);
    self->transaction_buffer = transaction_buffer;
    self->transaction_length = transaction_length;
    self->consumer.send = send;
}

struct embc_stream_consumer_s * embc_stream_sink_get_consumer(
        struct embc_stream_sink_s * self) {
    DBC_NOT_NULL(self);
    return &self->consumer;
}

void embc_stream_sink_set_producer(
        struct embc_stream_sink_s * self,
        struct embc_stream_producer_s * producer) {
    DBC_NOT_NULL(self);
    self->producer = producer;
}

void embc_stream_sink_set_write_fn(
        struct embc_stream_sink_s * self,
        embc_stream_sink_data_fn fn,
        void * user_data) {
    DBC_NOT_NULL(self);
    self->write_fn = fn;
    self->write_user_data = user_data;
}

void embc_stream_sink_receive(
        struct embc_stream_sink_s * self,
        uint8_t * dst_buffer,
        uint32_t dst_length,
        embc_stream_sink_data_fn done_fn,
        void * done_user_data) {
    DBC_NOT_NULL(self);
    if (self->dst_buffer && !dst_buffer) {
        send_event(self, EMBC_STREAM_EVENT_DISCONNECT);
    }
    uint8_t send_connect = (!self->dst_buffer && dst_buffer);
    self->dst_buffer = dst_buffer;
    self->dst_length = dst_length;
    self->done_fn = done_fn;
    self->done_user_data = done_user_data;
    self->offset = 0;
    if (send_connect && self->mode) {
        send_event(self, EMBC_STREAM_EVENT_CONNECT);
        send_write_request(self);
    }
}
