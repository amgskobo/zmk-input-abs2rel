/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>

static const struct device *const converter = DEVICE_DT_GET(DT_NODELABEL(zip_absolute_to_relative));
enum {
    capacity = CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES != 0
                   ? CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES
                   : MAX(DT_NUM_INST_STATUS_OKAY(zmk_input_listener), 1),
};

static void sources(void *a, void *b, void *c) {
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    __ASSERT_NO_MSG(device_is_ready(converter));
    for (unsigned source = 0; source < 256; source++) {
        struct zmk_input_processor_state state = {.input_device_index = (uint8_t)source};
        struct input_event event = {.type = INPUT_EV_ABS, .code = INPUT_ABS_X,
                                   .value = (int32_t)source * 100, .sync = true};
        __ASSERT_NO_MSG(zmk_input_processor_handle_event(converter, &event, 0, 0, &state) == 0);
        __ASSERT_NO_MSG(event.type == (source < capacity ? INPUT_EV_MSC : INPUT_EV_ABS));
        __ASSERT_NO_MSG(event.code == (source < capacity ? INPUT_MSC_SCAN : INPUT_ABS_X));
        __ASSERT_NO_MSG(event.value == (source < capacity ? 0 : (int32_t)source * 100) && event.sync);
    }
    /* Reading every stored reference after all first samples detects aliases. */
    for (unsigned source = 0; source < capacity; source++) {
        struct zmk_input_processor_state state = {.input_device_index = (uint8_t)source};
        struct input_event event = {.type = INPUT_EV_ABS, .code = INPUT_ABS_X,
                                   .value = (int32_t)source * 100 + 10, .sync = true};
        __ASSERT_NO_MSG(zmk_input_processor_handle_event(converter, &event, 0, 0, &state) == 0);
        __ASSERT_NO_MSG(event.type == INPUT_EV_REL && event.code == INPUT_REL_X &&
                        event.value == 5 && event.sync);
    }
    struct input_event event = {.type = INPUT_EV_ABS, .code = INPUT_ABS_X, .value = 20, .sync = true};
    __ASSERT_NO_MSG(zmk_input_processor_handle_event(converter, &event, 0, 0, NULL) == 0);
    __ASSERT_NO_MSG(event.type == INPUT_EV_REL && event.value == 10 && event.sync);
    __ASSERT_NO_MSG(zmk_keymap_layer_activate(1, false) >= 0);
    for (unsigned source = 0; source < capacity; source++) {
        struct zmk_input_processor_state state = {.input_device_index = (uint8_t)source};
        event = (struct input_event){.type = INPUT_EV_ABS, .code = INPUT_ABS_X,
                                     .value = 5000, .sync = true};
        __ASSERT_NO_MSG(zmk_input_processor_handle_event(converter, &event, 0, 0, &state) == 0);
        __ASSERT_NO_MSG(event.type == INPUT_EV_MSC && event.value == 0 && event.sync);
    }
    printk("abs2rel source limits: PASS capacity=%u listeners=%u lookups=256 NULL=0\n",
           capacity, DT_NUM_INST_STATUS_OKAY(zmk_input_listener));
    exit(0);
}
K_THREAD_DEFINE(abs2rel_source_test, 4096, sources, NULL, NULL, NULL, 0, 0, 100);
