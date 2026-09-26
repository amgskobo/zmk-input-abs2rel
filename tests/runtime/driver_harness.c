/* Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <zmk-input-abs2rel/absolute_to_relative_core.h>

typedef int atomic_val_t;
typedef int atomic_t;
#define BIT(n) (1 << (n))
#define SUPPRESS_BTN_TOUCH_BIT 0
#define SUPPRESS_BTN0_BIT 1
#define ABSOLUTE_TO_RELATIVE_STREAM_COUNT 2
#define COORD_INVALID_ZERO 0xfff
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define INPUT_EV_REL 2
#define INPUT_EV_ABS 3
#define INPUT_EV_KEY 4
#define INPUT_ABS_X 5
#define INPUT_ABS_Y 6
#define INPUT_BTN_TOUCH 7
#define INPUT_BTN_0 8
#define ZMK_INPUT_PROC_STOP 1
#define ZMK_INPUT_PROC_CONTINUE 0
#define CONFIG_ZMK_LOG_LEVEL_DBG 0
#define IS_ENABLED(x) (x)
#define LOG_DBG(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define ARG_UNUSED(x) ((void)(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define DEVICE_DT_INST_GET(n) (&valid_device)
#define DT_INST_FOREACH_STATUS_OKAY(macro) macro(0)
#define ZMK_EV_EVENT_BUBBLE 0

struct absolute_to_relative_suppression { bool btn_touch, btn0; };
struct absolute_to_relative_stream {
    struct absolute_to_relative_axis_state x, y;
    bool btn0_press_suppressed;
    atomic_val_t applied_generation;
};
struct absolute_to_relative_data {
    struct absolute_to_relative_stream streams[ABSOLUTE_TO_RELATIVE_STREAM_COUNT];
    atomic_t reset_generation, suppression_flags;
};
struct absolute_to_relative_config { struct absolute_to_relative_suppression suppression; };
struct zmk_input_processor_state { size_t input_device_index; };
struct input_event { int type, code; int32_t value; bool sync; };
struct device { void *data; const void *config; const char *name; };
typedef int zmk_event_t;
static struct device valid_device;
/* Another instance first, so a lookup has to walk the list. */
static struct device first_device;
static const struct device *const absolute_to_relative_devices[] = {&first_device, &valid_device};
static atomic_t *watched_generation;
static int generation_reads, jump_on_read;
static atomic_val_t atomic_get(const atomic_t *value) {
    if (value == watched_generation && ++generation_reads == jump_on_read) {
        ++*watched_generation;
    }
    return *value;
}
static void atomic_set(atomic_t *value, atomic_val_t next) { *value = next; }
static void atomic_inc(atomic_t *value) { ++*value; }

/* DRIVER_FUNCTIONS */

