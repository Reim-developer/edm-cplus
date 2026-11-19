# edm-cplus

[![CI](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml/badge.svg)](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml)

Create electronic dance music with C++ code.

v0.0.2 adds SFX — the game-audio front-end's first layer — on top of
the v0.0.1 synth core. No sequencer, arrangement, or playback yet.
Those come as later versions, built on this core.

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

The examples render a one-second tone and the SFX presets:

```sh
./build/tone tone.wav
./build/sfx_demo out/
```

## License

MIT. See [LICENSE](LICENSE).
