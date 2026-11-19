# edm-cplus

[![CI](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml/badge.svg)](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml)

Create electronic dance music with C++ code.

A C++20 library for making music with code: a synth core, one-shot
sound effects, and a sequencer that schedules patterns at a tempo.
No arrangement or playback yet — those come as later versions.
See [CHANGELOG.md](CHANGELOG.md) for version history.

## Quickstart

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

## What is in the library

- **Synth core** — `edm::Rng` (deterministic xorshift32),
  `edm::Oscillator<Wave>` (Sine / Square / Saw / Noise),
  `edm::Envelope` (AD / ADSR), `edm::Biquad` (RBJ lowpass) +
  `edm::Highpass` (one-pole), `edm::Buffer` (owning PCM float
  buffer), `edm::write_wav` (16-bit mono WAV writer).
- **SFX** — `edm::SfxParams` + `edm::render_sfx`: sfxr-style
  parameterized one-shot effects (wave + linear frequency slide +
  AD envelope); `edm::randomize_sfx` makes random ones from a
  seeded Rng.
- **Sequencer** — `edm::Pattern` (16th-note step loops),
  `edm::Voice` / `edm::SynthVoice` (note interface + oscillator
  voice), `edm::Sequencer`: notes scheduled at a BPM,
  sample-accurate step boundaries, gate-aware note on/off.

Rules the library follows: C++20, `.hxx` headers + `.cxx` sources,
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

## Examples

```sh
./build/tone tone.wav
./build/sfx_demo out/
./build/sequence_demo out/
```

`tone` renders a one-second tone, `sfx_demo` the SFX presets plus a
randomized effect, `sequence_demo` a 2-bar sequenced loop.

## License

MIT. See [LICENSE](LICENSE).
