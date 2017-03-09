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
#include "embc/stream/async.h"


struct test_s {
    struct embc_stream_producer_s producer;
    struct embc_stream_consumer_s consumer;
};

static void consumer_send(struct embc_stream_consumer_s * self,
                          struct embc_stream_transaction_s * transaction) {
    (void) self;
    check_expected_ptr(transaction);
}

static void producer_send(struct embc_stream_producer_s * self,
                          struct embc_stream_transaction_s * transaction) {
    (void) self;
    check_expected_ptr(transaction);
}


static int setup(void ** state) {
    struct test_s *self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->producer.send = producer_send;
    self->consumer.send = consumer_send;
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void normal_use(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct embc_stream_transaction_s transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.type = EMBC_STREAM_IOCTL_NOOP;

    expect_memory(consumer_send, transaction, &transaction, sizeof(transaction));
    self->consumer.send(&self->consumer, &transaction);
    expect_memory(producer_send, transaction, &transaction, sizeof(transaction));
    self->producer.send(&self->producer, &transaction);
}


int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(normal_use, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
