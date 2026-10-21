/* Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
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
static unsigned info_count;
static char last_info[160];
static void record_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(last_info, sizeof(last_info), format, args);
    va_end(args);
    ++info_count;
}
#define LOG_INF(...) record_info(__VA_ARGS__)
#define ARG_UNUSED(x) ((void)(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define DEVICE_DT_INST_GET(n) (&valid_device)
#define DT_INST_FOREACH_STATUS_OKAY(macro) macro(0)
#define ZMK_EV_EVENT_BUBBLE 0

struct absolute_to_relative_suppression { bool btn_touch, btn0; };
struct absolute_to_relative_stream {
    struct absolute_to_relative_axis_state x, y;
    bool btn_touch_press_suppressed, btn0_press_suppressed;
    bool btn_touch_press_forwarded, btn0_press_forwarded;
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
static atomic_val_t atomic_set(atomic_t *value, atomic_val_t next) {
    atomic_val_t previous = *value;
    *value = next;
    return previous;
}
static void atomic_inc(atomic_t *value) { ++*value; }

/* Deliberately alias only button histories in the focused test-only control. */
#ifdef ABS2REL_TEST_ALIAS_BUTTON_HISTORY
#define btn0_press_suppressed btn_touch_press_suppressed
#define btn0_press_forwarded btn_touch_press_forwarded
#endif

/* DRIVER_FUNCTIONS */

/* A live policy edit cannot consume the release of a forwarded touch. */
static void test_touch_release_pairing(void) {
    struct absolute_to_relative_data data = {0};
    const struct absolute_to_relative_config config = {.suppression = {false, false}};
    const struct device saved_device = valid_device;
    valid_device = (struct device){.data = &data, .config = &config};
    assert(absolute_to_relative_init(&valid_device) == 0);
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        struct zmk_input_processor_state state = {.input_device_index = i};
        struct absolute_to_relative_suppression flags = {.btn_touch = false};
        assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
        struct input_event event = {.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                    .value = 1, .sync = false};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        assert(event.code == INPUT_BTN_TOUCH && event.sync);
        flags.btn_touch = true;
        assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 0, .sync = false};
        int result = absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state);
        fprintf(stderr, "touch policy edit stream=%zu release=%d code=%d sync=%d\n",
                i, result, event.code, event.sync);
        assert(result == ZMK_INPUT_PROC_CONTINUE && event.code == INPUT_BTN_TOUCH && event.sync);

        /* Suppressed press/repeat/release remains consumed for this stream. */
        for (int value = 1; value <= 2; value++) {
            event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                         .value = value, .sync = true};
            assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                   ZMK_INPUT_PROC_STOP);
            assert(event.code == COORD_INVALID_ZERO && !event.sync);
        }
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 0, .sync = true};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_STOP);
        assert(event.code == COORD_INVALID_ZERO && !event.sync);
        /* A second release has no locally suppressed press to consume. */
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 0, .sync = false};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        assert(event.code == INPUT_BTN_TOUCH && event.sync);

        /* Disabling suppression passes a later release and retires the record. */
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 1};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_STOP);
        flags.btn_touch = false;
        assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 0, .sync = false};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        assert(event.code == INPUT_BTN_TOUCH && event.sync);
        assert(!data.streams[i].btn_touch_press_suppressed);

        /* A route/generation reset retires a suppressed press's ownership. */
        flags.btn_touch = true;
        assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 1};
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_STOP);
        assert(absolute_to_relative_layer_listener(NULL) == ZMK_EV_EVENT_BUBBLE);
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH,
                                     .value = 0, .sync = false};
        watched_generation = &data.reset_generation;
        generation_reads = 0;
        jump_on_read = 2;
        assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        assert(event.code == INPUT_BTN_TOUCH && event.sync);
        watched_generation = NULL;
    }
    /* A press suppressed on one listener cannot claim another's release. */
    struct zmk_input_processor_state first = {.input_device_index = 0};
    struct zmk_input_processor_state second = {.input_device_index = 1};
    struct input_event event = {.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH, .value = 1};
    assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &first) ==
           ZMK_INPUT_PROC_STOP);
    event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH, .value = 0};
    assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &second) ==
           ZMK_INPUT_PROC_CONTINUE);
    assert(event.code == INPUT_BTN_TOUCH && event.sync);
    event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_TOUCH, .value = 0};
    assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &first) ==
           ZMK_INPUT_PROC_STOP);
    assert(event.code == COORD_INVALID_ZERO && !event.sync);
    puts("touch release pairing across settings, streams and layer resets: PASS");
    valid_device = saved_device;
}

