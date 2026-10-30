/*
 * Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct absolute_to_relative_axis_state {
    int32_t previous_position;
    int32_t previous_delta;
    bool initialized;
};

static inline void absolute_to_relative_axis_reset(struct absolute_to_relative_axis_state *state) {
    state->previous_position = 0;
    state->previous_delta = 0;
    state->initialized = false;
}

/*
 * Convert one absolute coordinate. The first value seeds the reference and
 * returns false. Later values return true and write the two-sample average.
 *
 * Checked subtraction preserves saturation across the full coordinate range.
 * The average splits each delta into quotient/remainder first, preserving one
 * final truncation toward zero without overflowing a 32-bit sum.
 */
static inline bool absolute_to_relative_axis_apply(struct absolute_to_relative_axis_state *state,
                                                   int32_t position, int32_t *relative) {
    if (!state->initialized) {
        state->previous_position = position;
        state->previous_delta = 0;
        state->initialized = true;
        return false;
    }

    int32_t delta;
    if (__builtin_sub_overflow(position, state->previous_position, &delta)) {
        delta = position > state->previous_position ? INT32_MAX : INT32_MIN;
    }

    const int32_t odd = delta % 2 + state->previous_delta % 2;
    int32_t average = delta / 2 + state->previous_delta / 2 + odd / 2;
    if (average < 0 && odd == 1) {
        average++;
    } else if (average > 0 && odd == -1) {
        average--;
    }
    *relative = average;
    state->previous_position = position;
    state->previous_delta = delta;
    return true;
}
