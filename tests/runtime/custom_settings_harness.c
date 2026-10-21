/* Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef ABS2REL_SETTINGS_NATIVE
#include <zephyr/kernel.h>
#endif
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

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) ((void)(x))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static int errors;
static char last_error[256];
static void record_error(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void record_error(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(last_error, sizeof(last_error), format, arguments);
    va_end(arguments);
    errors++;
}
#define LOG_ERR(...) record_error(__VA_ARGS__)

static struct zmk_custom_setting *settings;
static size_t settings_len;
#define ZMK_CUSTOM_SETTING_FOREACH(_var)                                                           \
    for (struct zmk_custom_setting *_var = settings; _var < settings + settings_len; _var++)

/* Controlled callback ordering with either host mutexes or the real kernel. */
static bool pause_old_read, old_copied, newer_done, newer_contended, release_old;
static const struct zmk_custom_setting *watched_setting;

#ifdef ABS2REL_SETTINGS_NATIVE
static K_MUTEX_DEFINE(control_mutex);
static K_CONDVAR_DEFINE(control_changed);
K_MUTEX_DEFINE(abs2rel_settings_apply_mutex);
static void gate_lock(void) { assert(k_mutex_lock(&control_mutex, K_FOREVER) == 0); }
static void gate_unlock(void) { assert(k_mutex_unlock(&control_mutex) == 0); }
static void gate_notify(void) { k_condvar_broadcast(&control_changed); }
static void gate_wait(const bool *first, const bool *second) {
    while (!*first && (second == NULL || !*second)) {
        assert(k_condvar_wait(&control_changed, &control_mutex, K_SECONDS(5)) == 0);
    }
}
static int observed_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout) {
#ifdef ABS2REL_SETTINGS_BYPASS_LOCK
    ARG_UNUSED(mutex); ARG_UNUSED(timeout);
    return 0;
#else
    int rc = k_mutex_lock(mutex, K_NO_WAIT);
    if (rc != 0) {
        gate_lock();
        newer_contended = true;
        gate_notify();
        gate_unlock();
        rc = k_mutex_lock(mutex, timeout);
    }
    assert(rc == 0);
    return rc;
#endif
}
static int observed_mutex_unlock(struct k_mutex *mutex) {
#ifdef ABS2REL_SETTINGS_BYPASS_LOCK
    ARG_UNUSED(mutex);
    return 0;
#else
    return k_mutex_unlock(mutex);
#endif
}
#define k_mutex_lock observed_mutex_lock
#define k_mutex_unlock observed_mutex_unlock
#else
#include <errno.h>
#include <pthread.h>
#include <time.h>
static pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t control_changed = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t abs2rel_settings_apply_mutex __attribute__((unused)) = PTHREAD_MUTEX_INITIALIZER;
#define K_FOREVER (-1)
static void gate_lock(void) { assert(pthread_mutex_lock(&control_mutex) == 0); }
static void gate_unlock(void) { assert(pthread_mutex_unlock(&control_mutex) == 0); }
static void gate_notify(void) { assert(pthread_cond_broadcast(&control_changed) == 0); }
static void gate_wait(const bool *first, const bool *second) {
    struct timespec deadline;
    assert(timespec_get(&deadline, TIME_UTC) == TIME_UTC);
    deadline.tv_sec += 5;
    while (!*first && (second == NULL || !*second)) {
        assert(pthread_cond_timedwait(&control_changed, &control_mutex, &deadline) == 0);
    }
}
static __attribute__((unused)) int k_mutex_lock(pthread_mutex_t *mutex, int timeout) {
    assert(timeout == K_FOREVER);
    int rc = pthread_mutex_trylock(mutex);
    if (rc == EBUSY) {
        gate_lock();
        newer_contended = true;
        gate_notify();
        gate_unlock();
        rc = pthread_mutex_lock(mutex);
    }
    assert(rc == 0);
    return 0;
}
static __attribute__((unused)) int k_mutex_unlock(pthread_mutex_t *mutex) {
    assert(pthread_mutex_unlock(mutex) == 0);
    return 0;
}
#endif

static int zmk_custom_setting_read(const struct zmk_custom_setting *setting,
                                   struct zmk_custom_setting_value *value) {
    gate_lock();
    *value = setting->value;
    int result = setting->read_result;
    if (pause_old_read && setting == watched_setting && value->bool_value && !old_copied) {
        old_copied = true;
        gate_notify();
        gate_wait(&release_old, NULL);
    }
    gate_unlock();
    return result;
}

