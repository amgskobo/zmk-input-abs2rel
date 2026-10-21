# Module-owned tests

Run from this checkout. Docker + Bash is the host entry point; no keyboard
checkout, shared test dispatcher or simulator repository is required.

```bash
bash tests/run-docker.sh
ZMK_TEST_MODE=full bash tests/run-docker.sh
```

On PowerShell, set `$env:ZMK_TEST_MODE="full"` for the second command
and invoke Git Bash explicitly. Remove the variable for the default quick run.

Quick executes **all host behavioral fixtures and scenarios** once. C fixtures use
AddressSanitizer/UndefinedBehaviorSanitizer; offline-admission Python units run
through the same `tests/run.sh` entry in both CI and Docker. Coverage-admission
controls also run in both modes. Full repeats those same assertions
under the retained optimization, coverage and (where already present) 32-bit
variants. CI uses quick for push/PR and full for scheduled/manual runs.
Unknown mode values fail; there is no silent skipped success.

[maintenance.json](maintenance.json) records supported cores, current commands
and explicit gaps; [contract.json](contract.json) maps the retained assertions.
These maps are not passing test evidence. Preserve ordered output, raw
timestamps and the first failure, and resolve/read actual dependencies before
execution. Public interface and integration checks use both unchanged stock
ZMK and DYA, as declared by this owner.

## Retained behavioral contracts

| Fixture | Assertions |
| --- | --- |
| [test_absolute_to_relative_core.c](test_absolute_to_relative_core.c) | Signed deltas, rounding and arithmetic boundaries |
| [runtime/driver_harness.c](runtime/driver_harness.c) | Independent histories, contact reset, suppression, generation changes and private-label-free state logs |
| [test_runtime_coverage.py](test_runtime_coverage.py) | Full-mode runner rejects empty, unrelated and partial measurement; each gated range needs executable lines and unexecuted lines/branches remain failures |
| [runtime/custom_settings_harness.c](runtime/custom_settings_harness.c) | Settings validation, private-key-free collision diagnostics, paired application and newer callback precedence across an older paused registry read |

Each fixture keeps its existing normal, boundary/invalid, independent-state,
reset/cancellation and failure/recovery scenarios where applicable. Checks
assert ordered outputs and explicit time inputs. Coverage is a full-mode gate,
not a reason to replace assertions or a hardware completeness claim.

The driver fixture pins a forwarded button's release across a mid-event
generation reset on both streams. A release paired with a suppressed press
stays consumed, and unrelated motion/new presses remain discarded. The touch
pairing regression also enables suppression between a forwarded press and its
release, for each listener. It requires the release and its sync boundary to
survive; consumed press/repeat/release pairs remain consumed. Disabling the
policy, layer resets and independent listener ownership are separate cases.
The pre-fix direct helper case consumed an unmatched release; its intended
behavior changed to preserve releases. Its invalid-code/no-sync assertions are
retained on a genuinely consumed pair. These checks establish the processor
boundary, not downstream HID or split delivery.

`test_repeat_preserves_forwarded_release` covers both button kinds and both
listeners across live policy changes, no reset, a reset before a repeat and a
reset during it. Four nonzero values (1, 2, -1 and INT32_MAX) retain the existing
pressed-value interpretation: 48 cases require the original release and touch
sync boundary. Separate cases reject false forwarding credit for a new press
discarded mid-event. Initialization clears both observed-forwarded and consumed
records; a layer reset retires only consumed records. A normal paired-consumption
fixture finishes any earlier forwarded contact before starting its consumed pair.

The coverage-admission controls run the actual runtime entry with recording
compiler/gcov tools. Empty reports, metadata-only reports and a report containing
just one gated function must fail. Separate parser checks retain normal counts
and unexecuted line/branch failures. These synthetic process checks prove the
coverage gate, not firmware execution. The normal full entry still requires
actual measured coverage for every selected function range. Macro expansions
are measured in their calling functions; macro definition lines have no separate
gcov executable range. Their existing state/output assertions remain in the
driver and settings fixtures.

## Separate integration and physical contracts

Keep the existing owner `run-integration-docker.sh` commands and accepted
upstream/DYA variants for changed APIs, DTS/Kconfig/registration or actual
kernel boundaries. Native fixtures and negative guards are retained. Do not
run all firmware variants for a pure arithmetic edit. Host extracted functions
and controlled work queues do not prove real scheduling or transport delivery.

## Overlapping input boundary

The normal quick/full driver fixture includes `test_overlapping_buttons`:
24 cases cover either button forwarded while the other is consumed, both
release orders, two forwarded buttons across generation changes, record
retirement after release, and two active listeners with different ownership.
Each edge requires the exact continue/stop result, event code and sync boundary.

Run the focused control inside the Linux build image:

```bash
python3 tests/check_button_overlap.py --output /absolute/new/overlap
```

[check_button_overlap.py](check_button_overlap.py) compiles the same owner
fixture normally and with a test-only alias of the two button histories.
Normal must pass all 24 cases; the alias must fail the release-ownership
assertion. No production source is rewritten. This checks observed processor
state; physical HID and arbitrary cross-processor routes remain separate.

