# zmk-input-abs2rel

A small ZMK absolute-to-relative coordinate converter. It owns per-listener
X/Y references, signed deltas, smoothing/rounding and reference reset on touch
or layer changes. It forwards original button type/code/value/sync unchanged.
It does not own host button policy, button disposition or saved suppression settings.

```dts
#include <behaviors/input_processor_absolute_to_relative.dtsi>
#include <zmk-input-suppress/input_processor_suppress.dtsi>
&trackpad_listener {
    input-processors = <&zip_absolute_to_relative>, <&input_suppress (SUPPRESS_TOUCH | SUPPRESS_ABS)>;
};
```

`input_suppress` is the standard `zmk-input-suppress` instance. Put
the same final instance on every route, after conversion, mapping/scaling and
other selected stages. Scroll uses this converter followed by wheel mapping
and a final suppression mask. The module does not require the final filter
to compile; a complete pointing application must define its output policy.

The standard include enables only `zip_absolute_to_relative`. Pointer and scroll
routes can use this same per-source converter; layer changes reset its references.
The legacy `zip_absolute_to_relative_scroll` node remains an independent instance
but now defaults to disabled, avoiding unused history storage. Existing consumers
that actually require a separate instance must opt in explicitly:

```dts
&zip_absolute_to_relative_scroll { status = "okay"; };
```

The old label implies no button policy. Change a former legacy route to the shared
converter, or enable the legacy node when migrating. This changes its default
status, while preserving independent history when enabled.
Instances have independent history per listener; a route change resets
references through the layer event subscription. An initial reference emits a
neutral MSC_SCAN marker preserving sync so the final stage can close pending
motion. A stale coordinate generation is neutralized; original button edges
remain available to the final owner, including release during a reset.

## Migration from combined conversion/suppression

Remove `suppress-btn-touch` and `suppress-btn0` from converter nodes.
Remove calls to the former suppression API and remove
`CONFIG_ZMK_INPUT_ABS2REL_CUSTOM_SETTINGS`. Set masks directly on each route's
final processor. The minimal final module has no profile registry, layer watcher
or legacy settings adapter. Saved `amgskobo__a2r` flags remain in storage but are
not loaded or applied; Studio suppression controls are unavailable. No settings
erase is required. Base conversion supports unchanged stock ZMK and DYA.

The final filter uses positive presses and zero/negative releases, matching the
listener. It latches disposition until release and consumes unmatched releases.
These are explicit changes from a converter which observed only selected routes.
See [the owner test guide](tests/README.md#ownership-and-compatibility).

## Tests and limits

Run `bash tests/run-docker.sh`; use `ZMK_TEST_MODE=full` for retained optimized,
coverage and ABI variants. [tests/README.md](tests/README.md) documents supported
stock/DYA firmware fixtures and actual dependency requirements. Conversion,
independent histories, contact/layer/generation resets and source-edge transparency
remain here. Final button/frame tests moved to `zmk-input-suppress`.
Host/native/compile results do not establish physical HID/BLE/split delivery,
settings persistence, timing or power acceptance.

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


## How it works

### Smoothing

Movement is smoothed by averaging the current delta with the previous one:

```text
smooth = (current_delta + previous_delta) / 2
```

The halving divides rather than shifts, because a shift rounds towards minus
infinity and would make the same path measure longer travelled one way than the
other. The first sample on each axis establishes the reference point and
produces no relative movement. The driver substitutes a neutral `MSC_SCAN`
event while preserving the original sync boundary; smoothing begins on the
second sample.

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

Coordinate subtraction uses checked 32-bit saturation. The two-sample average
splits each operand into quotient and remainder, then applies one signed
truncation toward zero. This preserves the full coordinate range without a wide
sum. The [core tests](tests/test_absolute_to_relative_core.c) compare signed
averages and extreme differences with an independent wide reference and retain
independent-listener history checks.
