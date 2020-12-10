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

#include "embc/assert.h"
#include "embc/log.h"
#include "embc/lib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static embc_lib_fatal_fn fatal_fn_ = 0;
static void * fatal_user_data_ = 0;
static embc_lib_print_fn print_fn_ = 0;
static void * print_user_data_ = 0;

static void embc_lib_printf_(const char *format, ...);

void embc_lib_initialize() {
    embc_allocator_set((embc_alloc_fn) malloc, (embc_free_fn) free);
}

void embc_lib_fatal_set(embc_lib_fatal_fn fn, void * user_data) {
    fatal_fn_ = fn;
    fatal_user_data_ = user_data;
}

void embc_lib_print_set(embc_lib_print_fn fn, void * user_data) {
    print_fn_ = fn;
    print_user_data_ = user_data;
    embc_log_initialize(embc_lib_printf_);
}

static void embc_lib_printf_(const char *format, ...) {
    char buffer[8192];
    va_list arg;
    va_start(arg, format);
    if (print_fn_) {
        vsnprintf(buffer, sizeof(buffer), format, arg);
        print_fn_(print_user_data_, buffer);
    } else {
        vprintf(format, arg);
    }
    va_end(arg);
}

void embc_fatal(char const * file, int line, char const * msg) {
    if (fatal_fn_) {
        fatal_fn_(fatal_user_data_, file, line, msg);
    } else {
        embc_lib_printf_("FATAL: %s : %d : %s\n", file, line, msg);
    }
}

EMBC_API void * embc_lib_alloc(embc_size_t sz) {
    return embc_alloc_clr(sz);
}

EMBC_API void embc_lib_free(void * ptr) {
    if (ptr) {
        embc_free(ptr);
    }
}
