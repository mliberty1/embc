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
    struct embc_stream_factory_s factory;
    struct embc_stream_handle_s handle;
};

int open(struct embc_stream_factory_s * self,
         void const * meta,
         struct embc_stream_handle_s ** handle) {
    struct test_s * s = (struct test_s *) self;
    *handle = &s->handle;
    check_expected_ptr(meta);
    return mock_type(int);
}

void write(struct embc_stream_handle_s * self,
           uint8_t const * buffer, uint32_t length) {
    (void) self;
    check_expected(length);
    check_expected_ptr(buffer);
}

int ioctl(struct embc_stream_handle_s * self,
          struct embc_stream_ioctl_s * transaction) {
    (void) self;
    (void) transaction;
    return mock_type(int);
}

void close(struct embc_stream_handle_s * self, uint8_t status) {
    (void) self;
    check_expected(status);
}

static int setup(void ** state) {
    struct test_s *self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->factory.open = open;
    self->handle.write = write;
    self->handle.ioctl = ioctl;
    self->handle.close = close;
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
    struct embc_stream_handle_s * handle = 0;
    const uint8_t HELLO[] = "hello";

    expect_any(open, meta);
    will_return(open, 0);
    assert_int_equal(0, self->factory.open(&self->factory, 0, &handle));
    assert_non_null(handle);

    expect_value(write, length, sizeof(HELLO));
    expect_memory(write, buffer, HELLO, sizeof(HELLO));
    handle->write(handle, HELLO, sizeof(HELLO));

    expect_value(close, status, 0);
    handle->close(handle, 0);
}


int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(normal_use, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
