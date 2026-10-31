# 0002. A zone is a seeded performance, played live, not a stored sample

Taken 2026-10-25. Stands.

## Context

The default library is 73 keys × 5 layers × 4 round robins × 9
articulations: 13,140 zones. Rendered as samples of a second or two each at
48 kHz that is several gigabytes - more than a plugin can keep in memory, and
slow to rebuild every time a setting changes.

## Decision

A zone is a **small set of performance variations** (`ZoneVariation` in
`Source/Core/Instrument.h`): detune, level, brightness, attack speed, timing,
noise, a pitch scoop, a slow "human" drift, and a curve of per-partial
timbre changes. Each is drawn from a seed (`Rng`, splitmix64) keyed by the
seed, the key, the layer and the round robin, so the same settings always
give the same library, and **New Takes** draws a fresh one. Every key also
has a character of its own shared by its takes, as a real instrument's
strings or bore do. The voice plays the model live with the zone's
variations.

The whole library is a few megabytes; rebuilding it on a change is instant.
**Export SFZ…** bakes it to WAV files plus an SFZ map when a real sample
library is wanted (`MultisampleExporter`).

## Consequences

- Round robins are clearly different (waveform correlation about 0.35) and
  consistent in level (within 0.7 dB); the same zone renders identically
  twice (tested).
- 32 voices use about 30% of one CPU core: the oscillator bank is the cost.
- Layer Blend moves smoothly between neighbouring layers, interpolating
  their variations; off, the layers switch as a sampled library's do.
- An exported library drops Legato (SFZ can't express it).
