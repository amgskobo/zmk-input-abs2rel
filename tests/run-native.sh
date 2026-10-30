#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[ "$#" -eq 1 ] || { echo 'pass one resolved workspace' >&2; exit 2; }
workspace="$(cd "$1" && pwd)"
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${ZMK_TEST_OUTPUT:?fresh absolute output required}"
[[ "$out" = /* ]] && [ ! -e "$out" ] && [ ! -L "$out" ] && [ ! -e "$out.build.log" ] && [ ! -L "$out.build.log" ] || { echo 'fresh absolute output required' >&2; exit 2; }
mkdir -p "$(dirname "$out")"
cd "$workspace"
source "$root/tests/integration/workspace-env.sh"
zmk_test_select_workspace "$workspace"
west zephyr-export
west build -p=always -o=-j2 -s "$workspace/zmk/app" -d "$out" -b native_sim//zmk_test_mock -- \
 -DZMK_CONFIG="$root/tests/native/config" -DZMK_EXTRA_MODULES="$root;$root/tests/native/module" >"$out.build.log" 2>&1
for repeat in 1 2 3; do
 timeout 30 "$out/zephyr/zmk.exe" >"$out.run-$repeat.log" 2>&1
 grep -Fq 'abs2rel native contract: PASS' "$out.run-$repeat.log"
 cat "$out.run-$repeat.log"
done
