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
# In each selected supported workspace, after its offline admission:
python3 /src/tests/source-limits/run.py /resolved/workspace /fresh/source-limits
# Match two ARM builds differing only in MAX_SOURCES=0 versus2, using the
# original unmodified owner snapshot as the baseline source:
python3 /src/tests/source-limits/measure_arm.py /arm/automatic /arm/limited \
  /original-owner /fresh/arm-measurement
```

| Boundary | Fixture and assertions |
| --- | --- |
| Arithmetic | test_absolute_to_relative_core.c: first reference, reset, symmetric rounding, saturation across full int32 range and independent axes |
| Complete production driver | runtime/driver_harness.c includes the unchanged driver file: actual structs, API and DT instance; independent sources/axes, source-local touch reset, layer/reset generations, transparent source events, unknown/invalid input, cold init and sync |
| Real core events | native/module/main.c: actual driver API with real ZMK layer dispatch, source/axis independence, ordered samples and transparent button values; same binary3 times on stock/DYA |
| Source capacity | runtime/source_limits_harness.c: all256 uint8 indices in automatic2/5, explicit1/2/256 and no-listener fallback profiles; independent retained coordinates, NULL, invalid pass-through and layer reset |
| Real source graph | source-limits/run.py: original sparse auto graph plus low auto/limited/wide/fallback and disabled-device/listener profiles; required base/disabled-child/sparse/Kconfig rejections; same native binary3 times per accepted profile |
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

## 32-bit arithmetic and clock contract

Coordinate subtraction uses checked 32-bit saturation. The two-sample average splits each operand into quotient/remainder and applies one signed truncation, preserving the full int32 coordinate contract without a wide sum.

test_absolute_to_relative_core.c retains full-range saturation and independent histories; the signed-average grid compares every small positive/negative pair with an independent wide reference.


## Optional source-index allocation

`CONFIG_ZMK_INPUT_ABS2REL_MAX_SOURCES=0` preserves automatic allocation for all
listeners, minimum one. Positive 1..256 reserves the original index prefix;
there is no remapping. Re-audit every actual callback and direct API caller.
The canonical source matrix fixes its original sparse automatic profile and its
new low automatic/limited profiles itself, so ambient `MAX_SOURCES` or
`LOW_SOURCES` values cannot silently replace the high-index coverage.
Seven configured native product profiles each run the same binary three times;
five negative profiles require their exact guard/Kconfig diagnostic. The final
profile additionally enables strict ASan and UBSan. Any upstream sanitizer failure
is reported BLOCKED with its original trace; it is not a product PASS.
`--control CASE` is an explicit focused control, not the complete matrix.
The whole-driver host entry retains every original assertion and adds six
capacity profiles, each checking all 256 possible uint8 source indices under
sanitizers (plus optimization and coverage in full mode).

The original native fixture keeps its direct-source 0/1 assertions and adds
ordered samples for its actual compiled callback indices (3/4 in the original
graph). Low overlays disable the unrelated mouse-key listeners and exercise
0/1 without changing the canonical sparse profile. Base, disabled child and
sparse references above the bound must fail; disabled listener/device cases
must build. `source-limits/apply_fault.py` creates named base/child/device guard
faults only in a distinct disposable owner copy. Required rejections must fail
when their guard is removed; compilation success never counts as a killed fault.

`source-limits/measure_arm.py` replays the real ARM compiler command for the
original baseline, current automatic and current limited source. It retains
objects, commands, disassembly, state symbols, function text hashes and compiler
stack frames, and requires identical default code/state/frames and an 84-byte
five-to-two saving without a frame increase. The native and offline build
entries use one compiler job. All evidence paths must be fresh.
