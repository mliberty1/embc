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

#include "hal_test_impl.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "embc/log.h"
#include "embc/assert.h"
#include <stdlib.h>
#include <string.h> // memset
#include <stdarg.h>
#include <stdio.h>


static void * hal_alloc(embc_size_t size_bytes) {
    void * ptr =  test_malloc((size_t) size_bytes);
    // printf("hal_alloc %p\n", ptr);
    return ptr;
}

static void hal_free(void * ptr) {
    // printf("hal_free %p\n", ptr);
    test_free(ptr);
}

void app_log_printf_(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

void hal_test_initialize() {
    embc_allocator_set(hal_alloc, hal_free);
    embc_log_initialize(app_log_printf_);
}

void embc_fatal(char const * file, int line, char const * msg) {
    (void) file;
    (void) line;
    (void) msg;
    mock_assert(0, msg, file, line);
}
