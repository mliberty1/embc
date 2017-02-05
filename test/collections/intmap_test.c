#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdbool.h>
#include "runtime/collections/intmap.h"
#include "common/log.h"
#include "common/ec.h"
#include "common/cdef.h"
#include <stdio.h>

void app_printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

void dbc_assert(char const *file, unsigned line, const char * msg) {
    (void) file;
    (void) line;
    (void) msg;
}

/* A test case that does nothing and succeeds. */
static void intmap_empty(void **state) {
    (void) state; /* unused */
    size_t value = 42;
    struct intmap_s * h = intmap_new();
    assert_int_equal(0, intmap_length(h));
    assert_int_equal(JETLEX_ERROR_NOT_FOUND, intmap_get(h, 10, (void **) &value));
    assert_int_equal(0, value);
    intmap_free(h);
}

static void intmap_put_get_remove_get(void **state) {
    (void) state; /* unused */
    size_t value;
    struct intmap_s * h = intmap_new();
    assert_int_equal(0, intmap_put(h, 10, (void *) 20, (void **) &value));
    assert_int_equal(1, intmap_length(h));
    assert_int_equal(0, intmap_get(h, 10, (void **) &value));
    assert_int_equal(20, value);
    assert_int_equal(0, intmap_remove(h, 10, (void **) &value));
    assert_int_equal(0, intmap_length(h));
    assert_int_equal(JETLEX_ERROR_NOT_FOUND, intmap_get(h, 10, 0));
    intmap_free(h);
}

static void intmap_resize(void **state) {
    (void) state; /* unused */
    size_t value_in;
    size_t value_out;
    struct intmap_s * h = intmap_new();
    for (size_t idx = 0; idx < 0x100; ++idx) {
        value_in = idx + 0x1000;
        assert_int_equal(0, intmap_put(h, idx, (void *) value_in, 0));
        assert_int_equal(idx + 1, intmap_length(h));
        assert_int_equal(0, intmap_get(h, idx, (void **) &value_out));
        assert_int_equal(value_in, value_out);
    }
    for (size_t idx = 0; idx < 0x100; ++idx) {
        assert_int_equal(0, intmap_get(h, idx, (void **) &value_out));
        assert_int_equal(idx + 0x1000, value_out);
    }
    intmap_free(h);
}

static void intmap_iterator(void **state) {
    (void) state; /* unused */
    size_t key;
    size_t value;
    struct intmap_iterator_s * iter = 0;
    struct intmap_s * h = intmap_new();
    assert_int_equal(0, intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, intmap_put(h, 3, (void *) 3, 0));
    iter = intmap_iterator_new(h);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    intmap_iterator_free(iter);
    intmap_free(h);
}

static void intmap_iterator_remove_current(void **state) {
    (void) state; /* unused */
    size_t key;
    size_t value;
    struct intmap_iterator_s * iter = 0;
    struct intmap_s * h = intmap_new();
    assert_int_equal(0, intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, intmap_put(h, 3, (void *) 3, 0));
    iter = intmap_iterator_new(h);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, intmap_remove(h, 1, (void **) &value));
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    intmap_iterator_free(iter);
    intmap_free(h);
}

static void intmap_iterator_remove_next(void **state) {
    (void) state; /* unused */
    size_t key;
    size_t value;
    struct intmap_iterator_s * iter = 0;
    struct intmap_s * h = intmap_new();
    assert_int_equal(0, intmap_put(h, 1, (void *) 1, 0));
    assert_int_equal(0, intmap_put(h, 0x100001, (void *) 2, 0));
    assert_int_equal(0, intmap_put(h, 3, (void *) 3, 0));
    iter = intmap_iterator_new(h);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(1, value);
    assert_int_equal(0, intmap_remove(h, 0x100001, (void **) &value));
    assert_int_equal(2, value);
    assert_int_equal(0, intmap_iterator_next(iter, &key, (void **) &value));
    assert_int_equal(3, value);
    intmap_iterator_free(iter);
    intmap_free(h);
}

INTMAP_DECLARE(mysym, size_t)
INTMAP_DEFINE_STRUCT(mysym)
INTMAP_DEFINE(mysym, size_t)

static void mysym(void **state) {
    (void) state; /* unused */
    size_t key;
    size_t value;
    struct mysym_iterator_s * iter = 0;
    struct mysym_s * h = mysym_new();
    assert_int_equal(0, mysym_put(h, 1, 1, 0));
    assert_int_equal(0, mysym_put(h, 0x100001, 2, 0));
    assert_int_equal(0, mysym_put(h, 3, 3, 0));
    assert_int_equal(3, mysym_length(h));
    iter = mysym_iterator_new(h);
    assert_int_equal(0, mysym_iterator_next(iter, &key, &value));
    assert_int_equal(1, value);
    assert_int_equal(0, mysym_remove(h, 0x100001, &value));
    assert_int_equal(2, value);
    assert_int_equal(0, mysym_iterator_next(iter, &key, &value));
    assert_int_equal(3, value);
    mysym_iterator_free(iter);
    mysym_free(h);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(intmap_empty),
        cmocka_unit_test(intmap_put_get_remove_get),
        cmocka_unit_test(intmap_resize),
        cmocka_unit_test(intmap_iterator),
        cmocka_unit_test(intmap_iterator_remove_current),
        cmocka_unit_test(intmap_iterator_remove_next),
        cmocka_unit_test(mysym),
    };

    log_initialize(app_printf);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
