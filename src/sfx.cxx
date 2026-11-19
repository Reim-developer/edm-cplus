// SFX implementation. See sfx.hxx.

#include "edm/sfx.hxx"

#include <array>

#include "edm/plugins.hxx"

namespace edm {
namespace {
// sfxr-style randomize ranges (documented in sfx.hxx).
constexpr double kRandomF0MinHz = 100.0;
constexpr double kRandomF0MaxHz = 1200.0;
constexpr double kRandomF1MinHz = 50.0;
constexpr double kRandomF1MaxHz = 1500.0;
constexpr double kRandomSecondsMin = 0.1;
constexpr double kRandomSecondsMax = 0.8;
constexpr double kRandomVolumeMin = 0.3;
constexpr double kRandomVolumeMax = 0.8;
constexpr double kRandomAttackMinSeconds = 0.001;
constexpr double kRandomAttackMaxSeconds = 0.05;
constexpr double kRandomDecayMinSeconds = 0.05;
constexpr double kRandomDecayMaxSeconds = 0.5;
} // namespace

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
  params.wave = kWaves[static_cast<std::size_t>(rng.uniform() * static_cast<double>(kWaves.size()))];
  params.f0_hz = uniform(kRandomF0MinHz, kRandomF0MaxHz);
  params.f1_hz = uniform(kRandomF1MinHz, kRandomF1MaxHz);
  params.seconds = uniform(kRandomSecondsMin, kRandomSecondsMax);
  params.volume = uniform(kRandomVolumeMin, kRandomVolumeMax);
  params.attack_seconds = uniform(kRandomAttackMinSeconds, kRandomAttackMaxSeconds);
  params.decay_seconds = uniform(kRandomDecayMinSeconds, kRandomDecayMaxSeconds);
  return params;
}

} // namespace edm