int main(void) {
    struct absolute_to_relative_suppression flags = {0};
    assert(encode_suppression(&flags) == 0);
    flags.btn_touch = true;
    assert(encode_suppression(&flags) == BIT(SUPPRESS_BTN_TOUCH_BIT));
    flags.btn_touch = false;
    flags.btn0 = true;
    assert(encode_suppression(&flags) == BIT(SUPPRESS_BTN0_BIT));
    flags.btn_touch = true;
    assert(encode_suppression(&flags) == 3);

    struct absolute_to_relative_data data = {0};
    struct absolute_to_relative_stream *stream = &data.streams[0];
    struct zmk_input_processor_state state = {0};
    assert(stream_for_event(&data, NULL) == stream);
    assert(stream_for_event(&data, &state) == stream);
    state.input_device_index = 1;
    assert(stream_for_event(&data, &state) == &data.streams[1]);
    state.input_device_index = 2;
    assert(stream_for_event(&data, &state) == NULL);

    struct input_event event = {.value = 50, .sync = true};
    assert(process_axis(&event, &stream->x, INPUT_REL_X));
    assert(event.code == COORD_INVALID_ZERO && !event.sync);
    event.value = 60;
    assert(!process_axis(&event, &stream->x, INPUT_REL_X));
    assert(event.type == INPUT_EV_REL && event.code == INPUT_REL_X && event.value == 5);
    stream->btn0_press_suppressed = true;
    apply_generation(stream, 2);
    assert(!stream->x.initialized && !stream->y.initialized);
    assert(stream->applied_generation == 2 && !stream->btn0_press_suppressed);

    data.suppression_flags = 0;
    stream->x.initialized = stream->y.initialized = true;
    event = (struct input_event){.value = 1, .sync = false};
    assert(handle_touch_button(&event, &data, stream) == ZMK_INPUT_PROC_CONTINUE);
    assert(event.sync && !stream->x.initialized && !stream->y.initialized);
    data.suppression_flags = BIT(SUPPRESS_BTN_TOUCH_BIT);
    event = (struct input_event){.value = 0, .sync = true};
    assert(handle_touch_button(&event, &data, stream) == ZMK_INPUT_PROC_STOP);
    assert(event.code == COORD_INVALID_ZERO && !event.sync);

    data.suppression_flags = 0;
    stream->btn0_press_suppressed = true;
    event = (struct input_event){.value = 1, .sync = true};
    assert(handle_button_suppress(&event, &data, stream) == ZMK_INPUT_PROC_CONTINUE);
    assert(!stream->btn0_press_suppressed);
    data.suppression_flags = BIT(SUPPRESS_BTN0_BIT);
    event = (struct input_event){.value = 0, .sync = true};
    assert(handle_button_suppress(&event, &data, stream) == ZMK_INPUT_PROC_CONTINUE);
    assert(!stream->btn0_press_suppressed);
    event.value = 1;
    assert(handle_button_suppress(&event, &data, stream) == ZMK_INPUT_PROC_STOP);
    assert(stream->btn0_press_suppressed && event.code == COORD_INVALID_ZERO && !event.sync);
    event = (struct input_event){.value = 0, .sync = true};
    assert(handle_button_suppress(&event, &data, stream) == ZMK_INPUT_PROC_STOP);
    assert(!stream->btn0_press_suppressed && event.code == COORD_INVALID_ZERO && !event.sync);

    struct device dev = {.data = &data};
    state.input_device_index = 2;
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_X, .value = 9};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    state.input_device_index = 0;
    data.reset_generation = 3;
    stream->applied_generation = 2;
    data.suppression_flags = 0;
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_X, .value = 9};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    assert(stream->applied_generation == 3 && !event.sync);
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_X, .value = 19};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    assert(event.type == INPUT_EV_REL && event.code == INPUT_REL_X);
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_Y, .value = 11};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_Y, .value = 21};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    event = (struct input_event){.type = INPUT_EV_ABS, .code = 99};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH, .value = 1};
    assert(stream->x.initialized);
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    assert(event.sync && !stream->x.initialized); /* the press drops the reference */
    data.suppression_flags = BIT(SUPPRESS_BTN0_BIT);
    event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_0, .value = 1};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    event = (struct input_event){.type = INPUT_EV_KEY, .code = 99};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    event = (struct input_event){.type = 99, .code = 99};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_CONTINUE);
    watched_generation = &data.reset_generation;
    generation_reads = 0;
    jump_on_read = 2;
    event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_X, .value = 42, .sync = true};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    assert(stream->applied_generation == 4 && event.code == COORD_INVALID_ZERO && !event.sync);
    /* A key that would have passed, discarded the same way. */
    generation_reads = 0;
    event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH, .value = 1};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    assert(stream->applied_generation == 5 && event.code == COORD_INVALID_ZERO && !event.sync);
    watched_generation = NULL;

    struct absolute_to_relative_suppression readback = {0};
    struct absolute_to_relative_config config = {.suppression = {.btn_touch = true, .btn0 = false}};
    valid_device = (struct device){.data = &data, .config = &config, .name = "test"};
    struct device other_device = {.data = &data};
    assert(absolute_to_relative_get_suppression(NULL, &readback) == -EINVAL);
    assert(absolute_to_relative_get_suppression(&valid_device, NULL) == -EINVAL);
    assert(absolute_to_relative_get_suppression(&other_device, &readback) == -ENODEV);
    assert(absolute_to_relative_set_suppression(NULL, &flags) == -EINVAL);
    assert(absolute_to_relative_set_suppression(&valid_device, NULL) == -EINVAL);
    assert(absolute_to_relative_set_suppression(&other_device, &flags) == -ENODEV);
    /* Init resets every stream, whatever it held before. */
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        data.streams[i].btn0_press_suppressed = true;
        data.streams[i].applied_generation = 7;
        data.streams[i].x.initialized = data.streams[i].y.initialized = true;
    }
    assert(absolute_to_relative_init(&valid_device) == 0);
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        assert(!data.streams[i].btn0_press_suppressed && data.streams[i].applied_generation == 0);
    }
    assert(data.suppression_flags == BIT(SUPPRESS_BTN_TOUCH_BIT));
    assert(data.reset_generation == 0 && !stream->x.initialized && !stream->y.initialized);
    assert(!data.streams[1].x.initialized && !data.streams[1].y.initialized);
    assert(absolute_to_relative_get_suppression(&valid_device, &readback) == 0);
    assert(readback.btn_touch && !readback.btn0);
    flags = (struct absolute_to_relative_suppression){.btn_touch = false, .btn0 = true};
    assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
    assert(absolute_to_relative_get_suppression(&valid_device, &readback) == 0);
    assert(!readback.btn_touch && readback.btn0);
    assert(absolute_to_relative_layer_listener(NULL) == ZMK_EV_EVENT_BUBBLE);
    /* Every device in the list counts, the first as much as the last. */
    assert(absolute_to_relative_device_valid(&first_device));
    assert(data.reset_generation == 1);
    puts("absolute-to-relative driver event helpers: PASS");
    return 0;
}
