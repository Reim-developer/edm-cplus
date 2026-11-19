// RiserVoice implementation. See riser_voice.hxx.

#include "edm/riser_voice.hxx"

#include <cmath>
#include <cstdint>

namespace edm {
namespace {

// Per-sample lerp factor for the cutoff: the cutoff chases its
// exponential target this fast, so coefficient changes are smooth
// (no zipper) even though set_lowpass runs every sample.
constexpr double kRiserCutoffSmoothing = 0.0002;
// Butterworth Q for the riser lowpass — neutral, no resonance peak.
constexpr double kRiserFilterQ = 0.707;

} // namespace

RiserVoice::RiserVoice(RiserVoiceConfig config, double sample_rate) noexcept
    : config_(config),
      sample_rate_(sample_rate > 0.0 ? sample_rate : kDefaultSampleRate),
      rng_(static_cast<std::uint32_t>(config.seed)),
      rng_initial_(static_cast<std::uint32_t>(config.seed)),
      current_cutoff_hz_(config.f0_hz) {
  total_ = (config.seconds > 0.0)
               ? static_cast<std::size_t>(config.seconds * sample_rate_)
               : 0;
}

void RiserVoice::note_on(int /*midi*/, float velocity) noexcept {
  lowpass_.reset();
  current_cutoff_hz_ = config_.f0_hz;
  index_ = 0;
  velocity_ = velocity;
  active_ = true;
}

void RiserVoice::note_off(int /*midi*/) noexcept {
  // One-shot: the gate does not cut a riser short.
}

float RiserVoice::next() noexcept {
  if (!active_ || index_ >= total_) return 0.0f;
  const double frac = total_ > 0 ? static_cast<double>(index_) / total_ : 0.0;
  // Exponential sweep f0 -> f1. Non-positive endpoints fall back to
  // a fixed f1 (set_lowpass clamps, so nothing can go NaN).
  double target = config_.f1_hz;
  if (config_.f0_hz > 0.0 && config_.f1_hz > 0.0) {
    target = config_.f0_hz * std::pow(config_.f1_hz / config_.f0_hz, frac);
  }
  current_cutoff_hz_ += (target - current_cutoff_hz_) * kRiserCutoffSmoothing;
  lowpass_.set_lowpass(current_cutoff_hz_, kRiserFilterQ, sample_rate_);
  ++index_;
  // Gain rises linearly with the sweep: silent at the start, full
  // volume at the end.
  return static_cast<float>(lowpass_.process(rng_.bipolar()) * config_.volume *
                            frac * velocity_);
}

void RiserVoice::reset() noexcept {
  rng_ = rng_initial_;
  lowpass_.reset();
  current_cutoff_hz_ = config_.f0_hz;
  index_ = 0;
  velocity_ = 0.0f;
  active_ = false;
}

} // namespace edm
