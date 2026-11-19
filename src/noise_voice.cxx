// NoiseVoice implementation. See noise_voice.hxx.

#include "edm/noise_voice.hxx"

#include <cmath>
#include <cstdint>

namespace edm {
namespace {

// The clap texture: burst spacing and the gate window, from the
// spike's clap (3 bursts, 12 ms apart, inside the first 40 ms).
constexpr double kNoiseBurstSpacingSeconds = 0.012;
constexpr double kNoiseBurstWindowSeconds = 0.04;
constexpr double kNoiseOddBurstGate = 0.25;

} // namespace

NoiseVoice::NoiseVoice(NoiseVoiceConfig config, double sample_rate) noexcept
    : config_(config),
      sample_rate_(sample_rate > 0.0 ? sample_rate : kDefaultSampleRate),
      rng_(static_cast<std::uint32_t>(config.seed)),
      rng_initial_(static_cast<std::uint32_t>(config.seed)) {
  highpass_.set(config.highpass_cutoff_hz, sample_rate_);
  total_ = (config.seconds > 0.0)
               ? static_cast<std::size_t>(config.seconds * sample_rate_)
               : 0;
}

void NoiseVoice::note_on(int /*midi*/, float velocity) noexcept {
  highpass_.reset();
  index_ = 0;
  velocity_ = velocity;
  active_ = true;
}

void NoiseVoice::note_off(int /*midi*/) noexcept {
  // One-shot: the gate does not cut a noise hit short.
}

float NoiseVoice::next() noexcept {
  if (!active_ || index_ >= total_) return 0.0f;
  const double t = static_cast<double>(index_) / sample_rate_;
  const double env = (config_.decay_seconds > 0.0)
                         ? std::exp(-t / config_.decay_seconds)
                         : 1.0;
  // Burst gate (clap texture): alternate full/quarter volume while
  // inside the burst window. bursts == 1 disables it.
  double gate = 1.0;
  if (config_.bursts > 1 && t < kNoiseBurstWindowSeconds) {
    const int burst = static_cast<int>(t / kNoiseBurstSpacingSeconds);
    gate = (burst % 2 == 0) ? 1.0 : kNoiseOddBurstGate;
  }
  ++index_;
  return static_cast<float>(highpass_.process(rng_.bipolar()) * env * gate *
                            config_.volume * velocity_);
}

void NoiseVoice::reset() noexcept {
  rng_ = rng_initial_;
  highpass_.reset();
  index_ = 0;
  velocity_ = 0.0f;
  active_ = false;
}

} // namespace edm
