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
    /* Consume touch presses and paired releases, after dropping the reference. */
    bool btn_touch;
    /* Consume INPUT_BTN_0 when the pad reports a physical click. */
    bool btn0;
};

/*
 * Reads the flags the processor is applying right now. Returns -ENODEV when
 * dev is not an absolute-to-relative processor instance, or -EINVAL when dev
 * or out is NULL.
 */
int absolute_to_relative_get_suppression(const struct device *dev,
                                         struct absolute_to_relative_suppression *out);

/*
 * Applies new flags.
 *
 * The update emits no input event. For both touch and BTN_0, a release is
 * consumed only while suppression is enabled, its press was consumed by this
 * stream and no earlier forwarded press still needs a release. A consumed
 * repeat cannot revoke that release. Disabling suppression passes future
 * edges through. Layer resets retire consumed-press records, but keep a
 * forwarded press pending until release; init clears both. A new press
 * discarded during a reset is never recorded as forwarded. Forwarded touch
 * edges own a sync boundary. Reapplying the current flags is a no-op for operational logging.
 *
 * Nothing is persisted here; that is the settings layer's job, which is what
 * keeps this driver free of a second owner for the same value.
 * Returns -ENODEV when dev is not an absolute-to-relative processor instance,
 * or -EINVAL when dev or flags is NULL.
 */
int absolute_to_relative_set_suppression(const struct device *dev,
                                         const struct absolute_to_relative_suppression *flags);
