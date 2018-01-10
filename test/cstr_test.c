#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "embc/cstr.h"
#include "embc/cdef.h"


static void to_u32_empty(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(1, embc_cstr_to_u32(0, &value));
    assert_int_equal(1, embc_cstr_to_u32("", &value));
}

static void to_u32_zero(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, embc_cstr_to_u32("0", &value));
    assert_int_equal(0, value);
    assert_int_equal(0, embc_cstr_to_u32("  0  ", &value));
    assert_int_equal(0, value);
    assert_int_equal(1, embc_cstr_to_u32("0", 0));
}

static void to_u32_42(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, embc_cstr_to_u32("42", &value));
    assert_int_equal(42, value);
    assert_int_equal(0, embc_cstr_to_u32("  42  ", &value));
    assert_int_equal(42, value);
}

static void to_u32_0_h(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(1, embc_cstr_to_u32(" 0 h", &value));
}

static void to_u32_hex(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, embc_cstr_to_u32("0x12345678", &value));
    assert_int_equal(0x12345678, value);
}

struct i32s_case_s {
    const char * str;
    int32_t exponent;
    int32_t value;
};

static void to_i32s(void **state) {
    (void) state; /* unused */
    const struct i32s_case_s c[] = {
        {"1", 0, 1},
        {"1", 2, 100},
        {"1.01", 2, 101},
        {"   1.01   ", 2, 101},
        {"  +1.01  ", 2, 101},
        {"  -1.01   ", 2, -101},
        {"  1.010101   ", 2, 101},
    };

    for (embc_size_t i = 0; i < EMBC_ARRAY_SIZE(c); ++i) {
        int32_t x = 0;
        assert_int_equal(0, embc_cstr_to_i32s(c[i].str, c[i].exponent, &x));
        assert_int_equal(c[i].value, x);
    }
}

const char MSG1[] = "hello world!";

static void copy_zero_tgt_size(void ** state) {
    (void) state;
    char tgt[] = "hello world";
    assert_int_equal(-1, embc_cstr_copy(tgt,  MSG1, 0));
    assert_int_equal(tgt[0], 'h'); // not really meaningful
}

static void copy_zero_src_size(void ** state) {
    (void) state;
    char tgt[] = "hello world";
    assert_int_equal(0, embc_cstr_copy(tgt, "", sizeof(tgt)));
    assert_int_equal(tgt[0], 0);
}

static void copy_normal(void ** state) {
    (void) state;
    char tgt[32];
    assert_int_equal(0, embc_cstr_copy(tgt, MSG1, sizeof(tgt)));
    assert_string_equal(tgt, MSG1);
}

static void truncated(void ** state) {
    (void) state;
    char tgt[8];
    assert_int_equal(1, embc_cstr_copy(tgt, MSG1, sizeof(tgt)));
    assert_string_equal(tgt, "hello w");
}

static void array_copy_truncated(void ** state) {
    (void) state;
    char tgt[8];
    assert_int_equal(1, embc_cstr_array_copy(tgt, MSG1));
    assert_string_equal(tgt, "hello w");
}

static void atoi_good_with_space(void ** state) {
    (void) state;
    int32_t value = 3;
    assert_int_equal(0, embc_cstr_to_i32("  42  ", &value));
    assert_int_equal(value, 42);
}

static void atoi_bad(void ** state) {
    (void) state;
    int32_t value = 3;
    assert_int_equal(1, embc_cstr_to_i32("  hello  ", &value));
    assert_int_equal(value, 3);
}

static void atoi_invalid_params(void ** state) {
    (void) state;
    int value = 3;
    assert_int_equal(1, embc_cstr_to_i32(NULL, &value));
    assert_int_equal(1, embc_cstr_to_i32("42", NULL));
    assert_int_equal(1, embc_cstr_to_i32(" ", &value));
    assert_int_equal(value, 3);
}

#if EMBC_CSTR_FLOAT_ENABLE
static void atof_good_with_space(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(0, embc_cstr_to_f32("  4.2  ", &value));
    assert_float_close(value, 4.2, 0.00001);
}

static void atof_good_exponent_form(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(0, safestr_atof("  42.1e-1  ", &value));
    assert_float_close(value, 4.21, 0.00001);
}

static void atof_bad(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(1, safestr_atof("  hello ", &value));
    assert_int_equal(value, 3.0);
}
#endif

static void test_toupper(void ** state) {
    (void) state;
    char msg1[] = "";
    char msg2[] = "lower UPPER 123%#";
    assert_int_equal(1, embc_cstr_toupper(NULL));
    assert_int_equal(0, embc_cstr_toupper(msg1));
    assert_string_equal("", msg1);
    assert_int_equal(0, embc_cstr_toupper(msg2));
    assert_string_equal("LOWER UPPER 123%#", msg2);
}

static const char * const trueTable[] = {"v0", "v1", "v2", "v3", NULL};

