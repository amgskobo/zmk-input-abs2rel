/* Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ZMK_INPUT_ABS2REL_SUBSYSTEM "amgskobo__a2r"

enum zmk_custom_setting_value_type {
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32 = 1,
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,
};
struct zmk_custom_setting_value {
    enum zmk_custom_setting_value_type type;
    union {
        int32_t int32_value;
        bool bool_value;
    };
};
struct zmk_custom_setting {
    const char *custom_subsystem_id;
    const char *key;
    int read_result;
    struct zmk_custom_setting_value value;
};
typedef struct { int unused; } zmk_custom_CallRequest;
typedef struct { int unused; } pb_callback_t;

#define ARG_UNUSED(x) ((void)(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int errors;
#define LOG_ERR(fmt, ...) ((void)errors++, (void)sizeof(printf(fmt, ##__VA_ARGS__)))

static struct zmk_custom_setting *settings;
static size_t settings_len;
#define ZMK_CUSTOM_SETTING_FOREACH(_var)                                                           \
    for (struct zmk_custom_setting *_var = settings; _var < settings + settings_len; _var++)

static int zmk_custom_setting_read(const struct zmk_custom_setting *setting,
                                   struct zmk_custom_setting_value *value) {
    *value = setting->value;
    return setting->read_result;
}

/* The apply step: one devicetree instance and its two switches. */
#include <zmk-input-abs2rel/absolute_to_relative.h>
#define ZMK_EV_EVENT_BUBBLE 0
#define DT_INST_FOREACH_STATUS_OKAY(fn) fn(0)
#define DEVICE_DT_INST_GET(n) (&instance)
typedef struct { int kind; } zmk_event_t;
static struct device instance;
static struct zmk_custom_setting absolute_to_relative_cs_suppress_btn_touch_0 = {
    .value = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL, .bool_value = true}};
static struct zmk_custom_setting absolute_to_relative_cs_suppress_btn0_0 = {
    .value = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL, .bool_value = false}};
static int applies;
static struct absolute_to_relative_suppression applied;

int absolute_to_relative_set_suppression(const struct device *dev,
                                         const struct absolute_to_relative_suppression *flags) {
    assert(dev == &instance);
    applies++;
    applied = *flags;
    return 0;
}

/* DRIVER_FUNCTIONS */

static struct zmk_custom_setting int32_setting(int32_t value) {
    return (struct zmk_custom_setting){
        .value = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32, .int32_value = value}};
}

static struct zmk_custom_setting bool_setting(bool value) {
    return (struct zmk_custom_setting){
        .value = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL, .bool_value = value}};
}

static void test_readers(void) {
    bool flag = false;

    struct zmk_custom_setting setting = bool_setting(true);
    assert(read_flag(&setting, &flag) && flag);
    setting.read_result = -2;
    flag = false;
    assert(!read_flag(&setting, &flag) && !flag);
    setting = int32_setting(1);
    assert(!read_flag(&setting, &flag) && !flag);
}

static void test_unique_keys(void) {
    struct zmk_custom_setting list[] = {
        {.custom_subsystem_id = "other", .key = "pad.width"},
        {.custom_subsystem_id = ZMK_INPUT_ABS2REL_SUBSYSTEM, .key = "pad.width"},
        {.custom_subsystem_id = "other", .key = "pad.layer"},
        {.custom_subsystem_id = ZMK_INPUT_ABS2REL_SUBSYSTEM, .key = "pad.layer"},
        /* Another subsystem reusing a key after ours is not a duplicate. */
        {.custom_subsystem_id = "other", .key = "pad.layer"},
        {.custom_subsystem_id = ZMK_INPUT_ABS2REL_SUBSYSTEM, .key = "pad.width"},
    };
    settings = list;

    settings_len = ARRAY_SIZE(list);
    assert(abs2rel_check_unique_keys() == 0);
    assert(errors == 1);

    errors = 0;
    settings_len = ARRAY_SIZE(list) - 1;
    assert(abs2rel_check_unique_keys() == 0);
    assert(errors == 0);
}

/* Both switches are applied together, or neither is. */
static void test_apply(void) {
    const zmk_event_t changed = {1};
    assert(absolute_to_relative_settings_event_cb(&changed) == ZMK_EV_EVENT_BUBBLE);
    assert(applies == 1 && applied.btn_touch && !applied.btn0);
    absolute_to_relative_cs_suppress_btn_touch_0.read_result = -2;
    absolute_to_relative_apply_settings();
    absolute_to_relative_cs_suppress_btn_touch_0.read_result = 0;
    absolute_to_relative_cs_suppress_btn0_0.read_result = -2;
    absolute_to_relative_apply_settings();
    assert(applies == 1);
}

int main(void) {
    assert(!abs2rel_namespace_handler(NULL, NULL));
    test_readers();
    test_unique_keys();
    test_apply();
    puts("abs2rel custom settings helpers: PASS");
    return 0;
}
