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
#include "embc/stream/async_sink.h"
#include "embc/stream/async_source.h"
#include "embc.h"


static const uint8_t const HELLO_WORLD[] = "hello world!";

struct test_s {
    struct embc_stream_sink_s sink;
    struct embc_stream_source_s source;
};

static int setup(void ** state) {
    struct test_s *self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void connect(struct test_s * self) {
    struct embc_stream_consumer_s * consumer = 0;
    struct embc_stream_producer_s * producer = 0;
    embc_stream_sink_get_consumer(&self->sink, &consumer);
    embc_stream_source_set_consumer(&self->source, consumer);
    embc_stream_source_get_producer(&self->source, &producer);
    embc_stream_sink_set_producer(&self->sink, producer);
}

static void source_done(void * user_data) {
    check_expected_ptr(user_data);
}

static void dest_done(void * user_data, uint8_t * buffer, uint32_t length) {
    (void) user_data;
    check_expected(length);
    check_expected_ptr(buffer);
}

static void consumer_buffer(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t b[5];
    uint8_t buffer[256];
    embc_stream_source_initialize(&self->source, 0, 0);
    embc_stream_sink_initialize(&self->sink, b, sizeof(b));
    connect(self);
    embc_stream_sink_receive(&self->sink, buffer, sizeof(buffer), dest_done, self);
    expect_value(dest_done, length, sizeof(HELLO_WORLD));
    expect_memory(dest_done, buffer, HELLO_WORLD, sizeof(HELLO_WORLD));
    expect_any(source_done, user_data);
    embc_stream_source_configure(&self->source,
                                 HELLO_WORLD, sizeof(HELLO_WORLD),
                                 source_done, self);
    embc_stream_source_open(&self->source);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(consumer_buffer, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
