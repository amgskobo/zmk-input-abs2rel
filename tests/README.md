# Converter owner checks

abs2rel owns coordinate references, signed/saturated deltas, two-sample rounding,
source-local touch resets and layer-generation resync. Original buttons and sync
pass unchanged. It does not own suppression, HID button claims, frame completion,
runtime settings or their compatibility/persistence.

```sh
bash tests/run-docker.sh
ZMK_TEST_MODE=full bash tests/run-docker.sh
# In the selected build image, without fetching:
ZMK_TEST_OUTPUT=/fresh/native bash tests/run-native.sh /resolved/workspace
ZMK_TEST_OFFLINE_WORKSPACE=/resolved/workspace ZMK_TEST_OUTPUT=/fresh/integration \
  bash /src/tests/integration/run.sh upstream # or dya
```

| Boundary | Fixture and assertions |
| --- | --- |
| Arithmetic | test_absolute_to_relative_core.c: first reference, reset, symmetric rounding, saturation across full int32 range and independent axes |
| Complete production driver | runtime/driver_harness.c includes the unchanged driver file: actual structs, API and DT instance; independent sources/axes, source-local touch reset, layer/reset generations, transparent source events, unknown/invalid input, cold init and sync |
| Real core events | native/module/main.c: actual driver API with real ZMK layer dispatch, source/axis independence, ordered samples and transparent button values; same binary3 times on stock/DYA |
| Standard nodes/storage | check_converter.py: standard include has one active instance; explicit native legacy opt-in has independent histories; allocated state count/bytes come from the actual linker map |
| Build registration | integration/run.sh: stock/DYA ARM builds, enabled converter and absent legacy policy registration, then native fixture |
| Test admission | test_runtime_coverage.py: actual full entry rejects empty/unrelated/partial reports, unexecuted functions/lines/branches and invalid mode/old selectors |
| Offline safety | integration/test_offline.py: exact supported pins/clean source, missing inputs, nonoverlapping project paths, safe/fresh outputs including symlink aliases, and rejection of unowned settings-fixture selection |

Quick runs every host behavioral scenario once under ASan/UBSan. Full adds
optimized and measured coverage variants. Arithmetic has its own complete
line/branch gate; the runtime entry independently gates every real driver
function, executable line and branch. No production definitions or structs are
lifted/recreated. Emitted-source selectors and settings-fixture scaffolding are
retired. Their former internal interfaces are not part of the converter contract.
The KEY generation probe asserts that a real generation increment occurred.
Generation changes during the initial and final snapshots have distinct cases.

Read AGENTS and actual dependency instructions before execution. Supported pins:
stock5b51501fead672c41b5cfb396f3dafe0894bf4e9, DYA
e5c9b6915b56801193e359dd9bad4a167ce0d1b8, Zephyr
10ba6d0cb38bc3d258775d27982f707599320085. The offline entry verifies these and
CMSIS/HAL/nanopb/Studio-message pins, prepares an isolated unchanged local core
clone and source links, and never fetches or patches dependencies. DYA now tests
the same pure-converter contract; it has no Custom Settings fixture dependency.
This does not forbid a real consuming firmware from enabling unrelated settings
providers. Run all3 consuming production roles for compiler/integration changes.

Every output path must be fresh. Native rejects existing/linked paths or logs;
the offline preparer also rejects outputs inside selected sources and linked
projects before writing. Test commands retain raw order/timestamps; compile,
controlled host races and native dispatch do not establish physical USB/BLE/split,
sensor, latency/power or missing-source recovery. See maintenance.json and
contract.json for current ownership.

## Ownership and compatibility

The standard include enables one converter. The optional legacy scroll instance
is disabled by default; native explicitly enables it and checks independent
histories. check_converter.py checks actual active instances and linker storage.
No coordinate/reference/generation state was removed during suppression extraction.

Former source-button pair/repeat/overlap and pending-frame assertions belong to
zmk-input-suppress host driver.c and its actual listener/HID fixtures. The old
profile/settings/window/layer-watcher interfaces are retired. Existing suppression
values remain stored but unapplied. No filter/settings algorithm is duplicated here.
Prior fixtures, assertion maps and first failures are externally archived by the
consuming integration owner; no failed snapshot was accepted. Internal exported
source selectors are removed and rejected; output freshness guards remain active.

Retired simulation cases route-switch and split-contacts retain calculation/state
assertions above. Their physical split/HID/LED/battery timing assertions and old
snapshot failures remain unvalidated; host/native/compile results do not replace
them. Source loss, arbitrary interleavings, sensor/USB/BLE/split delivery, latency,
power and physical acceptance remain separate gaps.

## Selected workspace admission

The owner-local `integration/workspace-env.sh` derives `ZEPHYR_BASE`,
`Zephyr_DIR` and a nonempty `ZEPHYR_MODULES` from the selected workspace.
Discovery failure and empty module lists stop before compilation; ambient cache
modules cannot supply missing dependencies. The canonical host entry runs
`integration/test_workspace_env.py`, including failed-discovery/empty-list
regressions and a symlinked cache with paths containing spaces. The native entry
uses the same selection helper. This validates dependency selection, not hardware.
