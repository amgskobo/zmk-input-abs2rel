/*
 * Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 *
 * Runtime suppression flags for the absolute-to-relative processor.
 *
 * This processor has no runtime prefix, and that is why it lives in its own
 * module. The prefix marks a processor that replaces a fixed upstream one, and
 * the whole of zmk-input-processors is those replacements. This has no
 * upstream counterpart -- it is an original -- so it belongs with the other
 * originals, each in a module of its own.
 */

#pragma once

#include <stdbool.h>

#include <zephyr/device.h>

struct absolute_to_relative_suppression {
    /* Consume INPUT_BTN_TOUCH after using it to drop the reference point. */
    bool btn_touch;
    /* Consume INPUT_BTN_0 when the pad reports a physical click. */
    bool btn0;
};

/*
 * Reads the flags the processor is applying right now. Returns -ENODEV when
 * dev is not an absolute-to-relative processor instance.
 */
int absolute_to_relative_get_suppression(const struct device *dev,
                                         struct absolute_to_relative_suppression *out);

/*
 * Applies new flags.
 *
 * Turning btn0 suppression off does not release a press this processor has
 * already swallowed: the driver still passes that press's release through, for
 * the same reason it does across a layer change. Dropping it would leave the
 * button held down with nothing left to release it.
 *
 * Nothing is persisted here; that is the settings layer's job, which is what
 * keeps this driver free of a second owner for the same value.
 * Returns -ENODEV when dev is not an absolute-to-relative processor instance.
 */
int absolute_to_relative_set_suppression(const struct device *dev,
                                         const struct absolute_to_relative_suppression *flags);