/* A consumed repeat cannot revoke a release owed by an earlier forwarded press. */
static void test_repeat_preserves_forwarded_release(void) {
    const struct device saved_device = valid_device;
    const int codes[] = {INPUT_BTN_TOUCH, INPUT_BTN_0};
    const int32_t repeats[] = {1, 2, -1, INT32_MAX};
    unsigned missing[2] = {0};
    for (size_t button = 0; button < ARRAY_SIZE(codes); button++) {
        for (size_t listener = 0; listener < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; listener++) {
            for (unsigned reset = 0; reset < 3; reset++) {
                for (size_t repeat = 0; repeat < ARRAY_SIZE(repeats); repeat++) {
                    struct absolute_to_relative_data data = {0};
                    const struct absolute_to_relative_config config = {.suppression = {false, false}};
                    valid_device = (struct device){.data = &data, .config = &config};
                    assert(absolute_to_relative_init(&valid_device) == 0);
                    struct zmk_input_processor_state state = {.input_device_index = listener};
                    struct input_event event = {.type = INPUT_EV_KEY, .code = codes[button],
                                                .value = 1, .sync = true};
                    assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                           ZMK_INPUT_PROC_CONTINUE);
                    assert(event.code == codes[button] && event.sync);
                    const struct absolute_to_relative_suppression flags = {true, true};
                    assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
                    if (reset == 1) {
                        assert(absolute_to_relative_layer_listener(NULL) == ZMK_EV_EVENT_BUBBLE);
                    } else if (reset == 2) {
                        watched_generation = &data.reset_generation;
                        generation_reads = 0;
                        jump_on_read = 2;
                    }
                    event = (struct input_event){.type = INPUT_EV_KEY, .code = codes[button],
                                                 .value = repeats[repeat], .sync = true};
                    assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                           ZMK_INPUT_PROC_STOP);
                    assert(event.code == COORD_INVALID_ZERO && !event.sync);
                    watched_generation = NULL;
                    event = (struct input_event){.type = INPUT_EV_KEY, .code = codes[button],
                                                 .value = 0, .sync = button != 0};
                    int result = absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state);
                    if (result != ZMK_INPUT_PROC_CONTINUE || event.code != codes[button] || !event.sync) {
                        missing[button]++;
                    }
                }
            }
            /* A press discarded by a mid-event reset did not reach downstream. */
            struct absolute_to_relative_data data = {0};
            const struct absolute_to_relative_config config = {.suppression = {false, false}};
            valid_device = (struct device){.data = &data, .config = &config};
            assert(absolute_to_relative_init(&valid_device) == 0);
            struct zmk_input_processor_state state = {.input_device_index = listener};
            watched_generation = &data.reset_generation;
            generation_reads = 0;
            jump_on_read = 2;
            struct input_event event = {.type = INPUT_EV_KEY, .code = codes[button],
                                        .value = 1, .sync = true};
            assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                   ZMK_INPUT_PROC_STOP);
            watched_generation = NULL;
            const struct absolute_to_relative_suppression flags = {true, true};
            assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
            event = (struct input_event){.type = INPUT_EV_KEY, .code = codes[button], .value = 1};
            assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                   ZMK_INPUT_PROC_STOP);
            event = (struct input_event){.type = INPUT_EV_KEY, .code = codes[button], .value = 0};
            assert(absolute_to_relative_handle_event(&valid_device, &event, 0, 0, &state) ==
                   ZMK_INPUT_PROC_STOP);
            assert(event.code == COORD_INVALID_ZERO && !event.sync);
        }
    }
    valid_device = saved_device;
    fprintf(stderr, "repeat release losses: touch=%u btn0=%u (24 cases each)\n", missing[0], missing[1]);
    assert(missing[0] == 0 && missing[1] == 0);
    puts("forwarded releases survive repeat, policy and generation changes: PASS");
}

static void overlap_edge(struct zmk_input_processor_state *state, int code,
                         int32_t value, int expected) {
    struct input_event event = {.type = INPUT_EV_KEY, .code = code, .value = value,
                                .sync = code != INPUT_BTN_TOUCH};
    int result = absolute_to_relative_handle_event(&valid_device, &event, 0, 0, state);
    assert(result == expected && "overlap button release ownership");
    assert(event.code == (expected == ZMK_INPUT_PROC_CONTINUE ? code : COORD_INVALID_ZERO));
    assert(event.sync == (expected == ZMK_INPUT_PROC_CONTINUE));
}

