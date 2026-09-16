/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_absolute_to_relative

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/devicetree.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>

#include <zmk-input-abs2rel/absolute_to_relative.h>
#include <zmk-input-abs2rel/absolute_to_relative_core.h>

LOG_MODULE_REGISTER(absolute_to_relative, CONFIG_ZMK_LOG_LEVEL);

#define COORD_INVALID_ZERO 0xFFF
#define SUPPRESS_BTN_TOUCH_BIT 0
#define SUPPRESS_BTN0_BIT 1
#define ABSOLUTE_TO_RELATIVE_LISTENER_COUNT DT_NUM_INST_STATUS_OKAY(zmk_input_listener)
#define ABSOLUTE_TO_RELATIVE_STREAM_COUNT MAX(ABSOLUTE_TO_RELATIVE_LISTENER_COUNT, 1)

/* The devicetree values, which seed the live ones in data at init. */
struct absolute_to_relative_config {
    struct absolute_to_relative_suppression suppression;
};

struct absolute_to_relative_stream {
    struct absolute_to_relative_axis_state x;
    struct absolute_to_relative_axis_state y;
    /*
     * Set while a BTN_0 press has been suppressed here and its release has not
     * been seen yet. Suppression has to stay paired: which processors run is
     * decided per event from the layer active at that moment, so a press and
     * its release can be routed differently when the layer changes in between.
     * Dropping a release whose press was never suppressed leaves the button
     * held down on the host with nothing left to release it.
     */
    bool btn0_press_suppressed;
    atomic_val_t applied_generation;
};

struct absolute_to_relative_data {
    /* Each listener keeps an independent coordinate and button history. */
    struct absolute_to_relative_stream streams[ABSOLUTE_TO_RELATIVE_STREAM_COUNT];
    /*
     * A layer callback can run outside the input thread. It only increments
     * this shared atomic generation; the input thread remains the sole writer
     * of every stream. Each stream applies a new generation when it next sees
     * an event. If the generation moves during an event, that event is
     * discarded and its stream starts from a clean reference.
     */
    atomic_t reset_generation;
    /* Runtime settings callbacks and input handling may run in different contexts. */
    atomic_t suppression_flags;
};

static atomic_val_t encode_suppression(const struct absolute_to_relative_suppression *suppression) {
    atomic_val_t flags = 0;

    if (suppression->btn_touch) {
        flags |= BIT(SUPPRESS_BTN_TOUCH_BIT);
    }
    if (suppression->btn0) {
        flags |= BIT(SUPPRESS_BTN0_BIT);
    }

    return flags;
}

int absolute_to_relative_get_suppression(const struct device *dev,
                                         struct absolute_to_relative_suppression *out) {
    if (dev == NULL || out == NULL) {
        return -EINVAL;
    }

    const struct absolute_to_relative_data *data = dev->data;
    atomic_val_t flags = atomic_get(&data->suppression_flags);

    *out = (struct absolute_to_relative_suppression){
        .btn_touch = (flags & BIT(SUPPRESS_BTN_TOUCH_BIT)) != 0,
        .btn0 = (flags & BIT(SUPPRESS_BTN0_BIT)) != 0,
    };

    return 0;
}

int absolute_to_relative_set_suppression(const struct device *dev,
                                         const struct absolute_to_relative_suppression *flags) {
    if (dev == NULL || flags == NULL) {
        return -EINVAL;
    }

    struct absolute_to_relative_data *data = dev->data;

    atomic_set(&data->suppression_flags, encode_suppression(flags));

    LOG_DBG("%s: suppress btn_touch %d, btn0 %d", dev->name, flags->btn_touch, flags->btn0);

    return 0;
}

/**
 * Drop the reference point, so the next sample on each axis establishes a new
 * one instead of being measured against a position that no longer relates to it.
 */
static inline void drop_reference(struct absolute_to_relative_stream *stream) {
    absolute_to_relative_axis_reset(&stream->x);
    absolute_to_relative_axis_reset(&stream->y);
}

static inline void apply_generation(struct absolute_to_relative_stream *stream,
                                    atomic_val_t generation) {
    drop_reference(stream);
    stream->btn0_press_suppressed = false;
    stream->applied_generation = generation;
}

static inline struct absolute_to_relative_stream *
stream_for_event(struct absolute_to_relative_data *data,
                 const struct zmk_input_processor_state *state) {
    if (state == NULL) {
        return &data->streams[0];
    }

    if (state->input_device_index >= ABSOLUTE_TO_RELATIVE_STREAM_COUNT) {
        LOG_ERR("Input device index %u exceeds the %u allocated abs2rel streams",
                state->input_device_index, ABSOLUTE_TO_RELATIVE_STREAM_COUNT);
        return NULL;
    }

    return &data->streams[state->input_device_index];
}

/**
 * Process absolute-to-relative conversion for a single axis
 * Returns true if first position (should suppress event), false if normal motion
 */
