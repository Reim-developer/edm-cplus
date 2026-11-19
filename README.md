# edm-cplus

[![CI](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml/badge.svg)](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml)

Create electronic dance music with C++ code.

v0.0.3 adds the sequencer + patterns — the composition layer —
on top of the v0.0.2 SFX and the v0.0.1 synth core. This is the
version that turns the library from "makes sounds" into "makes
music": notes, scheduled at a tempo, rendered through voices.
No arrangement or playback yet. Those come as later versions.

## v0.0.3 — Sequencer + patterns

- `edm::Pattern` — a loop of 16th-note steps; each step is a rest
  or a note (MIDI) with velocity + gate length. Out-of-range
  access is safe by construction.
- `edm::Voice` — the voice interface (`note_on` / `note_off` /
  `next` / `reset`); `edm::SynthVoice` is oscillator + ADSR +
  optional lowpass, monophonic with retrigger. `make_synth_voice`
  builds one from a config struct.
- `edm::Sequencer` — schedules patterns at a BPM, sample-accurate
  step boundaries, gate-aware note on/off. Tracks own their
  voices via `std::unique_ptr`; patterns loop and may differ in
  length across tracks.

```cpp
#include <utility>

#include "edm/buffer.hxx"
#include "edm/pattern.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"
#include "edm/wav.hxx"

// 140 BPM, 44.1 kHz. A 2-bar bass loop:
edm::Sequencer seq(140.0, 44100);

edm::Pattern bass(16);
bass.set(0, 45, 0.8f, 2);  // A2, gate 2 steps
bass.set(4, 45, 0.8f, 2);
bass.set(8, 45, 0.8f, 2);
bass.set(12, 45, 0.8f, 2);

edm::SynthVoiceConfig config;
config.wave = edm::Wave::Saw;
config.filter_cutoff_hz = 800.0;
seq.add_track(edm::make_synth_voice(config, 44100), std::move(bass));

edm::Buffer out(seq.frames_for_bars(2));
seq.render(out.data());
edm::write_wav("loop.wav", out.data(), 44100);
```

## v0.0.2 — SFX

- `edm::SfxParams` — wave + linear frequency slide + AD envelope,
  plain struct, designated initializers.
- `edm::render_sfx` — renders an effect into a caller-sized span.
- Presets `edm::sfx::jump`, `coin`, `laser`, `explosion` — the
  spike's proven values.
- `edm::randomize_sfx` — sfxr-style random params from a seeded Rng.

## v0.0.1 — the synth core

- `edm::Rng` — deterministic xorshift32, seeded. Same seed, same stream.
- `edm::Oscillator<Wave>` — template over `Sine`, `Square`, `Saw`,
  `Noise`. Compile-time wave selection, no macros.
- `edm::Envelope` — AD (one-shot) and ADSR (gated), per-sample.
- `edm::Biquad` — RBJ lowpass, cutoff/Q settable per sample (sweeps).
  `edm::Highpass` — one-pole.
- `edm::Buffer` — owning PCM float buffer; views are `std::span<float>`.
- `edm::write_wav` — 16-bit mono WAV writer.

Rules the core follows: C++20, `.hxx` headers + `.cxx` sources,
templates instead of macros, smart pointers / RAII (no raw `new` /
`delete` anywhere), render functions take a destination span and never
allocate, no exceptions in the DSP path, no global mutable state.

## Build & test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Requires CMake 3.24+ and a C++20 compiler (gcc 13+ / clang 18+
verified). Tests use GoogleTest, fetched automatically at configure
time (needs network). No Python needed anywhere.

The examples render a one-second tone, the SFX presets, and a
2-bar sequenced loop:

```sh
./build/tone tone.wav
./build/sfx_demo out/
./build/sequence_demo out/
```

## License

MIT. See [LICENSE](LICENSE).