/* The apply step: one devicetree instance and its two switches. */
#include <zmk-input-abs2rel/absolute_to_relative.h>
#define ZMK_EV_EVENT_BUBBLE 0
#undef DT_INST_FOREACH_STATUS_OKAY
#define DT_INST_FOREACH_STATUS_OKAY(fn) fn(0)
#undef DEVICE_DT_INST_GET
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
    assert(strstr(last_error, "Duplicate setting key") != NULL);
    assert(strstr(last_error, "pad.") == NULL);

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

static void *older_callback(void *unused) {
    ARG_UNUSED(unused);
    absolute_to_relative_apply_settings();
    return NULL;
}
static void *newer_callback(void *unused) {
    ARG_UNUSED(unused);
    gate_lock();
    absolute_to_relative_cs_suppress_btn_touch_0.value.bool_value = false;
    absolute_to_relative_cs_suppress_btn0_0.value.bool_value = true;
    gate_unlock();
    absolute_to_relative_apply_settings();
    gate_lock();
    newer_done = true;
    gate_notify();
    gate_unlock();
    return NULL;
}
#ifdef ABS2REL_SETTINGS_NATIVE
static struct k_thread older_thread, newer_thread;
K_THREAD_STACK_DEFINE(older_stack, 2048);
K_THREAD_STACK_DEFINE(newer_stack, 2048);
static void older_entry(void *a, void *b, void *c) {
    ARG_UNUSED(b); ARG_UNUSED(c); (void)older_callback(a);
}
static void newer_entry(void *a, void *b, void *c) {
    ARG_UNUSED(b); ARG_UNUSED(c); (void)newer_callback(a);
}
#endif

static void test_newer_registry_callback_wins(void) {
    absolute_to_relative_cs_suppress_btn_touch_0 = bool_setting(true);
    absolute_to_relative_cs_suppress_btn0_0 = bool_setting(false);
    watched_setting = &absolute_to_relative_cs_suppress_btn_touch_0;
    pause_old_read = true;
    old_copied = newer_done = newer_contended = release_old = false;
#ifdef ABS2REL_SETTINGS_NATIVE
    assert(k_thread_create(&older_thread, older_stack, K_THREAD_STACK_SIZEOF(older_stack),
                           older_entry, NULL, NULL, NULL, 5, 0, K_NO_WAIT) != NULL);
#else
    pthread_t older, newer;
    assert(pthread_create(&older, NULL, older_callback, NULL) == 0);
#endif
    gate_lock();
    gate_wait(&old_copied, NULL);
    gate_unlock();
#ifdef ABS2REL_SETTINGS_NATIVE
    assert(k_thread_create(&newer_thread, newer_stack, K_THREAD_STACK_SIZEOF(newer_stack),
                           newer_entry, NULL, NULL, NULL, 5, 0, K_NO_WAIT) != NULL);
#else
    assert(pthread_create(&newer, NULL, newer_callback, NULL) == 0);
#endif
    gate_lock();
    gate_wait(&newer_done, &newer_contended);
    release_old = true;
    gate_notify();
    gate_unlock();
#ifdef ABS2REL_SETTINGS_NATIVE
    assert(k_thread_join(&older_thread, K_SECONDS(5)) == 0);
    assert(k_thread_join(&newer_thread, K_SECONDS(5)) == 0);
#else
    assert(pthread_join(older, NULL) == 0);
    assert(pthread_join(newer, NULL) == 0);
#endif
    pause_old_read = false;
    fprintf(stderr, "abs2rel settings registry_touch=%d applied_touch=%d applied_btn0=%d contended=%d\n",
            absolute_to_relative_cs_suppress_btn_touch_0.value.bool_value,
            applied.btn_touch, applied.btn0, newer_contended);
    assert(!absolute_to_relative_cs_suppress_btn_touch_0.value.bool_value &&
           !applied.btn_touch && applied.btn0);
    puts("abs2rel newer registry callback wins: PASS");
}

int main(void) {
    assert(!abs2rel_namespace_handler(NULL, NULL));
    test_readers();
    test_unique_keys();
    test_apply();
    test_newer_registry_callback_wins();
    puts("abs2rel custom settings helpers: PASS");
#ifdef ABS2REL_SETTINGS_NATIVE
    exit(0);
#else
    return 0;
#endif
}
