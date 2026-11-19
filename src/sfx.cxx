// SFX implementation. See sfx.hxx.

#include "edm/sfx.hxx"

#include <algorithm>
#include <array>
#include <cmath>

#include "edm/plugins.hxx"

namespace edm {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The spike's waveSample: sine is sin(2*pi*phase), square is +/-0.8,
// saw is a 2*phase-1 ramp, noise is uniform in [-1, 1). The library's
// Oscillator template selects the wave at compile time; SFX params
// carry the wave at runtime, so this is a switch.
double wave_sample(Wave wave, double phase, Rng& rng) noexcept {
  switch (wave) {
    case Wave::Sine:
      return std::sin(2.0 * kPi * phase);
    case Wave::Square:
      return phase < 0.5 ? 0.8 : -0.8;
    case Wave::Saw:
      return 2.0 * phase - 1.0;
    case Wave::Noise:
      return rng.uniform() * 2.0 - 1.0;
  }
  return 0.0;
}

} // namespace

std::size_t sfx_frames(const SfxParams& params, double sample_rate) noexcept {
  if (params.seconds <= 0.0 || sample_rate <= 0.0) return 0;
  return static_cast<std::size_t>(params.seconds * sample_rate);
}

SlideSfx::SlideSfx(SfxParams params, Rng rng, double sample_rate) noexcept
    : params_(params), rng_(rng), sample_rate_(sample_rate),
      total_(sfx_frames(params, sample_rate)) {}

float SlideSfx::next() noexcept {
  // The v0.0.2 render loop, one sample at a time: the slide runs
  // over the effect's own frame count (total_), so a span longer
  // than the effect renders silence after the effect ends instead
  // of stretching it.
  if (index_ >= total_) return 0.0f;
  const double t = static_cast<double>(index_) / sample_rate_;
  const double frac = static_cast<double>(index_) / static_cast<double>(total_);
  const double freq = params_.f0_hz + (params_.f1_hz - params_.f0_hz) * frac;
  phase_ += freq / sample_rate_;
  if (phase_ >= 1.0) phase_ -= 1.0;
  double env = 1.0;
  if (params_.attack_seconds > 0.0 && t < params_.attack_seconds) {
    env = t / params_.attack_seconds;
  }
  const double tail = params_.seconds - t;
  if (params_.decay_seconds > 0.0 && tail < params_.decay_seconds) {
    env = std::min(env, tail / params_.decay_seconds);
  }
  ++index_;
  return static_cast<float>(wave_sample(params_.wave, phase_, rng_) * params_.volume * env);
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
