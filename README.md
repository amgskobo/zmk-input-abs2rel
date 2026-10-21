# ZMK Absolute-to-Relative Input Processor

[![Test](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml/badge.svg)](https://github.com/amgskobo/zmk-input-abs2rel/actions/workflows/test.yml)

[日本語](README_JA.md)

Converts absolute pointer coordinates into relative motion, smoothed over two
samples. For trackpads and touch sensors that report where the finger *is*,
feeding a pointer stack that wants to know how far it *moved*.

## Scope

This module provides absolute-to-relative conversion with per-input state and
optional runtime settings. It can be used independently of
[zmk-input-processors](https://github.com/amgskobo/zmk-input-processors), which
provides runtime-configurable versions of ZMK's standard input processors.
Related processors are available as separate modules:
[vector acceleration](https://github.com/amgskobo/zmk-input-vector-acceleration),
[inertia](https://github.com/amgskobo/zmk-input-inertia), and
[PadStick](https://github.com/amgskobo/zmk-input-padstick).

## Installation

```yaml
manifest:
  remotes:
    - name: amgskobo
      url-base: https://github.com/amgskobo
  projects:
    - name: zmk-input-abs2rel
      remote: amgskobo
      revision: main
```

## Usage

```dts
#include <behaviors/input_processor_absolute_to_relative.dtsi>

&trackpad_listener {
    input-processors = <&zip_absolute_to_relative>;
};
```

Or declare the node yourself when another suppression policy is needed, or
when the name matters — it is the settings key (see below):

```dts
pointer_abs_rel: pointer_abs_rel {
    compatible = "zmk,input-processor-absolute-to-relative";
    #input-processor-cells = <0>;
};
```

`INPUT_BTN_TOUCH` suppression defaults to true, including on a manually
declared node. The explicit property remains accepted so existing overlays and
overlays that want to document the policy keep building.

Devicetree boolean properties express `true` by being present; they cannot
carry a `false` value. Therefore a build without runtime custom settings
suppresses `BTN_TOUCH` presses and their paired releases. Releases without a
press consumed by this stream still pass through. Enable the custom-settings
option below when the flag must be switchable to `false`.

### Standard nodes

The module supplies two standard nodes. Each node keeps independent conversion
state for every ZMK input listener, so local and split-proxied devices can share
the same reference safely:

| Reference | Node name | Button handling |
| :--- | :--- | :--- |
| `zip_absolute_to_relative` | `abs_rel` | suppresses `BTN_TOUCH`; preserves `BTN_0` clicks |
| `zip_absolute_to_relative_scroll` | `abs_rel_scroll` | suppresses both `BTN_TOUCH` and `BTN_0` for scrolling |

State belongs to the pair of processor node and `input_device_index`. ZMK passes
the input listener's instance index, so two listeners using the same node keep
separate reference positions, smoothing history and suppressed-button records.
Settings still belong to the node and are shared by those listeners. Declare an
additional short-named instance only when a different button policy or a
separately adjustable settings entry is required.

### Configuration Reference

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `suppress-btn-touch` | bool | true | Consume `INPUT_BTN_TOUCH` after using it to drop the reference point, so it does not reach the mouse HID as button 0. |
| `suppress-btn0` | bool | false | Consume `INPUT_BTN_0` when the trackpad reports a physical click. |

Both are runtime values when the settings option below is on — they decide
whether a pad's physical click reaches the host at all, which is the kind of
thing that wants trying rather than deciding. On a pad that also carries
tap-to-click, one setting is a duplicate button and the other is a missing one,
and which is which depends on the pad.

When runtime settings turn `suppress-btn-touch` off, each forwarded touch edge
is made a synchronization boundary of its own. This is necessary because the
first coordinates after an edge establish a new reference and are consumed;
without a separate boundary a release could remain queued with no later event
to send it, leaving mouse button 0 held at the host.

For both `BTN_TOUCH` and `BTN_0`, suppression consumes a release only if
this stream consumed its press and suppression remains enabled. Enabling
`suppress-btn-touch` after forwarding a touch press therefore preserves its
release, with a synchronization boundary of its own. Disabling either flag
passes future edges through; it does not emit a press already swallowed.
A later consumed press or repeat cannot revoke the release owed by an earlier
forwarded press. Each listener keeps separate records for touch and physical
clicks. A layer change clears consumed-press records but retains observed
forwarded presses until their release. Initialization clears both kinds of
record. A new press discarded during a layer reset is never recorded as sent.

## How it works

### Smoothing

Movement is smoothed by averaging the current delta with the previous one:

```text
smooth = (current_delta + previous_delta) / 2
```

The halving divides rather than shifts, because a shift rounds towards minus
infinity and would make the same path measure longer travelled one way than the
other. The first sample on each axis establishes the reference point and
produces no event; smoothing begins on the second.

### Reference point and layer changes

Each input listener has its own reference point. `BTN_TOUCH` drops only that
listener's reference on both edges, while a layer change invalidates all
listeners. Absolute events are converted whenever they arrive, without checking
whether a contact is believed active — a processor route only sees the part of a
contact during which it holds the chain, so believing otherwise would silence it
for the rest of a contact that began elsewhere.

Which processors run is decided per event, from the layer active at that moment,
so one contact can be split across two chains. The instance a contact moves to
would otherwise still hold a reference point from an earlier touch, and turn its
first sample into the distance between two unrelated contacts — a jump across
the pad from a single count of real motion. So `zmk_layer_state_changed` is
subscribed directly and invalidates every reference. The callback only
increments one shared atomic generation. Each listener stream applies that
generation when it next receives an event; if it changes while an event is being
processed, that event is discarded and the stream starts clean. The layer
callback and coordinate conversion therefore never write the same state
concurrently.

### Value range

The conversion core preserves the full signed `int32_t` range used by Zephyr
input events. Differences wider than that range are saturated before smoothing
instead of wrapping. The output remains an `int32_t` relative event for the
processors and listener downstream.

## Changing the flags at runtime

`CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y` publishes both flags through
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings),
so a Studio client lists and edits them with no page of its own, under the
`amgskobo__a2r` subsystem.

A key is the owning node's devicetree name, then the field:

```
abs_rel.suppress_btn_touch
abs_rel_scroll.suppress_btn0
```

The node name is the one identifier both halves of the problem already hold: a
view drawing the chain walks devicetree for the processors in each listener and
gets a `const struct device *` per stage, whose `->name` is `DEVICE_DT_NAME()`,
which is `DT_NODE_FULL_NAME()` — the same string the key is built from. So a
stage's settings are exactly the keys starting with its device name, with
nothing registered, agreed between modules, or typed into devicetree.

### Name length limit

The longest field is `suppress_btn_touch` at 18 characters. Its complete
persisted name leaves the node **14 characters**: the 48-byte RPC key limit is
not enough on its own because Zephyr also stores
`custom_settings/<subsystem>/<key>` in a 64-byte name. A name that overruns
either limit fails the build, by name.

The option needs the patched ZMK that carries the custom Studio RPC protocol,
which is why it depends on `ZMK_CUSTOM_SETTINGS_STUDIO_RPC` rather than on the
registry alone: registering a subsystem includes `zmk/studio/custom.h`, which
pulls a nanopb-generated header that only exists when that RPC is built.

**With the option off, which is the default, none of it is compiled and the
devicetree values are fixed.** The processor itself has no dependency on any of
it — it builds and runs on upstream ZMK with no other module present.

## Project Structure

```
.
├── CMakeLists.txt
├── Kconfig
├── drivers/input/
│   ├── input_processor_absolute_to_relative.c
│   └── input_processor_absolute_to_relative_custom_settings.c   # only this needs the patched ZMK
├── include/zmk-input-abs2rel/
│   ├── absolute_to_relative_core.h              # dependency-free conversion core
│   ├── absolute_to_relative.h                   # runtime API
│   └── custom_settings.h                        # namespace and key shape
├── tests/
│   ├── integration/                              # upstream and DYA firmware fixtures
│   ├── run-integration-docker.sh
│   ├── run.sh
│   └── test_absolute_to_relative_core.c
├── dts/
│   ├── behaviors/input_processor_absolute_to_relative.dtsi
│   └── bindings/zmk,input-processor-absolute-to-relative.yaml
└── zephyr/module.yml
```

## Tests

Run from the repository root with Docker and Git Bash:

```bash
bash tests/run-docker.sh
ZMK_TEST_MODE=full bash tests/run-docker.sh
```

Quick runs every host behavioral scenario once under
AddressSanitizer/UndefinedBehaviorSanitizer. Full retains those scenarios and
adds optimized, coverage and supported ABI variants. Full mode requires 100%
line and branch coverage for the pure conversion core and the gated driver
and settings functions. The harnesses use controlled framework interfaces;
they do not establish physical HID or split delivery.

The separate integration entries build the module on unchanged upstream ZMK
and DYA. The DYA variant also checks Custom Studio settings registration:

```bash
bash tests/run-integration-docker.sh upstream
bash tests/run-integration-docker.sh dya
```

See [the test guide](tests/README.md) for named fixtures, supported variants,
offline dependency selection and remaining physical acceptance gaps. CI uses
quick mode for push/PR and full mode for scheduled/manual host runs; integration
jobs retain their separate firmware checks.

## License

[MIT](LICENSE)
