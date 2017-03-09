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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "embc/stream/async_source.h"
#include "embc.h"


static const uint8_t const HELLO_WORLD[] = "hello world!";

struct test_s {
    struct embc_stream_consumer_s consumer;
    struct embc_stream_producer_s * producer;
    struct embc_stream_source_s source;
    uint8_t buffer[256];
    uint8_t offset;
    uint8_t is_open;
};

static void consumer_send(struct embc_stream_consumer_s * self,
                          struct embc_stream_transaction_s * transaction) {
    struct test_s * s = EMBC_CONTAINER_OF(self, struct test_s, consumer);
    switch (transaction->type) {
        case EMBC_STREAM_IOCTL_OPEN:
            s->offset = 0;
            s->is_open = 1;
            s->producer->send(s->producer, transaction);
            break;
        case EMBC_STREAM_IOCTL_WRITE:
            memcpy(s->buffer + s->offset, transaction->data.ptr, transaction->length);
            s->offset += transaction->length;
            s->producer->send(s->producer, transaction);
            break;
        case EMBC_STREAM_IOCTL_CLOSE:
            s->is_open = 0;
            break;
        default:
            break;
    }
}

static int setup(void ** state) {
    struct test_s *self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->consumer.send = consumer_send;
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void connect(struct test_s * self) {
    embc_stream_source_set_consumer(&self->source, &self->consumer);
    self->producer = embc_stream_source_get_producer(&self->source);
}

static void send_hello_world_consumer_buffer(struct test_s * self) {
    uint8_t b[5];
    embc_stream_source_configure(&self->source, HELLO_WORLD, sizeof(HELLO_WORLD), 0, 0);
    embc_stream_source_open(&self->source);
    assert_int_equal(1, self->is_open);
    while (self->is_open) {
        struct embc_stream_transaction_s transaction;
        memset(&transaction, 0, sizeof(transaction));
        transaction.type = EMBC_STREAM_EVENT_WRITE_REQUEST;
        transaction.consumer_transaction_id = 1;
        transaction.length = sizeof(b);
        transaction.data.ptr = b;
        self->producer->send(self->producer, &transaction);
    }
    assert_memory_equal(self->buffer, HELLO_WORLD, sizeof(HELLO_WORLD));
}

static void send_hello_world(struct test_s * self) {
    embc_stream_source_configure(&self->source, HELLO_WORLD, sizeof(HELLO_WORLD), 0, 0);
    embc_stream_source_open(&self->source);
    assert_int_equal(1, self->is_open);
    while (self->is_open) {
        struct embc_stream_transaction_s transaction;
        memset(&transaction, 0, sizeof(transaction));
        transaction.type = EMBC_STREAM_EVENT_WRITE_REQUEST;
        transaction.consumer_transaction_id = 1;
        self->producer->send(self->producer, &transaction);
    }
    assert_memory_equal(self->buffer, HELLO_WORLD, sizeof(HELLO_WORLD));

}

static void write_request_provides_buffer(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_stream_source_initialize(&self->source, 0, 0);
    connect(self);
    send_hello_world_consumer_buffer(self);
}

static void write_request_with_producer_buffer(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t b[5];
    embc_stream_source_initialize(&self->source, b, sizeof(b));
    connect(self);
    send_hello_world(self);
}

static void write_request_with_producer_immediate_buffer(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    embc_stream_source_initialize(&self->source, 0, 0);
    connect(self);
    send_hello_world(self);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(write_request_provides_buffer, setup, teardown),
            cmocka_unit_test_setup_teardown(write_request_with_producer_buffer, setup, teardown),
            cmocka_unit_test_setup_teardown(write_request_with_producer_immediate_buffer, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
