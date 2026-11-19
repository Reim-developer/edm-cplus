#pragma once

// SFX: sfxr-style parameterized one-shot effects — the game-audio
// front-end's first layer. Each effect is a few numbers: a wave, a
// linear frequency slide, and an AD envelope (attack ramp + fade-out
// over the last `decay_seconds` of the effect).
//
// Ported from the game-audio spike (spike-game-audio/spike.cpp), which
// proved the presets and the synthesis with real WAV output.
// Deterministic given params + seed: noise consumes the passed Rng,
// never hidden state.
//
// Render functions take a destination span and never allocate; no
// exceptions in the DSP path.

#include <cstddef>
#include <span>

#include "edm/oscillator.hxx"
#include "edm/rng.hxx"

namespace edm {

// Plain struct, designated initializers, no macros.
struct SfxParams {
  Wave wave;
  double f0_hz;         // start frequency
  double f1_hz;         // end frequency (linear slide f0 -> f1)
  double seconds;       // duration
  double volume;        // 0..1
  double attack_seconds; // ramp-up to full volume
  double decay_seconds; // fade-out over the last decay_seconds
};

// Frame count for the params at the given sample rate.
[[nodiscard]] std::size_t sfx_frames(const SfxParams& params, double sample_rate) noexcept;

// Render the effect into a caller-sized span (size it with
// sfx_frames). The slide and envelope run over the span's length;
// noise consumes rng. Deterministic given params + seed.
void render_sfx(const SfxParams& params, Rng& rng, std::span<float> out,
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
