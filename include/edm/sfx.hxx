#pragma once

// SFX: one-shot effects, plugin-style. An SFX is anything that
// produces one sample at a time — the library's only job is to pull
// samples from a user-supplied generator into a buffer. The
// generator — owned and defined by the user — IS the plugin. The
// library never knows what the sound is, because the sound lives
// entirely in user code.
//
// A game has more SFX kinds than the library can enumerate, so the
// library does not try: the v0.0.2 slide model survives as ONE
// built-in generator, SlideSfx — one plugin among many, not the
// definition of SFX.
//
// The generic render path is a template, not a virtual interface:
// an SFX render call takes ONE sound, so compile-time polymorphism
// costs nothing and inlines. Notes are different: the Sequencer
// holds heterogeneous voices in ONE container, so they need the
// runtime Voice interface + unique_ptr. Two mechanisms, each for a
// principled reason — see the README.
//
// Render functions take a destination span and never allocate; no
// exceptions in the DSP path.

#include <concepts>
#include <cstddef>
#include <span>

#include "edm/oscillator.hxx"
#include "edm/rng.hxx"

namespace edm {

// The plugin contract: anything with a noexcept next() that yields
// a float is an SFX generator.
template <typename T>
concept SampleGenerator = requires(T& g) {
  { g.next() } noexcept -> std::convertible_to<float>;
};

// The generic render path — deliberately trivial: pull samples from
// any generator into the buffer. Zero-cost: the template inlines
// the generator, no vtable, no indirection.
template <SampleGenerator Generator>
void render_sfx(Generator&& gen, std::span<float> out) noexcept {
  for (float& sample : out) sample = gen.next();
}

// Plain struct, designated initializers, no macros. The parameter
// bundle for the SlideSfx model only — numbers, not a sound type.
struct SfxParams {
  Wave wave;
  double f0_hz;          // start frequency
  double f1_hz;          // end frequency (linear slide f0 -> f1)
  double seconds;        // duration
  double volume;         // 0..1
  double attack_seconds; // ramp-up to full volume
  double decay_seconds;  // fade-out over the last decay_seconds
};

// Frame count for the params at the given sample rate.
[[nodiscard]] std::size_t sfx_frames(const SfxParams& params, double sample_rate) noexcept;

// SlideSfx: oscillator + linear frequency slide + AD envelope —
// the v0.0.2 model, now ONE generator. Deterministic given params +
// seed: noise consumes the passed Rng, never hidden state. Once the
// effect's duration has elapsed, next() returns silence.
class SlideSfx {
 public:
  SlideSfx(SfxParams params, Rng rng, double sample_rate) noexcept;

  float next() noexcept;

 private:
  SfxParams params_;
  Rng rng_;
  double sample_rate_;
  std::size_t total_ = 0; // frames in the effect
  std::size_t index_ = 0; // frames produced so far
  double phase_ = 0.0;
};

// Convenience overload: render the slide model into a caller-sized
// span (size it with sfx_frames). Builds a SlideSfx and renders
// through the generic path. Different arity from the template
// overload, so no overload conflict.
void render_sfx(SfxParams params, Rng rng, std::span<float> out,
                double sample_rate) noexcept;

// sfxr-style random params from a seeded Rng (deterministic given
// the seed). Ranges: wave in {Sine, Square, Saw, Noise}, f0 in
// [100, 1200] Hz, f1 in [50, 1500] Hz, seconds in [0.1, 0.8],
// volume in [0.3, 0.8], attack in [0.001, 0.05] s, decay in
// [0.05, 0.5] s.
[[nodiscard]] SfxParams randomize_sfx(Rng& rng) noexcept;

namespace sfx {

// Presets — plain functions returning SfxParams, the spike's proven
// values.
[[nodiscard]] SfxParams jump();
[[nodiscard]] SfxParams coin();
[[nodiscard]] SfxParams laser();
[[nodiscard]] SfxParams explosion();

} // namespace sfx
} // namespace edm
