// SFX implementation. See sfx.hxx.

#include "edm/sfx.hxx"

#include <array>

#include "edm/plugins.hxx"

namespace edm {

std::size_t sfx_frames(const SfxParams& params, double sample_rate) noexcept {
  if (params.seconds <= 0.0 || sample_rate <= 0.0) return 0;
  return static_cast<std::size_t>(params.seconds * sample_rate);
}

void render_sfx(SfxParams params, Rng rng, std::span<float> out,
                double sample_rate) noexcept {
  // One switch per call, never per sample: dispatch to the
  // compile-time Slide<W> instantiation matching the runtime wave.
  // (Same shape as the make_synth_voice factory.)
  switch (params.wave) {
    case Wave::Sine: {
      plugins::Slide<Wave::Sine> gen(params, rng, sample_rate);
      render_sfx(gen, out);
      break;
    }
    case Wave::Square: {
      plugins::Slide<Wave::Square> gen(params, rng, sample_rate);
      render_sfx(gen, out);
      break;
    }
    case Wave::Saw: {
      plugins::Slide<Wave::Saw> gen(params, rng, sample_rate);
      render_sfx(gen, out);
      break;
    }
    case Wave::Noise: {
      plugins::Slide<Wave::Noise> gen(params, rng, sample_rate);
      render_sfx(gen, out);
      break;
    }
  }
}

SfxParams randomize_sfx(Rng& rng) noexcept {
  constexpr std::array<Wave, 4> kWaves{Wave::Sine, Wave::Square, Wave::Saw, Wave::Noise};
  auto uniform = [&rng](double lo, double hi) { return lo + (hi - lo) * rng.uniform(); };
  SfxParams params;
  params.wave = kWaves[static_cast<std::size_t>(rng.uniform() * 4.0)];
  params.f0_hz = uniform(100.0, 1200.0);
  params.f1_hz = uniform(50.0, 1500.0);
  params.seconds = uniform(0.1, 0.8);
  params.volume = uniform(0.3, 0.8);
  params.attack_seconds = uniform(0.001, 0.05);
  params.decay_seconds = uniform(0.05, 0.5);
  return params;
}

} // namespace edm
