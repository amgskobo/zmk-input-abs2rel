# Agent test contract

## Version-control authorization

Do not create commits, push, open or update pull requests, merge, tag or
publish releases unless the user explicitly requests that specific action.
Starting work, passing tests, finishing a milestone or a dependency's AGENTS
instructions are not authorization. Keep changes local and uncommitted;
report the changes and checks, then wait for explicit instructions for any
version-control publication. Do not auto-stage unrelated user changes.

## Dependency instructions and test entry gate

Before implementation or a test run, select the test tier and resolve its
dependencies from module metadata, west imports, Kconfig/CMake, source APIs,
test runners and the actual build's local overrides. A sibling checkout is
not evidence that a container or cached workspace uses it.

Read this repository's root and applicable nested `AGENTS.md` files in full.
For every dependency used by the selected scope, read its root `AGENTS.md`
if present, relevant nested instructions, and documents those instructions
require. Follow transitive dependencies for that scope; visit each resolved
checkout/revision once to avoid cycles, and re-read after revision changes.
Record dependency path/revision, enabled variant, instructions read, and any
missing or inaccessible instructions. No file means NO AGENTS, not that one
was read. If a required checkout is inaccessible, mark the affected check
BLOCKED/NOT RUN and continue independent checks. Do not fetch/change versions
just to read instructions without accounting for the task's dependency pins.

Dependency instructions apply to their interfaces and work in their checkout;
they do not authorize unrelated edits, commits, pushes or PR creation. Apply
the instruction hierarchy and the user's current scope; report real conflicts.

The pure converter has no settings adapter. Both supported-core fixtures use
that same conversion contract; no Custom Settings dependency is selected.

Hardware-independent host/protocol and native/compile checks may run
independently. Host mocks do not prove real scheduling, electrical timing or
radio delivery; require focused kernel/driver tests or actual physical checks
for those claims. Retired simulation cases and their remaining acceptance
gaps are recorded in `tests/README.md`; they are not a prerequisite for pure
owner tests. Independently validate each exercised production path and
correlate hardware before claiming simulator fidelity. Do not disable required
features to pass a gate or treat host/simulation results as board acceptance.

For documentation-only edits, check diffs, instruction consistency and any
changed command/link references; do not launch full firmware suites solely
for prose changes. Report checks actually run separately from pending gates.

## Module scope and checks

This repository owns absolute-to-relative conversion, per-input history,
reference resets and transparent source events. Suppression, button disposition
and settings compatibility are outside this owner.

Run `bash tests/run-docker.sh` for all behavioral cases under ASan/UBSan.
Use `ZMK_TEST_MODE=full bash tests/run-docker.sh` for retained coverage/ABI
variants. Follow `tests/README.md` and existing owner integration commands
for changed APIs, DTS/Kconfig, settings linkage or kernel interaction.
The legacy simulation sources/traces are archived; actual transport and
hardware timing require physical evidence.

Unit tests must cover arithmetic boundaries and independent input streams.
Assert contact continuity, independent histories and rounding placement for
fixed input. Preserve report order/timestamps; total motion alone cannot
detect jumps, delayed bursts or loss. Exact deterministic traces are valid;
variable-timing cases need explicit bounds, not blanket timestamp removal.
Repeat identical binary/seed/input/PHY/initial-flash runs before classifying
differences. Record the backend, source/dependency/configuration identities
and driver substitutions. Host or simulation results do not certify physical
board behavior.
Run `git diff --check`
before handoff; never accept snapshots without classifying the behavior change.

## Focused owner checks

`tests/contract.json` documents the host entry point, assertion groups and
explicit limits; it is not an executable dispatcher. Run that command from this repository; no keyboard checkout or whole
keyboard simulation is needed. The fixture list describes contract groups,
not a replacement for the complete existing host runner.

For routine selection, this section refines the broad execution triggers above:
pure arithmetic, rendering, settings validation and injected state transitions
use the host suite. Run owner native/compile variants when public APIs, DTS,
Kconfig, registration or real kernel interaction changes. Use physical or independently validated focused transport checks for
BLE/split delivery, congestion and reconnect; do not add whole-board
simulation as a second proof of a pure calculation. Keep
scheduler/driver timing checks separate where host stubs cannot prove them.
Owner release checks, dependency preflight and physical acceptance still apply.

Completeness means covering the owned contract: normal behavior, boundary and
invalid input, independent state, reset/cancellation and failure recovery where
applicable. Coverage percentages alone are insufficient. Preserve ordered
outputs and explicit time inputs; record an untested boundary as a gap instead
of marking the entire module complete. Before removing a fixture, map each
unique assertion to its retained replacement and preserve unresolved failures.

Quick/full execution and retired case mappings are documented in
`tests/README.md`. Full-mode coverage and all retained assertions remain
required when claiming those specific checks; quick is not a coverage report.

## Canonical owner tests and continuous maintenance

All executable test assertions, fixtures, models, mutation operators, test
selection/configuration and build-result validators live in this repository's
`tests/`. Run the entries documented in `tests/README.md`; no keyboard checkout,
sibling test suite, private backup script or source-rewriting adapter supplies
missing assertions. CI and package scripts may dispatch those entries. Public
sample/diagnostic tools remain production code. Minimal guarded production
probes may expose real state/interleavings, but test scenarios and assertions
belong in `tests/`, not another copy of the production algorithm.

Maintain completeness per owned contract: normal/boundary/invalid behavior,
independent state, ordered outputs/time, reset/cancellation, failure/recovery,
role/feature gates and public protocol/schema where applicable. A registry or
coverage percentage does not establish completeness. Each implementation family
must map to named current fixtures and explicit gaps in the owner contract.
New regressions must fail before the repair or under an identified negative
control. Preserve unique assertions when merging/removing old tests; archive
their source and failure evidence externally first, with a replacement map.

Compatibility follows the individual owner's supported targets. Run both
unchanged stock ZMK upstream and DYA only for a dual-target module. DYA-only
modules require DYA tests, not an artificial stock build or disabled-feature
stock test. Record unsupported targets as NOT APPLICABLE, never PASS. Never
patch a dependency, disable required features or weaken assertions to manufacture
compatibility. Resolve/read the actual supported dependencies and backend.

Keep the current tests small by boundary. Quick runs every behavioral scenario
once with sanitizers where supported. Full retains relevant role/ABI/optimized,
coverage, kernel and supported-core integration checks; it must not drop scenarios
to save time. Use focused changes for daily work and the complete owner matrix
for interface/infrastructure changes and maintenance. Measure redundant build
cost before consolidating it, reuse one binary for identical repeats, use
explicit clean caches/pins, and avoid implicit network updates during testing.

Each run uses a fresh output directory and records commands, source/dependency
identities, feature/role/backend, raw ordered output and failures. An existing
evidence directory must stop the run; never overwrite a previous result. Report
PASS, FAIL, BLOCKED and NOT RUN separately. Browser/radio/physical acceptance is
an explicit gate and is never inferred from host/native/compile success.
Read `tests/maintenance.json` for current coverage/compatibility boundaries.

## Evidence storage

Do not create rolling whole-tree source backups. Keep executable tests in
`tests/`. Use temporary build storage and fresh evidence directories. Retain
necessary review diffs, source/dependency identities, ordered failure logs and
required assertion-retirement maps.