The source-only entry `python3 tests/runtime/run.py --emit-driver FRESH_SOURCE`
emits that same driver fixture. Both source entries use exclusive creation.
The control also requires existing files, symlink aliases and unknown selectors
to be rejected without changing the existing source. It retains source hashes,
compiler identity and ordered command output in a fresh evidence directory.

## Settings callback kernel boundary

The custom-settings fixture pauses an older callback after it copies the touch
flag, admits a newer write to both registry flags, then releases the old read.
The final applied pair must match the newer registry state. Host modes use
pthread-backed mutex stand-ins. The same owner fixture can run with actual
Zephyr threads, mutexes and condition variables:

```bash
bash tests/native/run-settings.sh /absolute/resolved/workspace /tmp/abs2rel-settings-normal normal
bash tests/native/run-settings.sh /absolute/resolved/workspace /tmp/abs2rel-settings-control skip-lock
```

Run inside the Linux Zephyr build image with the native host toolchain. Both
outputs must be fresh. The entry requires unchanged Zephyr
`10ba6d0cb38bc3d258775d27982f707599320085`, shared by the reviewed stock/DYA
graphs. Each mode builds once and executes the same binary three times. Normal
must pass; the test-only skip-lock control must fail the stale-value assertion
each time. A timeout or unrelated failure is not an accepted control result.
Source hashes, dependency identity, configuration, build and ordered runtime
logs are retained under the output directory.

The source-only entry `python3 tests/runtime/run.py --emit-custom-settings FRESH_SOURCE`
emits the same fixture and extracted production functions, without executing
tests. Native execution proves this controlled mutex/thread boundary; registry
values, device and setter remain controlled. Persistence, Studio, arbitrary
interleavings and physical delivery need separate evidence. Stock/DYA module
integration entries still compile the actual adapter and registration paths.

## Retired simulation boundaries

The retired `tests/bsim` sources and ordered snapshots were hash-verified
before removal. Their retirement manifest retains raw traces and the assertion
mapping below. Archive locations belong to the consuming keyboard integration
repository and are not a prerequisite for these module-owned checks.
Unresolved snapshot failures remain open; no failed snapshot was accepted as
passing.

| Retired case | Behavioral replacement | Transport boundary |
| --- | --- | --- |
| route-switch | The owner fixtures above retain state/arithmetic/error assertions | Ordered physical split/HID/LED/battery delivery and timing: pending actual-device evidence |
| split-contacts | The owner fixtures above retain state/arithmetic/error assertions | Ordered physical split/HID/LED/battery delivery and timing: pending actual-device evidence |

For the affected case, exercise the same contact/layer/settings/power or
congestion/reconnect transition on the production board. Capture each report
value/order/timestamp and verify balanced releases and specified deadlines.
Do not mark the archived exact radio trace as covered by a host stub.

## Explicit offline integration

The existing `tests/integration/run.sh` also accepts an already resolved cache
through `ZMK_TEST_OFFLINE_WORKSPACE`, with a fresh `ZMK_TEST_OUTPUT` directory.
Run this entry inside the same build image with the module mounted at `/src`.
The offline path makes an isolated west workspace using the original fixture
manifest, an unchanged local Git clone of ZMK and symlinks to cached dependencies; it never fetches or patches
the core. Exact supported core, Zephyr and required library revisions and clean
source state are checked before a build; existing outputs are rejected.
Outputs inside the resolved workspace or any linked dependency are rejected,
including aliases through parent symlinks. Project paths must remain separate
from each other and from west/config metadata, so staging never writes through
a project link into the original cache. Layout checks run before output creation.
Select `upstream` or `dya` as the first argument to this owner's integration
entry. The workspace must match that variant.

DYA fixtures that enable custom settings require
`ZMK_TEST_CUSTOM_SETTINGS_ARCHIVE`: the SHA256-checked git archive of the pinned
`c6a7fef3a3be3d3ace5de9a4b0628c6418cd1f3f` test dependency, without local edits.
The accepted archive digest is recorded in `tests/integration/offline.py`.
All existing fixture assertions and cases run unchanged after preparation.
Test offline admission and rejection with
`python3 -m unittest discover -s tests/integration -p test_offline.py -v`.
Core updates require an explicit pin review rather than an implicit fetch.

The preparation uses Linux symlinks and runs inside the documented build image.
`ZMK_TEST_OFFLINE_NEGATIVE_CONTROL=skip-identity` in the admission test command
intentionally bypasses dependency identity validation; its rejection tests must fail.
`ZMK_TEST_OFFLINE_NEGATIVE_CONTROL=skip-archive-digest` bypasses the archive
digest. Its parseable but incorrect archive must fail the missing-rejection
assertion, so a tar parser error cannot stand in for digest validation.

For example, run the stock fixture inside the build image:

```bash
ZMK_TEST_OFFLINE_WORKSPACE=/resolved/upstream \
ZMK_TEST_OUTPUT=/tmp/abs2rel-upstream-v1 \
bash /src/tests/integration/run.sh upstream
```

Select `dya` with the resolved DYA workspace for its variant; provide the pinned
custom-settings archive when that fixture enables custom settings.
