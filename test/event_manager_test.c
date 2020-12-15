/* Copyright 2018 Domusys, Inc.  All rights reserved. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include "embc/event_manager.h"
#include "embc/time.h"


struct state_s {
    int64_t time_current;
    struct embc_evm_s * evm;
};

void cbk_full(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

void cbk1(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

void cbk2(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

static int setup(void ** state) {
    struct state_s * s = calloc(1, sizeof(struct state_s));
    s->evm = embc_evm_allocate();
    assert_non_null(s->evm);
    *state = s;
    return 0;
}

static int teardown(void ** state) {
    struct state_s * s = (struct state_s *) * state;
    if (s->evm) {
        embc_evm_free(s->evm);
    }
    free(s);
    return 0;
}

static void test_allocate(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_int_equal(-1, embc_evm_interval_next(s->evm, 10));
    assert_int_equal(EMBC_TIME_MIN, embc_evm_time_next(s->evm));
    embc_evm_process(s->evm, 10);
}

static void test_single_event(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_int_equal(1, embc_evm_schedule(s->evm, 10, cbk_full, s));
    assert_int_equal(10, embc_evm_time_next(s->evm));
    assert_int_equal(8, embc_evm_interval_next(s->evm, 2));

    embc_evm_process(s->evm, 9);
    expect_value(cbk_full, event_id, 1);
    embc_evm_process(s->evm, 10);
}

static void test_insert_two_events_in_order(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_int_equal(1, embc_evm_schedule(s->evm, 10, cbk1, s));
    assert_int_equal(2, embc_evm_schedule(s->evm, 20, cbk2, s));
    assert_int_equal(10, embc_evm_interval_next(s->evm, 0));

    expect_value(cbk1, event_id, 1);
    embc_evm_process(s->evm, 10);
    expect_value(cbk2, event_id, 2);
    embc_evm_process(s->evm, 20);
    assert_int_equal(-1, embc_evm_interval_next(s->evm, 10));
}

static void test_insert_two_events_out_of_order(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_int_equal(1, embc_evm_schedule(s->evm, 20, cbk2, s));
    assert_int_equal(20, embc_evm_interval_next(s->evm, 0));
    assert_int_equal(2, embc_evm_schedule(s->evm, 10, cbk1, s));
    assert_int_equal(10, embc_evm_interval_next(s->evm, 0));

    expect_value(cbk1, event_id, 2);
    embc_evm_process(s->evm, 10);
    expect_value(cbk2, event_id, 1);
    assert_int_equal(20, embc_evm_time_next(s->evm));
    assert_int_equal(8, embc_evm_interval_next(s->evm, 12));
    embc_evm_process(s->evm, 20);
    assert_int_equal(-1, embc_evm_interval_next(s->evm, 10));
}

static void test_insert_two_events_and_cancel_first(void **state) {
    struct state_s * s = (struct state_s *) *state;
    assert_int_equal(1, embc_evm_schedule(s->evm, 10, cbk1, s));
    assert_int_equal(2, embc_evm_schedule(s->evm, 20, cbk2, s));
    assert_int_equal(10, embc_evm_interval_next(s->evm, 0));
    embc_evm_cancel(s->evm, 1);
    expect_value(cbk2, event_id, 2);
    embc_evm_process(s->evm, 20);
    assert_int_equal(-1, embc_evm_interval_next(s->evm, 10));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_allocate, setup, teardown),
            cmocka_unit_test_setup_teardown(test_single_event, setup, teardown),
            cmocka_unit_test_setup_teardown(test_insert_two_events_in_order, setup, teardown),
            cmocka_unit_test_setup_teardown(test_insert_two_events_out_of_order, setup, teardown),
            cmocka_unit_test_setup_teardown(test_insert_two_events_and_cancel_first, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
