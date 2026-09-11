/*
 * Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 *
 * Publishes each converter's suppression flags through
 * zmk-feature-custom-settings.
 *
 * Both flags decide whether a pad's physical click reaches the host at all,
 * which is the kind of thing that wants trying rather than deciding: on a pad
 * that also carries tap-to-click, one setting is a duplicate button and the
 * other is a missing one, and which is which depends on the pad.
 *
 * This file owns persistence. The driver deliberately stores nothing, so
 * there is one owner for the value and no way for the two to disagree after a
 * reboot.
 */

#define DT_DRV_COMPAT zmk_input_processor_absolute_to_relative

#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <cormoran/zmk/custom_settings.h>
#include <zmk/event_manager.h>
#include <zmk/studio/custom.h>

#include <zmk-input-abs2rel/absolute_to_relative.h>
#include <zmk-input-abs2rel/custom_settings.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/*
 * Registers the namespace these parameters are published under.
 *
 * custom-settings resolves a setting's subsystem identifier to an index before
 * it can put the setting on the wire, and a setting whose subsystem was never
 * registered is dropped with -ENOENT however correctly it was defined.
 * Registration and definition are complementary, not alternatives.
 *
 * It lives in this file rather than one of its own because this module has a
 * single processor. A module with several -- zmk-input-processors -- collects
 * the registration and the check below into one module-level file instead, so
 * that they are written once rather than per processor.
 *
 * The subsystem answers no calls of its own: the values are read and written
 * through custom-settings' own RPC, which is what lets this processor appear
 * in a client that has no page for it. The advertised URL is this module's own
 * documentation, which is the only thing that explains what these settings do.
 */
static bool abs2rel_namespace_handler(const zmk_custom_CallRequest *request,
                                      pb_callback_t *encode_response);

static struct zmk_rpc_custom_subsystem_meta abs2rel_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("https://github.com/amgskobo/zmk-input-abs2rel"),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

/*
 * Through a wrapper so the token expands before it is stringified.
 *
 * ZMK_RPC_CUSTOM_SUBSYSTEM registers `#_identifier`, and `#` suppresses
 * expansion of its own argument, so passing the macro straight in would
 * register the literal text "ZMK_INPUT_ABS2REL_SUBSYSTEM_TOKEN". One
 * more layer of call expands it first.
 */
#define REGISTER_SUBSYSTEM(identifier, meta, handler)                                              \
    ZMK_RPC_CUSTOM_SUBSYSTEM(identifier, meta, handler)

REGISTER_SUBSYSTEM(ZMK_INPUT_ABS2REL_SUBSYSTEM_TOKEN, &abs2rel_meta, abs2rel_namespace_handler);

static bool abs2rel_namespace_handler(const zmk_custom_CallRequest *request,
                                      pb_callback_t *encode_response) {
    ARG_UNUSED(request);
    ARG_UNUSED(encode_response);

    return false;
}

#define ABSOLUTE_TO_RELATIVE_SETTING(n, field, key)                                                \
    ZMK_CUSTOM_SETTING_DEFINE_WITH_CONSTRAINTS(                                                    \
        absolute_to_relative_cs_##field##_##n, ZMK_INPUT_ABS2REL_SUBSYSTEM,                     \
        ZMK_INPUT_ABS2REL_SETTING_KEY(n, key),                                \
        ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,                                                        \
        ZMK_CUSTOM_SETTING_VALUE_BOOL(DT_INST_PROP_OR(n, field, false)),                           \
        ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC, ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,     \
        ZMK_CUSTOM_SETTING_PERMISSION_SECURE, ZMK_CUSTOM_SETTING_NO_CONSTRAINT);

/*
 * The key keeps the "suppress" that the devicetree property carries. Dropped,
 * the row reads btn0 = true where the value means the button is being taken
 * away, and a client has nothing to show that from: a key is the label, and a
 * label that hides a negation is worse than a long one.
 */
#define ABSOLUTE_TO_RELATIVE_SETTINGS(n)                                                           \
    ZMK_INPUT_ABS2REL_ASSERT_NAME_FITS(n, "suppress_btn_touch")                                 \
    ABSOLUTE_TO_RELATIVE_SETTING(n, suppress_btn_touch, "suppress_btn_touch")                      \
    ABSOLUTE_TO_RELATIVE_SETTING(n, suppress_btn0, "suppress_btn0")

DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_SETTINGS)

static bool read_flag(const struct zmk_custom_setting *setting, bool *out) {
    struct zmk_custom_setting_value value;

    if (zmk_custom_setting_read(setting, &value) != 0 ||
        value.type != ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL) {
        return false;
    }

    *out = value.bool_value;

    return true;
}

#define ABSOLUTE_TO_RELATIVE_APPLY(n)                                                              \
    {                                                                                              \
        struct absolute_to_relative_suppression flags;                                             \
                                                                                                   \
        if (read_flag(&absolute_to_relative_cs_suppress_btn_touch_##n, &flags.btn_touch) &&        \
            read_flag(&absolute_to_relative_cs_suppress_btn0_##n, &flags.btn0)) {                  \
            (void)absolute_to_relative_set_suppression(DEVICE_DT_INST_GET(n), &flags);             \
        }                                                                                          \
    }

static void absolute_to_relative_apply_settings(void) {
    DT_INST_FOREACH_STATUS_OKAY(ABSOLUTE_TO_RELATIVE_APPLY)
}

static int absolute_to_relative_settings_event_cb(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    /*
     * Both subscribed events mean the same thing here -- some stored value may
     * now differ from what the processor is running -- and re-reading every
     * instance is cheaper than working out which one moved.
     */
    absolute_to_relative_apply_settings();

    return ZMK_EV_EVENT_BUBBLE;
}

/*
 * Applied on two signals, and deliberately not from a SYS_INIT.
 *
 * zmk_custom_settings_initialized fires from the settings-subtree commit that
 * ends the boot settings_load pass, which is the only point at which a stored
 * value is both present and readable. A SYS_INIT is too early: it runs before
 * settings_load(), so it would read the devicetree default and leave the
 * processor on it for the rest of the session -- the value would persist and
 * show correctly in a client while having no effect on the hardware.
 *
 * The load path stores values without raising zmk_custom_setting_changed, so
 * that event alone would never deliver a stored value either. Together the two
 * cover boot and every later edit.
 *
 * In a build without CONFIG_SETTINGS nothing is stored and the event never
 * fires, which is correct: the driver already starts from its devicetree
 * values.
 */
ZMK_LISTENER(absolute_to_relative_custom_settings, absolute_to_relative_settings_event_cb);
ZMK_SUBSCRIPTION(absolute_to_relative_custom_settings, zmk_custom_setting_changed);
ZMK_SUBSCRIPTION(absolute_to_relative_custom_settings, zmk_custom_settings_initialized);

/*
 * Two nodes can still produce one key, and nothing downstream would say so.
 *
 * A key is the owning node's DT_NODE_FULL_NAME, which is the node's own name
 * and not its path, so devicetree keeps it unique only among its siblings. A
 * board that puts a converter under /input_processors and a module that puts
 * one at the root can pick the same name and neither Zephyr nor the settings
 * registry objects: zmk_custom_setting_find() returns the first match, so a
 * client's write always lands on whichever linked first while the second
 * silently keeps its devicetree values and appears in the list as though it
 * were being edited. A stored value restores into only one of them too.
 *
 * String equality across instances is not something the preprocessor can
 * evaluate, so the check runs once at startup and names the duplicated key. It
 * walks descriptors, not values, so it needs nothing from settings_load().
 */
static int abs2rel_check_unique_keys(void) {
    ZMK_CUSTOM_SETTING_FOREACH(setting) {
        if (strcmp(setting->custom_subsystem_id, ZMK_INPUT_ABS2REL_SUBSYSTEM) != 0) {
            continue;
        }

        ZMK_CUSTOM_SETTING_FOREACH(other) {
            if (other == setting) {
                /* Only report a pair once: stop at the first of the two. */
                break;
            }

            if (strcmp(other->custom_subsystem_id, ZMK_INPUT_ABS2REL_SUBSYSTEM) == 0 &&
                strcmp(other->key, setting->key) == 0) {
                LOG_ERR("Duplicate setting key \"%s\": two devicetree nodes share a "
                        "name, so only one of them is editable",
                        setting->key);
            }
        }
    }

    return 0;
}

SYS_INIT(abs2rel_check_unique_keys, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

