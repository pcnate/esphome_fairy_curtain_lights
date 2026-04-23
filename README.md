# fairy_curtain_lights

An ESPHome external component for ESP32 that drives two-channel fairy curtain
lights with **phase-shifted PWM** — two LEDC outputs share one timer but are
offset 180° (via the LEDC `hpoint` parameter) so their active windows never
overlap, which lets them alternately supply opposite polarities to an AC-style
load without shorting. Each channel is exposed as a standard ESPHome
`FloatOutput`, and each `write_state` value is scaled to half the duty range so
one channel can only ever fill its own half of the cycle. Drop it into a config
via `external_components:` and reference `fairy_curtain_lights:` with
`gpio_a`/`gpio_b`/`id_a`/`id_b` (and an optional `frequency`, default 1000 Hz).
