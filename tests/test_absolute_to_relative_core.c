/*
 * Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <zmk-input-abs2rel/absolute_to_relative_core.h>

static int32_t apply(struct absolute_to_relative_axis_state *state, int32_t position) {
    int32_t relative = INT32_MIN;
    assert(absolute_to_relative_axis_apply(state, position, &relative));
    return relative;
}

static void test_first_sample_and_reset(void) {
    struct absolute_to_relative_axis_state state;
    int32_t relative = 123;

    absolute_to_relative_axis_reset(&state);
    assert(!absolute_to_relative_axis_apply(&state, 100, &relative));
    assert(relative == 123);
    assert(apply(&state, 110) == 5);
    assert(apply(&state, 120) == 10);

    absolute_to_relative_axis_reset(&state);
    assert(!absolute_to_relative_axis_apply(&state, 900, &relative));
    assert(apply(&state, 890) == -5);

    /* A reset forgets the history, not only the seed flag. */
    absolute_to_relative_axis_reset(&state);
    assert(state.previous_position == 0 && state.previous_delta == 0 && !state.initialized);

    /* A first sample drops whatever delta was left behind, however it got
     * there, so the second sample is not averaged with it. */
    state.previous_delta = 100;
    assert(!absolute_to_relative_axis_apply(&state, 5, &relative));
    assert(apply(&state, 15) == 5);
}

static void test_symmetric_rounding(void) {
    struct absolute_to_relative_axis_state positive;
    struct absolute_to_relative_axis_state negative;
    int32_t ignored;

    absolute_to_relative_axis_reset(&positive);
    absolute_to_relative_axis_reset(&negative);
    assert(!absolute_to_relative_axis_apply(&positive, 0, &ignored));
    assert(!absolute_to_relative_axis_apply(&negative, 0, &ignored));

    assert(apply(&positive, 1) == 0);
    assert(apply(&negative, -1) == 0);
    assert(apply(&positive, 3) == 1);
    assert(apply(&negative, -3) == -1);
}

static void test_full_int32_coordinate_range(void) {
    struct absolute_to_relative_axis_state state;
    int32_t ignored;

    absolute_to_relative_axis_reset(&state);
    assert(!absolute_to_relative_axis_apply(&state, INT32_MIN, &ignored));
    assert(apply(&state, INT32_MAX) == INT32_MAX / 2);
    assert(apply(&state, INT32_MIN) == 0);

    absolute_to_relative_axis_reset(&state);
    assert(!absolute_to_relative_axis_apply(&state, INT32_MAX, &ignored));
    assert(apply(&state, INT32_MIN) == INT32_MIN / 2);
}

static void test_independent_streams(void) {
    struct absolute_to_relative_axis_state left;
    struct absolute_to_relative_axis_state right;
    int32_t ignored;

    absolute_to_relative_axis_reset(&left);
    absolute_to_relative_axis_reset(&right);
    assert(!absolute_to_relative_axis_apply(&left, 100, &ignored));
    assert(!absolute_to_relative_axis_apply(&right, 900, &ignored));
    assert(apply(&left, 110) == 5);
    assert(apply(&right, 880) == -10);
}

static void test_signed_average_grid(void) {
    for (int32_t delta = -33; delta <= 33; delta++) {
        for (int32_t previous = -33; previous <= 33; previous++) {
            struct absolute_to_relative_axis_state state = {
                .initialized = true, .previous_position = 0, .previous_delta = previous,
            };
            assert(apply(&state, delta) == (int32_t)(((int64_t)delta + previous) / 2));
        }
    }
}

int main(void) {
    test_first_sample_and_reset();
    test_symmetric_rounding();
    test_full_int32_coordinate_range();
    test_independent_streams();
    test_signed_average_grid();
    puts("absolute-to-relative core tests: PASS");
    return 0;
}
