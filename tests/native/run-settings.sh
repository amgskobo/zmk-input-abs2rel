#!/usr/bin/env bash
# Copyright (c) 2026 amgskobo
# SPDX-License-Identifier: MIT
set -euo pipefail
workspace="${1:?provide a resolved west workspace}"
output="${2:?provide a fresh absolute output}"
mode="${3:-normal}"
case "$mode" in normal) bypass=OFF ;; skip-lock) bypass=ON ;; *) echo 'mode must be normal or skip-lock' >&2; exit 2 ;; esac
case "$output" in /*) ;; *) echo 'output must be absolute' >&2; exit 2 ;; esac
if [ -e "$output" ] || [ -L "$output" ]; then echo 'output must be fresh' >&2; exit 2; fi
root="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
zephyr="$(cd -- "$workspace/zephyr" && pwd)"
revision="$(git -C "$zephyr" rev-parse HEAD)"
if [ "$revision" != 10ba6d0cb38bc3d258775d27982f707599320085 ] || [ -n "$(git -C "$zephyr" status --porcelain)" ]; then
  echo 'the unchanged reviewed Zephyr revision is required' >&2; exit 2
fi
mkdir -p -- "$(dirname -- "$output")"
mkdir -- "$output"
printf 'Zephyr %s %s\n' "$zephyr" "$revision" >"$output/dependencies.txt"
sha256sum "$root/drivers/input/input_processor_absolute_to_relative_custom_settings.c" "$root/tests/runtime/custom_settings_harness.c" "$root/tests/runtime/run.py" >"$output/source-sha256.txt"
python3 "$root/tests/runtime/run.py" --emit-custom-settings "$output/settings.c"
export ZEPHYR_BASE="$zephyr"
cmake -S "$root/tests/native/settings" -B "$output/build" -GNinja \
  -DBOARD=native_sim/native/64 -DWEST=OFF -DZEPHYR_MODULES= \
  -DABS2REL_SETTINGS_BYPASS_LOCK="$bypass" -DABS2REL_SETTINGS_FIXTURE="$output/settings.c" >"$output/configure.log" 2>&1
cmake --build "$output/build" --parallel 2 >"$output/build.log" 2>&1
for repeat in 1 2 3; do
  log="$output/run.$repeat.log"
  if timeout 30 "$output/build/zephyr/zephyr.exe" >"$log" 2>&1; then status=0; else status=$?; fi
  printf '%s\n' "$status" >"$output/run.$repeat.exit"
  if [ "$mode" = normal ]; then
    test "$status" = 0
    grep -Fq 'abs2rel newer registry callback wins: PASS' "$log"
    grep -Fq 'abs2rel custom settings helpers: PASS' "$log"
  else
    test "$status" != 0 && test "$status" != 124
    grep -Fq 'applied_touch=1' "$log"
    grep -Fq '!applied.btn_touch' "$log"
  fi
  echo "PASS: abs2rel real-kernel settings $mode repeat $repeat"
done
