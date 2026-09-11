# ZMK Absolute-to-Relative Input Processor

Converts absolute pointer coordinates into relative motion, smoothed over two
samples. For trackpads and touch sensors that report where the finger *is*,
feeding a pointer stack that wants to know how far it *moved*.

## Why this is its own module

It is an original, not a replacement. ZMK ships five input processors — scaler,
transform, code-mapper, temp-layer, behaviors — and
[zmk-input-processors](https://github.com/amgskobo/zmk-input-processors) is
runtime-configurable versions of those, each named `runtime-*` so a chain shows
at a glance which stages can be changed while the keyboard runs.

This has no upstream counterpart, so the prefix would say nothing about it, and
it kept its plain name as a documented exception. That exception was the
symptom: it belongs with the other originals —
[vector-acceleration](https://github.com/amgskobo/zmk-input-vector-acceleration),
[inertia](https://github.com/amgskobo/zmk-input-inertia),
[padstick](https://github.com/amgskobo/zmk-input-padstick) — each in a module
of its own.

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

Or declare the node yourself, which is worth doing when more than one instance
is routed or when the name matters — it is the settings key (see below):

```dts
pointer_abs_rel: pointer_abs_rel {
    compatible = "zmk,input-processor-absolute-to-relative";
    #input-processor-cells = <0>;
    suppress-btn-touch;
};
```

### Configuration Reference

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `suppress-btn-touch` | bool | false | Consume `INPUT_BTN_TOUCH` after using it to drop the reference point, so it does not reach the mouse HID as a button press. |
| `suppress-btn0` | bool | false | Consume `INPUT_BTN_0` when the trackpad reports a physical click. |

Both are runtime values when the settings option below is on — they decide
whether a pad's physical click reaches the host at all, which is the kind of
thing that wants trying rather than deciding. On a pad that also carries
tap-to-click, one setting is a duplicate button and the other is a missing one,
and which is which depends on the pad.

Turning `suppress-btn0` off does not release a press already swallowed: that
press's release still passes through, for the same reason it does across a
layer change.

## How it works

**Smoothing**: movement is smoothed by averaging the current delta with the
previous one: `smooth = (current + previous) / 2`. The halving divides rather
than shifts, because a shift rounds towards minus infinity and would make the
same path measure longer travelled one way than the other. The first sample on
each axis establishes the reference point and produces no event; smoothing
begins on the second.

**Reference point**: `BTN_TOUCH` drops it on both edges, and so does a layer
change. Absolute events are converted whenever they arrive, without checking
whether a contact is believed active — an instance only sees the part of a
contact during which it holds the chain, so believing otherwise would silence
it for the rest of a contact that began elsewhere.

**Layer changes**: which processors run is decided per event, from the layer
active at that moment, so one contact can be split across two chains. The
instance a contact moves to would otherwise still hold a reference point from
an earlier touch, and turn its first sample into the distance between two
unrelated contacts — a jump across the pad from a single count of real motion.
So `zmk_layer_state_changed` is subscribed directly and drops the reference.

`suppress-btn0` never drops a `BTN_0` release whose press was not suppressed
here. Passing a release through is always safe — the press it belongs to
already reached the host — while dropping one would leave the button held down
with nothing left to release it. That record is cleared on a layer change too.

## Changing the flags at runtime

`CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS=y` publishes both flags through
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings),
so a Studio client lists and edits them with no page of its own, under the
`amgskobo__abs2rel` subsystem.

A key is the owning node's devicetree name, then the field:

```
pointer_abs_rel.suppress_btn_touch
scroll_abs_rel.suppress_btn0
```

The node name is the one identifier both halves of the problem already hold: a
view drawing the chain walks devicetree for the processors in each listener and
gets a `const struct device *` per stage, whose `->name` is `DEVICE_DT_NAME()`,
which is `DT_NODE_FULL_NAME()` — the same string the key is built from. So a
stage's settings are exactly the keys starting with its device name, with
nothing registered, agreed between modules, or typed into devicetree.

The longest field is `suppress_btn_touch` at 18 characters, which leaves a node
28 of the 47 a key has. A name that overruns fails the build, by name.

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
│   └── absolute_to_relative_custom_settings.c   # only this needs the patched ZMK
├── include/zmk-input-abs2rel/
│   ├── absolute_to_relative.h                   # runtime API
│   └── custom_settings.h                        # namespace and key shape
├── dts/
│   ├── behaviors/input_processor_absolute_to_relative.dtsi
│   └── bindings/zmk,input-processor-absolute-to-relative.yaml
└── zephyr/module.yml
```

## License

MIT