static void to_index(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(0, embc_cstr_to_index("v0", trueTable, &index));
    assert_int_equal(0, index);
    assert_int_equal(0, embc_cstr_to_index("v1", trueTable, &index));
    assert_int_equal(1, index);
    assert_int_equal(0, embc_cstr_to_index("v2", trueTable, &index));
    assert_int_equal(2, index);
    assert_int_equal(0, embc_cstr_to_index("v3", trueTable, &index));
    assert_int_equal(3, index);
    assert_int_equal(1, embc_cstr_to_index("other", trueTable, &index));
}

static void to_index_caps(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(1, embc_cstr_to_index("V0", trueTable, &index));
}

static void to_index_invalid(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(2, embc_cstr_to_index("v0", trueTable, NULL));
    assert_int_equal(2, embc_cstr_to_index("v0", NULL, &index));
    assert_int_equal(2, embc_cstr_to_index(NULL, trueTable, &index));
}

static void to_bool(void ** state) {
    (void) state;
    bool value = false;
    assert_int_equal(0, embc_cstr_to_bool("TRUE", &value));      assert_true(value);
    assert_int_equal(0, embc_cstr_to_bool("true", &value));      assert_true(value);
    assert_int_equal(0, embc_cstr_to_bool("on", &value));        assert_true(value);
    assert_int_equal(0, embc_cstr_to_bool("1", &value));         assert_true(value);
    assert_int_equal(0, embc_cstr_to_bool("enable", &value));    assert_true(value);

    assert_int_equal(0, embc_cstr_to_bool("FALSE", &value));     assert_false(value);
    assert_int_equal(0, embc_cstr_to_bool("false", &value));     assert_false(value);
    assert_int_equal(0, embc_cstr_to_bool("off", &value));       assert_false(value);
    assert_int_equal(0, embc_cstr_to_bool("0", &value));         assert_false(value);
    assert_int_equal(0, embc_cstr_to_bool("disable", &value));   assert_false(value);

    assert_int_equal(1, embc_cstr_to_bool("other", &value));
}

static void to_bool_invalid(void ** state) {
    (void) state;
    bool value = false;
    assert_int_equal(1, embc_cstr_to_bool(NULL, &value));
    assert_int_equal(1, embc_cstr_to_bool("TRUE", NULL));
}

static void casecmp(void ** state) {
    (void) state;
    assert_int_equal(0, embc_cstr_casecmp("aajaa", "aajaa"));
    assert_int_equal(0, embc_cstr_casecmp("aajaa", "aaJaa"));
    assert_int_equal(-1, embc_cstr_casecmp("aajaa", "aakaa"));
    assert_int_equal(1, embc_cstr_casecmp("aajaa", "aahaa"));
    assert_int_equal(0, embc_cstr_casecmp("hello", "HELLO"));
}

static void hex_chars(void ** state) {
    (void) state;
    char v_upper[] = "0123456789ABCDEF";
    char v_lower[] = "0123456789abcdef";

    for (int i = 0; i < 16; ++i) {
        assert_int_equal(i, embc_cstr_hex_to_u4(v_upper[i]));
        assert_int_equal(i, embc_cstr_hex_to_u4(v_lower[i]));
        assert_int_equal(v_upper[i], embc_cstr_u4_to_hex((uint8_t) i));
    }
}

static void hex_chars_invalid(void ** state) {
    (void) state;
    assert_int_equal(0, embc_cstr_hex_to_u4('~'));
    assert_int_equal('0', embc_cstr_u4_to_hex(33));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(to_u32_empty),
        cmocka_unit_test(to_u32_zero),
        cmocka_unit_test(to_u32_42),
        cmocka_unit_test(to_u32_0_h),
        cmocka_unit_test(to_u32_hex),
        cmocka_unit_test(to_i32s),
        cmocka_unit_test(copy_zero_tgt_size),
        cmocka_unit_test(copy_zero_src_size),
        cmocka_unit_test(copy_normal),
        cmocka_unit_test(truncated),
        cmocka_unit_test(array_copy_truncated),
        cmocka_unit_test(atoi_good_with_space),
        cmocka_unit_test(atoi_bad),
        cmocka_unit_test(atoi_invalid_params),
#if EMBC_CSTR_FLOAT_ENABLE
        cmocka_unit_test(atof_good_with_space),
        cmocka_unit_test(atof_good_exponent_form),
        cmocka_unit_test(atof_bad),
#endif
        cmocka_unit_test(test_toupper),
        cmocka_unit_test(to_index),
        cmocka_unit_test(to_index_caps),
        cmocka_unit_test(to_index_invalid),
        cmocka_unit_test(to_bool),
        cmocka_unit_test(to_bool_invalid),
        cmocka_unit_test(casecmp),
        cmocka_unit_test(hex_chars),
        cmocka_unit_test(hex_chars_invalid),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