static void test_overlapping_buttons(void) {
    const struct device saved = valid_device;
    const struct absolute_to_relative_config config = {.suppression = {false, false}};
    const struct absolute_to_relative_suppression flags = {true, true};
    const int codes[] = {INPUT_BTN_TOUCH, INPUT_BTN_0};
    unsigned cases = 0;
    for (size_t listener = 0; listener < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; listener++) {
        struct zmk_input_processor_state state = {.input_device_index = listener};
        for (size_t first = 0; first < ARRAY_SIZE(codes); first++) {
            const size_t other = 1 - first;
            /* One forwarded button and one fully consumed pair, either release order. */
            for (unsigned order = 0; order < 2; order++) {
                struct absolute_to_relative_data data = {0};
                valid_device = (struct device){.data = &data, .config = &config};
                assert(absolute_to_relative_init(&valid_device) == 0);
                overlap_edge(&state, codes[first], 1, ZMK_INPUT_PROC_CONTINUE);
                assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
                overlap_edge(&state, codes[other], 1, ZMK_INPUT_PROC_STOP);
                if (order == 0) {
                    overlap_edge(&state, codes[other], 0, ZMK_INPUT_PROC_STOP);
                    overlap_edge(&state, codes[first], 0, ZMK_INPUT_PROC_CONTINUE);
                } else {
                    overlap_edge(&state, codes[first], 0, ZMK_INPUT_PROC_CONTINUE);
                    overlap_edge(&state, codes[other], 0, ZMK_INPUT_PROC_STOP);
                }
                cases++;
            }
            /* Both presses forwarded: retiring one release must keep the other. */
            for (unsigned reset = 0; reset < 3; reset++) {
                struct absolute_to_relative_data data = {0};
                valid_device = (struct device){.data = &data, .config = &config};
                assert(absolute_to_relative_init(&valid_device) == 0);
                overlap_edge(&state, codes[first], 1, ZMK_INPUT_PROC_CONTINUE);
                overlap_edge(&state, codes[other], 1, ZMK_INPUT_PROC_CONTINUE);
                assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
                overlap_edge(&state, codes[first], 2, ZMK_INPUT_PROC_STOP);
                overlap_edge(&state, codes[other], 2, ZMK_INPUT_PROC_STOP);
                if (reset == 0) {
                    assert(absolute_to_relative_layer_listener(NULL) == ZMK_EV_EVENT_BUBBLE);
                } else if (reset == 1) {
                    watched_generation = &data.reset_generation;
                    generation_reads = 0; jump_on_read = 2;
                }
                overlap_edge(&state, codes[first], 0, ZMK_INPUT_PROC_CONTINUE);
                watched_generation = NULL;
                if (reset == 2) {
                    watched_generation = &data.reset_generation;
                    generation_reads = 0; jump_on_read = 2;
                }
                overlap_edge(&state, codes[other], 2, ZMK_INPUT_PROC_STOP);
                watched_generation = NULL;
                overlap_edge(&state, codes[other], 0, ZMK_INPUT_PROC_CONTINUE);
                /* Its observed forwarded press is retired after that release. */
                overlap_edge(&state, codes[other], 1, ZMK_INPUT_PROC_STOP);
                overlap_edge(&state, codes[other], 0, ZMK_INPUT_PROC_STOP);
                cases++;
            }
        }
    }
    /* Same button on two active listeners has independent ownership as well. */
    for (size_t first = 0; first < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; first++) {
        for (size_t button = 0; button < ARRAY_SIZE(codes); button++) {
            struct absolute_to_relative_data data = {0};
            valid_device = (struct device){.data = &data, .config = &config};
            assert(absolute_to_relative_init(&valid_device) == 0);
            struct zmk_input_processor_state forwarded = {.input_device_index = first};
            struct zmk_input_processor_state consumed = {.input_device_index = 1 - first};
            overlap_edge(&forwarded, codes[button], 1, ZMK_INPUT_PROC_CONTINUE);
            assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
            overlap_edge(&consumed, codes[button], 1, ZMK_INPUT_PROC_STOP);
            overlap_edge(&consumed, codes[button], 0, ZMK_INPUT_PROC_STOP);
            overlap_edge(&forwarded, codes[button], 0, ZMK_INPUT_PROC_CONTINUE);
            cases++;
        }
    }
    assert(cases == 24);
    valid_device = saved;
    puts("overlapping button/listener release ownership: 24 cases PASS");
}

