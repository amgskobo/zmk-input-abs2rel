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

source /src/tests/integration/workspace-env.sh
zmk_test_select_workspace "$workspace"

west build -s "$workspace/zmk/app" -d "$work_dir/build" -b xiao_ble/nrf52840/zmk -- \
    -DZMK_EXTRA_MODULES="/src;/src/tests/integration/firmware" \
    -DSHIELD=abs2rel_test

test -f "$work_dir/build/zephyr/zmk.uf2"
grep -q '^CONFIG_ZMK_INPUT_PROCESSOR_ABSOLUTE_TO_RELATIVE=y' \
    "$work_dir/build/zephyr/.config"
strings "$work_dir/build/zephyr/zmk.elf" >"$work_dir/build/zephyr/strings.txt"
if grep -Fq 'amgskobo__a2r' "$work_dir/build/zephyr/strings.txt"; then
    echo "converter unexpectedly registered output policy" >&2
    exit 1
fi
python3 /src/tests/check_converter.py --dts "$work_dir/build/zephyr/zephyr.dts" \
    --zephyr "$workspace/zephyr" --map "$work_dir/build/zephyr/zmk.map" \
    --record "$work_dir/default-converter.json"
echo "PASS: pure converter $variant"

ZMK_TEST_OUTPUT="$work_dir/native" bash /src/tests/run-native.sh "$workspace"
python3 /src/tests/check_converter.py --dts "$work_dir/native/zephyr/zephyr.dts" \
    --zephyr "$workspace/zephyr" --map "$work_dir/native/zephyr/zmk.map" --legacy \
    --record "$work_dir/legacy-converter.json"
