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
 * A 64-bit intermediate preserves the full difference between any two int32_t
 * coordinates. The raw delta is saturated before it becomes history, keeping
 * every stored and emitted value representable by Zephyr's input_event.value.
 */
static inline bool absolute_to_relative_axis_apply(struct absolute_to_relative_axis_state *state,
                                                   int32_t position, int32_t *relative) {
    if (!state->initialized) {
        state->previous_position = position;
        state->previous_delta = 0;
        state->initialized = true;
        return false;
    }

    int64_t wide_delta = (int64_t)position - state->previous_position;
    int32_t delta;

    if (wide_delta > INT32_MAX) {
        delta = INT32_MAX;
    } else if (wide_delta < INT32_MIN) {
        delta = INT32_MIN;
    } else {
        delta = (int32_t)wide_delta;
    }

    *relative = (int32_t)(((int64_t)delta + state->previous_delta) / 2);
    state->previous_position = position;
    state->previous_delta = delta;
    return true;
}
