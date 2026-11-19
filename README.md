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

## SFX — plugins, not presets

An SFX is anything with `float next() noexcept`. The library's only
job is to pull samples from a generator you define into a buffer —
it never knows what the sound is, because the sound lives entirely
in your code:

```cpp
#include <cmath>

#include "edm/buffer.hxx"
#include "edm/sfx.hxx"
#include "edm/wav.hxx"

// Your generator — the library has no idea what a "blip" is.
struct Blip {
  double phase = 0.0;
  double increment = 660.0 / 44100;
  float next() noexcept {
    const float v = static_cast<float>(std::sin(2.0 * 3.141592653589793 * phase));
    phase += increment;
    if (phase >= 1.0) phase -= 1.0;
    return v;
  }
};

edm::Buffer out(4410);
Blip blip;
edm::render_sfx(blip, out.data());
edm::write_wav("blip.wav", out.data(), 44100);
```

`edm::plugins::Slide<Wave>` is one built-in plugin (oscillator +
linear frequency slide + AD envelope — the old preset model), with
the wave selected at compile time. `edm::SfxParams` is its
parameter bundle, `edm::sfx_frames` sizes the buffer,
`edm::randomize_sfx` makes sfxr-style random params, and the
params-based `render_sfx` overload renders it for you. The named
presets (`jump`, `coin`, `laser`, `explosion`) left the library in
0.0.4 — they live as local functions in `examples/sfx_demo.cxx`.
See the Plugins section below for writing your own.

## Plugins — `edm::plugins`

The plugin system is the extension point for all user-extensible
sound content: game SFX today, EDM voices and effects tomorrow.
Two ways to write one:

1. **Satisfy the concept** — any struct with `float next() noexcept`
   renders through `edm::render_sfx`, no inheritance needed (the
   Blip above). Good for trivial generators.
2. **Inherit the CRTP base** — `edm::plugins::Plugin<Derived>` owns
   the frame counter, so silence after the duration and `finished()`
   come for free; you implement only `next_impl()`:

```cpp
#include <cmath>

#include "edm/buffer.hxx"
#include "edm/plugins.hxx"
#include "edm/sfx.hxx"
#include "edm/wav.hxx"

// A plugin: inherit Plugin<Derived>, implement next_impl().
// The base handles the duration — no reset() needed, fresh
// construction is the reset.
struct Ping : public edm::plugins::Plugin<Ping> {
  double phase = 0.0;
  double increment = 880.0 / 44100;
  Ping(double sample_rate, double seconds)
      : edm::plugins::Plugin<Ping>(sample_rate, seconds) {}
  float next_impl() noexcept {
    const float v = static_cast<float>(std::sin(2.0 * 3.141592653589793 * phase));
    phase += increment;
    if (phase >= 1.0) phase -= 1.0;
    return v;
  }
};

edm::Buffer out(4410);
Ping ping(44100, 0.1);
edm::render_sfx(ping, out.data());
edm::write_wav("ping.wav", out.data(), 44100);
```

Why templates here, and the `Voice` interface for notes? An SFX
render call takes ONE sound, so compile-time polymorphism costs
nothing and inlines — no vtable, no indirection. The CRTP base is
the same idea for shared machinery: inheritance that shares code
without dispatching at runtime. The Sequencer holds heterogeneous
voices in ONE container, so notes need runtime polymorphism
(`Voice` + `std::unique_ptr`). Two mechanisms, each for a
principled reason.

Templates are preferred wherever they remove overhead, even when
they grow the binary — an extra fraction of a MiB is not a
constraint for this library (it is not written for embedded or
resource-limited targets).

## What is in the library

- **Synth core** — `edm::Rng` (deterministic xorshift32),
  `edm::Oscillator<Wave>` (Sine / Square / Saw / Noise),
  `edm::Envelope` (AD / ADSR), `edm::Biquad` (RBJ lowpass) +
  `edm::Highpass` (one-pole), `edm::Buffer` (owning PCM float
  buffer), `edm::write_wav` (16-bit mono WAV writer).
- **SFX** — plugins, not presets: any `float next() noexcept`
  generator renders through `edm::render_sfx`;
  `edm::plugins::Slide<Wave>` is one built-in plugin and
  `edm::plugins::Plugin<Derived>` the CRTP base for writing your
  own. See the SFX and Plugins sections above.
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
