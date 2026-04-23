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

Split into the canonical **declaration/definition** pattern:
`fairy_curtain_lights.h` holds the class declaration and method signatures
(plus trivial inline accessors like `get_channel_a()`);
`fairy_curtain_lights.cpp` holds the out-of-line method implementations and
starts with `#include "fairy_curtain_lights.h"`. Never redeclare the class in
the `.cpp` — ESPHome/PlatformIO sweeps every `.cpp` in the component directory
and the class is already visible via the header, so a redeclaration produces
an ODR violation / "redefinition of class" error.

**Inner class gotcha:** `ShiftedPWM : public output::FloatOutput` — the base
class's virtual methods are `write_state()` only. `ShiftedPWM::setup()` is
**not** an override (there's no virtual `setup()` in the `FloatOutput` chain);
`FairyCurtainLights::setup()` calls it explicitly. Do not mark
`ShiftedPWM::setup()` with `override` in the declaration — the compiler will
reject it.

## Repository layout

Canonical ESPHome external-component structure: source files live at
`components/fairy_curtain_lights/` (not at the repo root). The folder name is
snake_case, matches the YAML key users write in their config, and matches the
C++ namespace — all three must stay in sync.

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

## Dev loop

`examples/curtain-lights-dev.yaml.example` is a template for a self-contained
ESPHome device config that pulls the component via `external_components: -
source: { type: local, path: ../components }`. The working copy
(`examples/curtain-lights-dev.yaml`) is **gitignored** — each developer keeps
their own tweaked version locally without polluting history. CI stages a
throw-away copy from the `.example` before running `esphome config`.

To set up locally:

```bash
cp examples/curtain-lights-dev.yaml.example examples/curtain-lights-dev.yaml
cp examples/secrets.yaml.example examples/secrets.yaml   # fill in real values
cp .env.example .env                                      # set ESPHOME_DEVICE_IP
```

Then:

- `npm run config:esphome` — static validation via the ESPHome Docker image
- `npm run build:esphome` — compile firmware into `examples/.esphome/build/curtain-lights-dev/` (ESPHome writes its build tree next to the YAML, not at config root)
- `npm run upload:esphome` — OTA flash the resulting `.ota.bin` to the device
  via a multipart POST to `http://<ip>/update` (see `bin/upload-esphome.ts`)
- `npm run typecheck` — TS typecheck the helper scripts; this is what PR CI
  runs

The Docker-based ESPHome invocations use Windows-style `%cd%` for local dev; CI
calls the `docker run ... -v "$PWD:/config" ghcr.io/esphome/esphome ...`
incantation directly with POSIX paths.

The `DEVICE_NAME` constant at the top of `upload-esphome.ts` must match the
`esphome: name:` field in `curtain-lights-dev.yaml` — both halves of the
firmware path
(`examples/.esphome/build/<name>/.pioenvs/<name>/firmware.ota.bin`) derive
from it.

## CI/CD

Two long-lived branches: `master` cuts stable releases, `development` cuts
`-rc.N` prereleases. Semantic-release reads conventional-commit messages to
decide version bumps and changelog entries. Three workflows:

- `release.yml` — runs on push to `master`/`development`; runs `npm ci` then
  `npx semantic-release`, exposes `new-version` / `new-version-created` outputs
  for any future downstream jobs to gate on
- `pr.yml` — runs on PRs; typecheck + `esphome config` against the example YAML
  (writes dummy secrets inline for validation)
- `backmerge.yml` — fires on successful `Release` workflow runs on `master`;
  opens (or reuses) a PR from `master` → `development` and enables auto-merge.
  Idempotent via `gh pr list` check

`.releaserc.json` uses the standard commit-analyzer → release-notes-generator
→ changelog → npm (with `npmPublish: false`) → github → git plugin chain. No
release assets are attached — users consume this repo by pinning a git ref
in their `external_components:` source, so the tag itself is the deliverable.
No `@semantic-release/exec` / `update:version-h` stamping is wired up; if you
ever want the component to self-report a version, add it as a new plugin step
and list the generated file in `@semantic-release/git.assets`.

**GitHub repo settings required for full functionality** (configure in the web UI):

1. *Actions → General → Workflow permissions*: "Read and write" + "Allow
   GitHub Actions to create and approve pull requests" (needed by
   `backmerge.yml`)
2. *General → Pull Requests*: "Allow auto-merge" (needed by `gh pr merge
   --auto` in `backmerge.yml`)
3. If you add branch protection on `development` that requires `pr.yml` to
   pass: PRs opened by `GITHUB_TOKEN` don't trigger other workflows, so the
   back-merge PR will stall. Fix by either not requiring `pr.yml` on
   `development`, or by switching `backmerge.yml` to a fine-grained PAT
   (`BACKMERGE_TOKEN`) with `Pull requests: Read and write` + `Contents: Read`.