static inline bool process_axis(struct input_event *event,
                                struct absolute_to_relative_axis_state *axis, uint16_t rel_code) {
    int32_t relative;

    if (!absolute_to_relative_axis_apply(axis, event->value, &relative)) {
        /* First report on this axis - store position and suppress output. */
        if (IS_ENABLED(CONFIG_ZMK_LOG_LEVEL_DBG)) {
            LOG_DBG("Initial %s position: %d (suppressed)", (rel_code == INPUT_REL_X) ? "X" : "Y",
                    event->value);
        }

        /* Mark event as invalid for clarity */
        event->code = COORD_INVALID_ZERO;
        event->sync = false;

        return true; /* Signal to suppress this event */
    }

    /*
     * Calculate delta and apply smoothing (use local prev to reduce memory
     * access).
     *
     * Halved by dividing rather than shifting. A shift rounds towards minus
     * infinity, so an odd sum loses its half going one way and gains it going
     * the other, and the same path travelled in opposite directions does not
     * come back to where it started. Division truncates towards zero, which
     * treats both directions alike.
     */
    if (IS_ENABLED(CONFIG_ZMK_LOG_LEVEL_DBG)) {
        LOG_DBG("%s: %d -> rel_%s: %d", (rel_code == INPUT_REL_X) ? "X" : "Y", event->value,
                (rel_code == INPUT_REL_X) ? "x" : "y", relative);
    }

    /* Update event and state */
    event->type = INPUT_EV_REL;
    event->code = rel_code;
    event->value = relative;

    return false; /* Signal to continue processing */
}

/**
 * Handle touch button events (BTN_TOUCH)
 *
 * Both edges drop the reference point. A press begins a contact that bears no
 * relation to the last one, and a release ends one - and the driver emits a
 * final absolute pair just after the release, which would otherwise be turned
 * into motion against a position that has stopped meaning anything.
 *
 * The press is never treated as redundant, even when a contact already looks
 * active. Which processors run is decided per event from the layer active at
 * that moment, so this instance may simply have missed the release that ended
 * the previous contact; skipping the reset in that case would measure the new
 * contact against the old one's position.
 */
static int handle_touch_button(struct input_event *event, struct absolute_to_relative_data *data,
                               struct absolute_to_relative_stream *stream) {
    drop_reference(stream);

    if (IS_ENABLED(CONFIG_ZMK_LOG_LEVEL_DBG)) {
        LOG_DBG("Touch %s - reference dropped", event->value ? "started" : "released");
    }

    if (atomic_get(&data->suppression_flags) & BIT(SUPPRESS_BTN_TOUCH_BIT)) {
        if (IS_ENABLED(CONFIG_ZMK_LOG_LEVEL_DBG)) {
            LOG_DBG("Suppressing BTN_TOUCH");
        }
        event->code = COORD_INVALID_ZERO;
        event->sync = false;
        return ZMK_INPUT_PROC_STOP;
    }

    /* The source groups this edge with coordinates using sync=false. Once the
     * first coordinate pair is consumed as a reference, no later event may be
     * available to flush a release. A forwarded edge therefore owns a report
     * boundary of its own. */
    event->sync = true;

    return ZMK_INPUT_PROC_CONTINUE;
}

/**
 * Handle button suppression (BTN_0)
 *
 * A release is only dropped when the matching press was dropped here. A press
 * that reached the host on another layer keeps its release, so the button can
 * never be left stuck down.
 */
