/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../../drivers/input/input_processor_absolute_to_relative.c"

#define EXPECTED_CAPACITY \
    (CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES ? CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES : \
     MAX(ABS2REL_TEST_LISTENERS, 1))

int main(void) {
    struct absolute_to_relative_data *data = device.data;
    assert(absolute_to_relative_init(&device) == 0);
    assert(ABSOLUTE_TO_RELATIVE_STREAM_COUNT == EXPECTED_CAPACITY);
    assert(stream_for_event(data, NULL) == &data->streams[0]);
    for (unsigned source = 0; source < 256; source++) {
        struct zmk_input_processor_state state = {.input_device_index = (uint8_t)source};
        struct absolute_to_relative_stream *stream = stream_for_event(data, &state);
        assert(stream == (source < EXPECTED_CAPACITY ? &data->streams[source] : NULL));
        struct input_event event = {INPUT_EV_ABS, INPUT_ABS_X, (int32_t)source * 100, true};
        assert(absolute_to_relative_handle_event(&device, &event, 0, 0, &state) == 0);
        if (source < EXPECTED_CAPACITY) {
            assert(event.type == INPUT_EV_MSC && event.code == INPUT_MSC_SCAN &&
                   event.value == 0 && event.sync);
            event = (struct input_event){INPUT_EV_ABS, INPUT_ABS_X,
                                         (int32_t)source * 100 + 10, true};
            assert(absolute_to_relative_handle_event(&device, &event, 0, 0, &state) == 0);
            assert(event.type == INPUT_EV_REL && event.code == INPUT_REL_X &&
                   event.value == 5 && event.sync);
        } else {
            assert(event.type == INPUT_EV_ABS && event.code == INPUT_ABS_X &&
                   event.value == (int32_t)source * 100 && event.sync);
        }
    }
    /* Every source retains its own coordinate even after all other sources ran. */
    for (unsigned source = 0; source < EXPECTED_CAPACITY; source++) {
        assert(data->streams[source].x.previous_position == (int32_t)source * 100 + 10);
        assert(data->streams[source].x.previous_delta == 10);
        assert(data->streams[source].x.initialized);
        assert(!data->streams[source].y.initialized);
    }
    struct input_event event = {INPUT_EV_ABS, INPUT_ABS_X, 20, true};
    assert(absolute_to_relative_handle_event(&device, &event, 0, 0, NULL) == 0);
    assert(event.type == INPUT_EV_REL && event.value == 10 && event.sync);
    assert(absolute_to_relative_layer_listener(NULL) == 0);
    for (unsigned source = 0; source < EXPECTED_CAPACITY; source++) {
        struct zmk_input_processor_state state = {.input_device_index = (uint8_t)source};
        event = (struct input_event){INPUT_EV_ABS, INPUT_ABS_X, 1000, true};
        assert(absolute_to_relative_handle_event(&device, &event, 0, 0, &state) == 0);
        assert(event.type == INPUT_EV_MSC && event.value == 0 && event.sync);
    }
    printf("source capacity: PASS configured=%u listeners=%u capacity=%u lookups=256 "
           "stream_bytes=%zu data_bytes=%zu\n", CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES,
           ABS2REL_TEST_LISTENERS, EXPECTED_CAPACITY, sizeof(data->streams[0]), sizeof(*data));
    return 0;
}
