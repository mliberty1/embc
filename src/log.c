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

#include "embc/log.h"
#include <stdio.h>


char const * const embc_log_level_str[EMBC_LOG_LEVEL_ALL + 1] = {
        "EMERGENCY",
        "ALERT",
        "CRITICAL",
        "ERROR",
        "WARN",
        "NOTICE"
        "INFO",
        "DEBUG",
        "DEBUG2"
        "DEBUG3",
        "ALL"
};

char const embc_log_level_char[EMBC_LOG_LEVEL_ALL + 1] = {
        '!', 'A', 'C', 'E', 'W', 'N', 'I', 'D', 'D', 'D', '.'
};


void embc_log_printf_default(const char * fmt, ...) {
    (void) fmt;
}

volatile embc_log_printf EMBC_USED embc_log_printf_ = embc_log_printf_default;

int embc_log_initialize(embc_log_printf handler) {
    if (NULL == handler) {
        embc_log_printf_ = embc_log_printf_default;
    } else {
        embc_log_printf_ = handler;
    }
    return 0;
}

void embc_log_finalize() {
    embc_log_initialize(0);
}
