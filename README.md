# edm-cplus

[![CI](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml/badge.svg)](https://github.com/Reim-developer/edm-cplus/actions/workflows/ci.yml)

Create electronic dance music with C++ code.

A C++20 library for making music with code: a synth core, one-shot
sound effects, a sequencer that schedules patterns at a tempo, and
arrangements that assemble named sections into a song. No playback
yet — that comes as a later version.
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
config.filter_q = 0.7;  // resonance (default 0.7)
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

## Arrangement — sections, automation, sidechain

A song is an `edm::Arrangement`: named sections rendered in order
at one BPM. A section is a span of bars with its own sequencer —
the name is yours ("intro", "drop"), not an enum; the library
provides the mechanism, you name the parts. Every section's
sequencer must be built at the arrangement's BPM (there is no
check — a mismatched section plays at the wrong tempo).

```cpp
#include <memory>
#include <utility>

#include "edm/arrangement.hxx"
#include "edm/buffer.hxx"
#include "edm/constants.hxx"
#include "edm/kick_voice.hxx"
#include "edm/pattern.hxx"
#include "edm/section.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"
#include "edm/wav.hxx"

// One section: 2 bars, kick + filtered bass, the cutoff opening
// across the section, the music ducking on every kick.
edm::Sequencer seq(128.0, 44100);
edm::Pattern kick(16);
for (int s = 0; s < 16; s += 4) kick.set(s, 36, 1.0f);
seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, 44100),
              std::move(kick));

edm::SynthVoiceConfig bass;
bass.wave = edm::Wave::Saw;
bass.filter_cutoff_hz = 500.0;
edm::Pattern bass_notes(16);
for (int s = 2; s < 16; s += 4) bass_notes.set(s, 38, 1.0f, 2);
seq.add_track(edm::make_synth_voice(bass, 44100), std::move(bass_notes));

edm::Section drop("drop", 2, std::move(seq));
drop.bind_filter_cutoff(1, edm::Automation({{0.0, 500.0}, {2.0, 6000.0}},
                                           edm::Curve::Exponential));
drop.enable_sidechain(0);

edm::Arrangement song(128.0, 44100);
song.add_section(std::move(drop));
edm::Buffer out(song.frames());
song.render(out.data());
out.normalize(edm::kMixNormalizePeak);
edm::write_wav("song.wav", out.data(), 44100);
```

**Automation** is a curve over bar positions — `Linear` or
`Exponential` (the right shape for filter sweeps, since pitch is
logarithmic) — bound to a track's volume or a filterable voice's
cutoff. Bindings are evaluated once per 64-frame block, so a
sweep moves smoothly without a per-sample recompute. Binding
again replaces the previous curve; binding to a bad track, an
empty curve, or a non-filterable voice returns `false`.

**Sidechain** is per section: the section renders twice (kick-only
bus, then music-only bus, each from a reset sequencer) and mixes
kick + ducked music, with the gain curve read from the kick
pattern's note-on times (`1 - depth * exp(-t/release)`). The
ducking is uniform across the whole music bus — the spike this
round was built from ducks the lead less than the bass, but the
library has one bus and one curve; per-track depth belongs to a
future version if anyone asks.

**Patterns may be any length.** A pattern shorter than its section
loops; a pattern as long as the section plays exactly once. That
is how a snare roll in the last bar of a build is written: a
64-step pattern over a 4-bar section, notes only in the last 16
steps.

**Four EDM voices** ship with this round, all one-shot (note_off
is a no-op except the supersaw's hard cut) and all deterministic:

- `edm::KickVoice` — sine with a fast exponential pitch drop
  (150 → 45 Hz) and decay, 0.30 s.
- `edm::SupersawVoice` — 7 detuned saws under a plucky
  one-shot envelope (not the ADSR `Envelope` — a lead note
  decays away on its own).
- `edm::NoiseVoice` — config-driven hats / clap / snare: the
  kind of sound is data (filter cutoff, decay, duration,
  bursts), not a type.
- `edm::RiserVoice` — noise through a lowpass sweeping
  exponentially (400 → 8000 Hz) over a caller-computed duration.

**Known limits of this round:** voices do not ring across
section boundaries (each section resets its sequencer — tails
belong to the future effects layer); the lead is dry (no delay
yet — same round); sidechain ducking is uniform (see above).
`examples/arrangement_demo.cxx` rebuilds the 32-bar Unity-style
track with the library API (60 s, sections intro / build / drop /
breakdown / build2 / drop2) — run it and listen.

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
  sample-accurate step boundaries, gate-aware note on/off,
  per-track mute. `edm::midi_to_freq<Hz>` converts MIDI notes
  (default A4 = 440 Hz; `midi_to_freq<432.0>` for alternate
  concert pitch).
- **Arrangement** — `edm::Section` (a named span of bars with
  its own sequencer), `edm::Automation` (linear / exponential
  parameter curves over bar positions), `edm::Arrangement`
  (sections in order at one BPM); sidechain ducking per section;
  four EDM voices — `edm::KickVoice`, `edm::SupersawVoice`,
  `edm::NoiseVoice` (config-driven hats / clap / snare),
  `edm::RiserVoice`; `edm::Buffer::normalize` for the master
  peak. See the Arrangement section above.

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
./build/arrangement_demo unity_style.wav
```

`tone` renders a one-second tone, `sfx_demo` the SFX presets plus a
randomized effect, `sequence_demo` a 2-bar sequenced loop,
`arrangement_demo` a 32-bar Unity-style track (60 s).

## License

MIT. See [LICENSE](LICENSE).
