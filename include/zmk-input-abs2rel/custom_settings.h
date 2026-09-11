/*
 * Copyright (c) 2026 amgskobo
 * SPDX-License-Identifier: MIT
 *
 * The custom-settings namespace this module publishes under.
 *
 * A setting is dropped with -ENOENT unless its subsystem is registered, and a
 * client groups the list it renders by subsystem, so this is also the heading
 * a person reads. It is named for the module rather than for anything about
 * the processor, because the module is what the heading collects.
 */

#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define ZMK_INPUT_ABS2REL_SUBSYSTEM "amgskobo__abs2rel"

/*
 * A setting key is the owning node's devicetree name, then the field:
 *
 *     pointer_abs_rel.suppress_btn_touch
 *     scroll_abs_rel.suppress_btn0
 *
 * The node name is the one identifier both halves of the problem already hold.
 * A view drawing the chain walks devicetree for the processors in each
 * listener and gets a `const struct device *` per stage, whose ->name is
 * DEVICE_DT_NAME(), which is DT_NODE_FULL_NAME() -- the same string this
 * builds the key from. So a stage's settings are the ones whose key starts
 * with its device name, and nothing has to be registered, agreed between
 * modules, or typed into devicetree by a board author for that to hold.
 *
 * It removes the commonest way to collide, since a name is no longer written
 * by hand. It does not remove every way: DT_NODE_FULL_NAME is a node's own
 * name and not its path, so devicetree keeps it unique only among siblings,
 * and a board node and a module node under different parents can still pick
 * the same one. That is what the startup check in the settings file is for.
 */
#define ZMK_INPUT_ABS2REL_SETTING_KEY(n, field) DT_NODE_FULL_NAME(DT_DRV_INST(n)) "." field

/*
 * Fail by name when a node cannot fit a settings key.
 *
 * custom-settings already refuses a key over
 * CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN, but it can only say that some key
 * was too long: the key is built inside its own macro, so the message names
 * the settings file and not the devicetree node that caused it. Since the
 * length is the node's name plus the field, the node is the only thing anyone
 * can act on.
 *
 * The longest field here is "suppress_btn_touch" at 18, which leaves a node
 * 28 characters of the 47 a key has.
 */
#define ZMK_INPUT_ABS2REL_ASSERT_NAME_FITS(n, longest_field)                                       \
    BUILD_ASSERT(sizeof(ZMK_INPUT_ABS2REL_SETTING_KEY(n, longest_field)) <=                        \
                     CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN,                                       \
                 "devicetree node \"" DT_NODE_FULL_NAME(DT_DRV_INST(n))                            \
                 "\" has a name too long to key its settings; shorten the node name");
