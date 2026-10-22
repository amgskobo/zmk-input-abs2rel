/* Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_absolute_to_relative
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/devicetree.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk-input-abs2rel/absolute_to_relative_core.h>
#define ABSOLUTE_TO_RELATIVE_STREAM_COUNT MAX(DT_NUM_INST_STATUS_OKAY(zmk_input_listener), 1)
struct absolute_to_relative_stream {
    struct absolute_to_relative_axis_state x, y;
    atomic_val_t applied_generation;
};
struct absolute_to_relative_data {
    struct absolute_to_relative_stream streams[ABSOLUTE_TO_RELATIVE_STREAM_COUNT];
    atomic_t reset_generation;
};
static inline void drop_reference(struct absolute_to_relative_stream *stream) {
    absolute_to_relative_axis_reset(&stream->x);
    absolute_to_relative_axis_reset(&stream->y);
}
static inline void apply_generation(struct absolute_to_relative_stream *stream,
                                    atomic_val_t generation) {
    drop_reference(stream);
    stream->applied_generation = generation;
}
static inline struct absolute_to_relative_stream *
stream_for_event(struct absolute_to_relative_data *data,
                 const struct zmk_input_processor_state *state) {
    if (state == NULL) { return &data->streams[0]; }
    if (state->input_device_index >= ABSOLUTE_TO_RELATIVE_STREAM_COUNT) { return NULL; }
    return &data->streams[state->input_device_index];
}
/* A reference-only sample has no relative payload. Keep its original frame
 * boundary so the final filter can flush earlier accepted events. */
static void reference_only(struct input_event *event) {
    event->type = INPUT_EV_MSC;
    event->code = INPUT_MSC_SCAN;
    event->value = 0;
}
static void process_axis(struct input_event *event,
                          struct absolute_to_relative_axis_state *axis, uint16_t code) {
    int32_t relative;
    if (!absolute_to_relative_axis_apply(axis, event->value, &relative)) {
        reference_only(event);
        return;
    }
    event->type = INPUT_EV_REL;
    event->code = code;
    event->value = relative;
}
static int absolute_to_relative_handle_event(const struct device *dev, struct input_event *event,
                                             uint32_t param1, uint32_t param2,
                                             struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    struct absolute_to_relative_data *data = dev->data;
    struct absolute_to_relative_stream *stream = stream_for_event(data, state);
    if (stream == NULL) { return ZMK_INPUT_PROC_CONTINUE; }
    atomic_val_t generation = atomic_get(&data->reset_generation);
    if (stream->applied_generation != generation) { apply_generation(stream, generation); }
    /* Both source-button edges reach the common final owner across resets. */
    if (event->type == INPUT_EV_KEY) {
        if (event->code == INPUT_BTN_TOUCH) { drop_reference(stream); }
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type == INPUT_EV_ABS) {
        if (event->code == INPUT_ABS_X) { process_axis(event, &stream->x, INPUT_REL_X); }
        else if (event->code == INPUT_ABS_Y) { process_axis(event, &stream->y, INPUT_REL_Y); }
    }
    atomic_val_t after = atomic_get(&data->reset_generation);
    if (after != generation) {
        apply_generation(stream, after);
        reference_only(event);
    }
    return ZMK_INPUT_PROC_CONTINUE;
}
static int absolute_to_relative_init(const struct device *dev) {
    struct absolute_to_relative_data *data = dev->data;
    atomic_set(&data->reset_generation, 0);
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; ++i) {
        apply_generation(&data->streams[i], 0);
    }
    return 0;
}
static const struct zmk_input_processor_driver_api absolute_to_relative_driver_api = {
    .handle_event = absolute_to_relative_handle_event,
};
#define ABSOLUTE_TO_RELATIVE_INST(n) \
    static struct absolute_to_relative_data data_##n; \
    DEVICE_DT_INST_DEFINE(n, absolute_to_relative_init, NULL, &data_##n, NULL, POST_KERNEL, \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &absolute_to_relative_driver_api);
DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_INST)

#define ABSOLUTE_TO_RELATIVE_RESYNC(n) \
    { struct absolute_to_relative_data *data = DEVICE_DT_INST_GET(n)->data; \
      atomic_inc(&data->reset_generation); }

static int absolute_to_relative_layer_listener(const zmk_event_t *event) {
    ARG_UNUSED(event);
    DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_RESYNC)
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(absolute_to_relative_layer, absolute_to_relative_layer_listener);
ZMK_SUBSCRIPTION(absolute_to_relative_layer, zmk_layer_state_changed);
