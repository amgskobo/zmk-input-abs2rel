#!/usr/bin/env bash
# Copyright (c) 2026 amgskobo
# SPDX-License-Identifier: MIT

set -euo pipefail

variant="${1:-upstream}"
case "$variant" in
upstream | dya) ;;
*)
    echo "unknown integration variant '$variant': expected upstream or dya" >&2
    exit 2
    ;;
esac

if [ -n "${ZMK_TEST_OFFLINE_WORKSPACE:-}" ]; then
    work_dir="${ZMK_TEST_OUTPUT:?set ZMK_TEST_OUTPUT to a fresh evidence directory}"
    workspace="$(python3 /src/tests/integration/offline.py "$ZMK_TEST_OFFLINE_WORKSPACE" "$variant" "/src/tests/integration/config/$variant/west.yml" "$work_dir")"
    cd "$workspace"
    west zephyr-export
else
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/zmk-input-abs2rel-integration.XXXXXX")"

cleanup() {
    rm -rf "$work_dir"
}
trap cleanup EXIT HUP INT TERM

cp -R "/src/tests/integration/config/$variant" "$work_dir/config"
cd "$work_dir"

west init -l config
west update --narrow --fetch-opt=--depth=1
west zephyr-export

workspace="$work_dir"
fi

export ZEPHYR_BASE="$workspace/zephyr"

cmake_args=()
if [ "$variant" = dya ]; then
    cmake_args+=("-DEXTRA_CONF_FILE=/src/tests/integration/custom-settings.conf")
fi

west build -s "$workspace/zmk/app" -d "$work_dir/build" -b xiao_ble/nrf52840/zmk -- \
    -DZMK_EXTRA_MODULES="/src;/src/tests/integration/firmware" \
    -DSHIELD=abs2rel_test \
    "${cmake_args[@]}"

test -f "$work_dir/build/zephyr/zmk.uf2"
grep -q '^CONFIG_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE=y' \
    "$work_dir/build/zephyr/.config"
if [ "$variant" = upstream ]; then
    if grep -q '^CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y' \
        "$work_dir/build/zephyr/.config"; then
        echo "custom settings unexpectedly enabled" >&2
        exit 1
    fi
else
    grep -q '^CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y' \
        "$work_dir/build/zephyr/.config"
    strings "$work_dir/build/zephyr/zmk.elf" >"$work_dir/build/zephyr/strings.txt"
    grep -Fxq amgskobo__a2r "$work_dir/build/zephyr/strings.txt"
    for instance in a2r_default abs_rel abs_rel_scroll; do
        for field in suppress_btn_touch suppress_btn0; do
            grep -Fxq "$instance.$field" "$work_dir/build/zephyr/strings.txt" || {
                echo "missing Studio setting: $instance.$field" >&2
                exit 1
            }
        done
    done
fi

echo "$variant ZMK firmware fixture: PASS"
