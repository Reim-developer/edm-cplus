#pragma once

// Oscillator with compile-time wave selection: Oscillator<Wave::Sine>,
// Oscillator<Wave::Square>, Oscillator<Wave::Saw>, Oscillator<Wave::Noise>.
// This is the templates-instead-of-macros rule in action — the wave is a
// template argument, not a runtime switch or a macro.
//
// Phase accumulation, sample-accurate. Noise consumes randomness from an
// explicit, seeded Rng passed by the caller: determinism must be visible,
// so Noise has no hidden internal RNG. Non-noise waves ignore the Rng.
//
// Waveforms are the spike's proven ones: sine is sin(2*pi*phase), square
// is +/-0.8, saw is a 2*phase-1 ramp, noise is uniform in [-1, 1).

#include <cmath>
#include <cstddef>
#include <span>

#include "edm/rng.hxx"

namespace edm {

enum class Wave { Sine, Square, Saw, Noise };

template <Wave W>
class Oscillator {
 public:
  Oscillator() = default;

  // Frequency in Hz at the given sample rate. Non-positive values silence
  // the phase advance (the oscillator holds its current phase).
  void set_frequency(double hz, double sample_rate) noexcept {
    frequency_ = hz;
    sample_rate_ = sample_rate;
    increment_ = (hz > 0.0 && sample_rate > 0.0) ? hz / sample_rate : 0.0;
  }

  void reset() noexcept { phase_ = 0.0; }

  [[nodiscard]] double frequency() const noexcept { return frequency_; }
  [[nodiscard]] double sample_rate() const noexcept { return sample_rate_; }
  [[nodiscard]] double phase() const noexcept { return phase_; }

  // Single sample. Only available for non-noise waves; Noise needs an Rng.
  float next() noexcept requires (W != Wave::Noise) { return sample(); }

  // Single sample for any wave. Only Noise consumes randomness.
  float next(Rng& rng) noexcept { return sample(rng); }

  // Render into a destination span. Never allocates.
  void render(std::span<float> out) noexcept requires (W != Wave::Noise) {
    for (float& s : out) s = sample();
  }

  void render(std::span<float> out, Rng& rng) noexcept {
    for (float& s : out) s = sample(rng);
  }

 private:
  static constexpr double kPi = 3.14159265358979323846;

  float sample() noexcept {
    if constexpr (W == Wave::Sine) {
      const float v = static_cast<float>(std::sin(2.0 * kPi * phase_));
      advance();
      return v;
    } else if constexpr (W == Wave::Square) {
      const float v = phase_ < 0.5 ? 0.8f : -0.8f;
      advance();
      return v;
    } else if constexpr (W == Wave::Saw) {
      const float v = static_cast<float>(2.0 * phase_ - 1.0);
      advance();
      return v;
    } else {
      // Noise without an Rng is not a thing: next() is constrained away
      // for Noise, so this branch is unreachable. Return silence.
      return 0.0f;
    }
  }

  float sample(Rng& rng) noexcept {
    if constexpr (W == Wave::Noise) {
      return rng.bipolar();
    } else {
      (void)rng; // only Noise consumes randomness
      return sample();
    }
  }

  void advance() noexcept {
    phase_ += increment_;
    if (phase_ >= 1.0) phase_ -= 1.0;
  }

  double phase_ = 0.0;
  double increment_ = 0.0;
  double frequency_ = 0.0;
  double sample_rate_ = 44100.0;
};

} // namespace edm