int main(void) {
#ifdef ABS2REL_TEST_OVERLAP_ONLY
    test_overlapping_buttons();
    return 0;
#endif
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
    /* Finish the forwarded contact before checking a fully consumed pair. */
    event = (struct input_event){.value = 0, .sync = false};
    assert(handle_touch_button(&event, &data, stream) == ZMK_INPUT_PROC_CONTINUE);
    assert(event.sync);
    data.suppression_flags = BIT(SUPPRESS_BTN_TOUCH_BIT);
    /* A consumed release must be paired with a press consumed here. The live
     * policy-change case now has its own forwarded-release regression below. */
    event = (struct input_event){.value = 1, .sync = true};
    assert(handle_touch_button(&event, &data, stream) == ZMK_INPUT_PROC_STOP);
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

    /* The coordinate reset may discard new motion or presses, but must keep
     * an already forwarded button's release, independently for each stream. */
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        state.input_device_index = i;
        data.suppression_flags = 0;
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_0,
                                     .value = 1, .sync = true};
        assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        watched_generation = &data.reset_generation;
        generation_reads = 0;
        jump_on_read = 2;
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_0,
                                     .value = 0, .sync = true};
        assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_CONTINUE);
        assert(event.code == INPUT_BTN_0 && event.sync);
        watched_generation = NULL;

        data.suppression_flags = BIT(SUPPRESS_BTN0_BIT);
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_0,
                                     .value = 1, .sync = true};
        assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_STOP);
        watched_generation = &data.reset_generation;
        generation_reads = 0;
        event = (struct input_event){.type = INPUT_EV_KEY, .code = INPUT_BTN_0,
                                     .value = 0, .sync = true};
        assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) ==
               ZMK_INPUT_PROC_STOP);
        assert(event.code == COORD_INVALID_ZERO && !event.sync);
        watched_generation = NULL;
    }
    state.input_device_index = 0;

    /* Passing an unrelated motion event is still forbidden across a reset. */
    watched_generation = &data.reset_generation;
    generation_reads = 0;
    event = (struct input_event){.type = INPUT_EV_REL, .code = INPUT_REL_X,
                                 .value = 3, .sync = true};
    assert(absolute_to_relative_handle_event(&dev, &event, 0, 0, &state) == ZMK_INPUT_PROC_STOP);
    assert(event.code == COORD_INVALID_ZERO && !event.sync);
    watched_generation = NULL;

    struct absolute_to_relative_suppression readback = {0};
    struct absolute_to_relative_config config = {.suppression = {.btn_touch = true, .btn0 = false}};
    valid_device = (struct device){.data = &data, .config = &config, .name = "private-runtime-label"};
    struct device other_device = {.data = &data};
    assert(absolute_to_relative_get_suppression(NULL, &readback) == -EINVAL);
    assert(absolute_to_relative_get_suppression(&valid_device, NULL) == -EINVAL);
    assert(absolute_to_relative_get_suppression(&other_device, &readback) == -ENODEV);
    assert(absolute_to_relative_set_suppression(NULL, &flags) == -EINVAL);
    assert(absolute_to_relative_set_suppression(&valid_device, NULL) == -EINVAL);
    assert(absolute_to_relative_set_suppression(&other_device, &flags) == -ENODEV);
    assert(info_count == 0); /* Rejected requests must not emit state changes. */
    /* Init resets every stream, whatever it held before. */
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        data.streams[i].btn_touch_press_suppressed = true;
        data.streams[i].btn0_press_suppressed = true;
        data.streams[i].btn_touch_press_forwarded = true;
        data.streams[i].btn0_press_forwarded = true;
        data.streams[i].applied_generation = 7;
        data.streams[i].x.initialized = data.streams[i].y.initialized = true;
    }
    assert(absolute_to_relative_init(&valid_device) == 0);
    assert(info_count == 1 && strstr(last_info, valid_device.name) == NULL);
    for (size_t i = 0; i < ABSOLUTE_TO_RELATIVE_STREAM_COUNT; i++) {
        assert(!data.streams[i].btn_touch_press_suppressed &&
               !data.streams[i].btn0_press_suppressed && data.streams[i].applied_generation == 0);
        assert(!data.streams[i].btn_touch_press_forwarded && !data.streams[i].btn0_press_forwarded);
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
    /* Useful state logs contain flags, never a runtime device label. */
    assert(info_count == 2);
    assert(strstr(last_info, valid_device.name) == NULL);
    assert(strstr(last_info, "btn_touch=0") && strstr(last_info, "btn0=1"));
    /* Reapplying identical settings is not another state transition. */
    assert(absolute_to_relative_set_suppression(&valid_device, &flags) == 0);
    assert(info_count == 2);
    assert(absolute_to_relative_get_suppression(&valid_device, &readback) == 0);
    assert(!readback.btn_touch && readback.btn0);
    assert(absolute_to_relative_layer_listener(NULL) == ZMK_EV_EVENT_BUBBLE);
    /* Every device in the list counts, the first as much as the last. */
    assert(absolute_to_relative_device_valid(&first_device));
    assert(data.reset_generation == 1);
    test_touch_release_pairing();
    test_repeat_preserves_forwarded_release();
    test_overlapping_buttons();
    puts("absolute-to-relative driver event helpers: PASS");
    return 0;
}
