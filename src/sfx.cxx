// SFX implementation. See sfx.hxx.

#include "edm/sfx.hxx"

#include <algorithm>
#include <array>
#include <cmath>

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

void render_sfx(const SfxParams& params, Rng& rng, std::span<float> out,
                double sample_rate) noexcept {
  if (out.empty() || sample_rate <= 0.0) return;
  const std::size_t n = out.size();
  double phase = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sample_rate;
    const double frac = static_cast<double>(i) / static_cast<double>(n);
    const double freq = params.f0_hz + (params.f1_hz - params.f0_hz) * frac;
    phase += freq / sample_rate;
    if (phase >= 1.0) phase -= 1.0;
    double env = 1.0;
    if (params.attack_seconds > 0.0 && t < params.attack_seconds) {
      env = t / params.attack_seconds;
    }
    const double tail = params.seconds - t;
    if (params.decay_seconds > 0.0 && tail < params.decay_seconds) {
      env = std::min(env, tail / params.decay_seconds);
    }
    out[i] = static_cast<float>(wave_sample(params.wave, phase, rng) * params.volume * env);
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

namespace sfx {

SfxParams jump() {
  return SfxParams{.wave = Wave::Square,
                   .f0_hz = 300.0,
                   .f1_hz = 900.0,
                   .seconds = 0.25,
                   .volume = 0.5,
                   .attack_seconds = 0.01,
                   .decay_seconds = 0.08};
}

SfxParams coin() {
  return SfxParams{.wave = Wave::Sine,
                   .f0_hz = 988.0,
                   .f1_hz = 1319.0,
                   .seconds = 0.30,
                   .volume = 0.5,
                   .attack_seconds = 0.01,
                   .decay_seconds = 0.10};
}

SfxParams laser() {
  return SfxParams{.wave = Wave::Saw,
                   .f0_hz = 1200.0,
                   .f1_hz = 100.0,
                   .seconds = 0.30,
                   .volume = 0.5,
                   .attack_seconds = 0.01,
                   .decay_seconds = 0.12};
}

SfxParams explosion() {
  return SfxParams{.wave = Wave::Noise,
                   .f0_hz = 800.0,
                   .f1_hz = 60.0,
                   .seconds = 0.60,
                   .volume = 0.7,
                   .attack_seconds = 0.005,
                   .decay_seconds = 0.45};
}

} // namespace sfx
} // namespace edm
