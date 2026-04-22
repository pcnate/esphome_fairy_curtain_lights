# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An **ESPHome external component** that drives two-channel fairy curtain lights on
an ESP32 using **phase-shifted PWM**. The two channels share one LEDC timer but
are offset 180° via the LEDC `hpoint` parameter, so their active-high windows
never overlap. This matters because the lights are AC-ish: channel A supplies
one polarity, channel B the other, and overlap would short/brown them.

Consequently, `write_state()` scales input by `MAX_DUTY / 2` (not `MAX_DUTY`) —
each channel can only use half the period before it would collide with the
other. A YAML `output` level of `1.0` means "fill my half of the cycle," not
"full duty."

There is no local build system. This component is consumed by an ESPHome user's
YAML config (via `external_components:`), and ESPHome's Python codegen compiles
it as part of the user's firmware build. There are no tests in this repo.

## Architecture

Two sides that must stay in sync:

**Python side — `__init__.py`**
Registers the component with ESPHome's codegen, defines the YAML schema
(`gpio_a`, `gpio_b`, `frequency`, `id_a`, `id_b`), and in `to_code()` constructs
the C++ object and registers `get_channel_a()` / `get_channel_b()` as two
`FloatOutput`s that the user can reference from their `light:` / `output:`
blocks.

**C++ side — `fairy_curtain_lights.h`**
Header-only component. `FairyCurtainLights` owns two inner-class
`ShiftedPWM : output::FloatOutput` instances on `LEDC_CHANNEL_0` and
`LEDC_CHANNEL_1`, both bound to `LEDC_TIMER_0` at 13-bit resolution
(`MAX_DUTY = 8191`). Channel B is constructed with `hpoint = MAX_DUTY / 2` to
achieve the 180° shift. The timer is configured in the constructor; channels
configure themselves in `setup()`.

**`fairy_curtain_lights.cpp` is a stale duplicate** of the header — same class
redefined, does not `#include` the header, and is not how ESPHome builds this.
Changes to behavior must go in the `.h`. If you touch this file, reconcile or
delete it rather than editing both.

## When editing

- Any new YAML field needs matching changes in **both** `__init__.py` (schema +
  `to_code`) and the C++ constructor/accessors. They are a contract.
- `LEDC_TIMER_0` and `LEDC_CHANNEL_0/1` are hard-coded. If the user's YAML also
  uses `ledc`-based outputs elsewhere, channel/timer collisions are the likely
  culprit — don't assume the component is broken.
- The `MAX_DUTY / 2` scaling in `write_state()` is deliberate (see above), not a
  bug. Do not "fix" it to `MAX_DUTY`.
- ESP-IDF LEDC APIs (`ledc_timer_config`, `ledc_channel_config`, `ledc_set_duty`,
  `ledc_update_duty`) are used directly rather than ESPHome's `ledc` output —
  that's intentional because ESPHome's wrapper does not expose `hpoint`.

## Code style

Global style rules in `~/.claude/CLAUDE.md` apply (spaces inside `( )`, `[ ]`,
`{ }` when non-empty; JSDoc on functions; etc.). The `.h` already follows this
style; `__init__.py` is inconsistent and `.cpp` does not follow it — match the
`.h` when editing, and prefer fixing drift over preserving it.
