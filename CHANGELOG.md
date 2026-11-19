# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.4] - 2026-11-19

### Added

- `edm::SampleGenerator` — a C++20 concept: an SFX is anything with
  `float next() noexcept`. The generator is the plugin; the library
  renders any user-supplied generator without knowing what the sound is.
- `edm::render_sfx(Generator&&, std::span<float>)` — the generic render
  path: pulls samples from any generator into a buffer. Zero-cost (the
  template inlines the generator — no vtable, no indirection).
- `edm::SlideSfx` — the v0.0.2 slide model (oscillator + linear
  frequency slide + AD envelope) as ONE built-in generator, not the
  definition of SFX.
- `CHANGELOG.md` (this file). Version history moves out of the README.

### Changed

- The params-based `render_sfx(SfxParams, Rng, std::span<float>, double)`
  convenience overload is reimplemented on the generic path (it builds
  a SlideSfx and renders through it).
- README trimmed: it now covers what the library is, build, quickstart,
  examples, and license; history lives here.

### Removed

- **Breaking:** `edm::sfx::jump()`, `coin()`, `laser()`, `explosion()`
  and the `edm::sfx` namespace. Sound identities are user content, not
  library knowledge — a game has more SFX kinds than the library can
  enumerate, so it must not try. The presets live on as local functions
  in `examples/sfx_demo.cxx`, demonstrating how a user defines their own
  sounds. (0.x, no users yet — the break is intentional.)

## [0.0.3] - 2026-11-19

### Added

- `edm::Pattern` — a loop of 16th-note steps; each step is a rest or a
  note (MIDI) with velocity + gate length.
- `edm::Voice` — the voice interface (`note_on` / `note_off` / `next` /
  `reset`); `edm::SynthVoice` is oscillator + ADSR + optional lowpass,
  monophonic with retrigger. `make_synth_voice` builds one from a
  config struct.
- `edm::Sequencer` — schedules patterns at a BPM, sample-accurate step
  boundaries, gate-aware note on/off. Tracks own their voices via
  `std::unique_ptr`; patterns loop and may differ in length across
  tracks.
- `examples/sequence_demo.cxx` — renders a 2-bar bass + lead loop.

## [0.0.2] - 2026-11-19

### Added

- SFX: `edm::SfxParams` (wave + linear frequency slide + AD envelope),
  `edm::render_sfx`, `edm::randomize_sfx` (sfxr-style random params),
  and the `edm::sfx::*` presets — removed in 0.0.4.
- `examples/sfx_demo.cxx` — renders the 4 presets + 1 randomized
  effect to WAVs.

### Changed

- Tests migrated from a Python script to GoogleTest via CMake
  FetchContent (v1.17.0). No Python anywhere in build or tests.

## [0.0.1] - 2026-11-19

### Added

- The synth core: `edm::Rng` (deterministic xorshift32),
  `edm::Oscillator<Wave>` (Sine / Square / Saw / Noise),
  `edm::Envelope` (AD / ADSR), `edm::Biquad` (RBJ lowpass) +
  `edm::Highpass` (one-pole), `edm::Buffer` (owning PCM float
  buffer), `edm::write_wav` (16-bit mono WAV writer).
- `examples/tone.cxx` — renders a one-second tone.