static int handle_button_suppress(struct input_event *event,
                                  struct absolute_to_relative_data *data,
                                  struct absolute_to_relative_stream *stream) {
    if (!(atomic_get(&data->suppression_flags) & BIT(SUPPRESS_BTN0_BIT))) {
        /* Not suppressing here, so nothing of ours is outstanding. */
        stream->btn0_press_suppressed = false;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    bool was_suppressed = stream->btn0_press_suppressed;

    stream->btn0_press_suppressed = event->value != 0;

    if (!event->value && !was_suppressed) {
        LOG_WRN("Passing BTN_0 release: its press was not suppressed here");
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (IS_ENABLED(CONFIG_ZMK_LOG_LEVEL_DBG)) {
        LOG_DBG("Suppressing BTN_0 %s", event->value ? "press" : "release");
    }
    event->code = COORD_INVALID_ZERO;
    event->sync = false;
    return ZMK_INPUT_PROC_STOP;
}

/**
 * Main event handler - converts absolute input events to relative
 */
static int absolute_to_relative_handle_event(const struct device *dev, struct input_event *event,
                                             uint32_t param1, uint32_t param2,
                                             struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;
    struct absolute_to_relative_stream *stream = stream_for_event(data, state);
    if (stream == NULL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    atomic_val_t generation_before = atomic_get(&data->reset_generation);

    if (generation_before != stream->applied_generation) {
        apply_generation(stream, generation_before);
    }

    int result = ZMK_INPUT_PROC_CONTINUE;

    /* Handle button events */
    if (event->type == INPUT_EV_KEY) {
        if (event->code == INPUT_BTN_TOUCH) {
            result = handle_touch_button(event, data, stream);
        } else if (event->code == INPUT_BTN_0) {
            result = handle_button_suppress(event, data, stream);
        }
    } else if (event->type == INPUT_EV_ABS) {
        /*
         * Convert absolute axes to relative motion.
         *
         * There is deliberately no contact-state gate here. Contact state would be
         * per instance, but an instance only sees the events that arrive while it
         * holds the chain, and the chain is chosen per event from the layer active
         * at that moment. An instance that missed the press of the contact now in
         * progress would gate itself off for the rest of it and pass absolute
         * events through unconverted - which reads as the pointer dying mid-stroke
         * until the finger is lifted, and only intermittently, since an instance
         * that once saw a press without its release stays open by accident.
         *
         * The reference point already covers not knowing where the finger was: the
         * first sample on an axis establishes it and is suppressed, and the next
         * one converts. That is the same sample every contact spends at its start.
         */
        if (event->code == INPUT_ABS_X) {
            result = process_axis(event, &stream->x, INPUT_REL_X) ? ZMK_INPUT_PROC_STOP
                                                                  : ZMK_INPUT_PROC_CONTINUE;
        } else if (event->code == INPUT_ABS_Y) {
            result = process_axis(event, &stream->y, INPUT_REL_Y) ? ZMK_INPUT_PROC_STOP
                                                                  : ZMK_INPUT_PROC_CONTINUE;
        }
    }

    atomic_val_t generation_after = atomic_get(&data->reset_generation);
    if (generation_after != generation_before) {
        apply_generation(stream, generation_after);
        event->code = COORD_INVALID_ZERO;
        event->sync = false;
        return ZMK_INPUT_PROC_STOP;
    }

    return result;
}

/**
 * Device initialization
 */
static int absolute_to_relative_init(const struct device *dev) {
    struct absolute_to_relative_data *data = (struct absolute_to_relative_data *)dev->data;
    const struct absolute_to_relative_config *config = dev->config;

    atomic_set(&data->suppression_flags, encode_suppression(&config->suppression));
    atomic_set(&data->reset_generation, 0);
    for (size_t i = 0U; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        data->streams[i].btn0_press_suppressed = false;
        data->streams[i].applied_generation = 0;
        drop_reference(&data->streams[i]);
    }

    LOG_INF("Initialized (suppress_btn_touch=%d, suppress_btn0=%d)", config->suppression.btn_touch,
            config->suppression.btn0);

    return 0;
}

/**
 * Driver API
 */
static const struct zmk_input_processor_driver_api absolute_to_relative_driver_api = {
    .handle_event = absolute_to_relative_handle_event,
};

/**
 * Device instantiation macro
 */
#define ABSOLUTE_TO_RELATIVE_INST(n)                                                               \
    static struct absolute_to_relative_data processor_absolute_to_relative_data_##n;               \
    static const struct absolute_to_relative_config processor_absolute_to_relative_config_##n = {  \
        .suppression =                                                                             \
            {                                                                                      \
                .btn_touch = DT_INST_PROP_OR(n, suppress_btn_touch, true),                         \
                .btn0 = DT_INST_PROP_OR(n, suppress_btn0, false),                                  \
            },                                                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, absolute_to_relative_init, NULL,                                      \
                          &processor_absolute_to_relative_data_##n,                                \
                          &processor_absolute_to_relative_config_##n, POST_KERNEL,                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &absolute_to_relative_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_INST)

/**
 * Drop the reference point when the layer changes.
 *
 * Which processors run is decided per event from the layer active at that
 * moment, so one contact can be split across two instances of this processor.
 * The instance the contact moves to has a reference point left over from an
 * earlier contact, and the first sample it sees is turned into the distance
 * between two unrelated touches - a single delta of up to the whole pad, which
 * an acceleration or inertia stage downstream then multiplies.
 *
 * Dropping the reference costs the one sample spent re-establishing it, the
 * same sample every contact already spends when it starts.
 *
 * Nothing here records whether a contact is in progress, deliberately. An
 * instance only sees the events that arrive while it holds the chain, so a
 * flag taken from BTN_TOUCH would be wrong for exactly the contact this reset
 * exists to rescue.
 */
#define ABSOLUTE_TO_RELATIVE_RESYNC(n)                                                             \
    {                                                                                              \
        struct absolute_to_relative_data *data = DEVICE_DT_INST_GET(n)->data;                      \
        atomic_inc(&data->reset_generation);                                                       \
    }

static int absolute_to_relative_layer_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_RESYNC)

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(absolute_to_relative_layer, absolute_to_relative_layer_listener);
ZMK_SUBSCRIPTION(absolute_to_relative_layer, zmk_layer_state_changed);
