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
#include "embc/ec.h"


static void enum_check(void **state) {
    (void) state;
    assert_int_equal(0, EMBC_ERROR_SUCCESS);
    assert_int_equal(0, EMBC_SUCCESS);
    assert_int_equal(1, EMBC_ERROR_UNSPECIFIED);
}

static void names(void **state) {
    (void) state;
    assert_string_equal("SUCCESS", embc_error_code_name(0));
    assert_string_equal("UNSPECIFIED", embc_error_code_name(EMBC_ERROR_UNSPECIFIED));
    assert_string_equal("UNKNOWN", embc_error_code_name(-1));
}

static void descriptions(void **state) {
    (void) state;
    assert_string_equal("Success (no error)", embc_error_code_description(0));
    assert_string_equal("Unspecified error", embc_error_code_description(EMBC_ERROR_UNSPECIFIED));
    assert_string_equal("Unknown error", embc_error_code_description(-1));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(enum_check),
            cmocka_unit_test(names),
            cmocka_unit_test(descriptions),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
