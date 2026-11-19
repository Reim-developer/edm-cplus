// KickVoice implementation. See kick_voice.hxx.

#include "edm/kick_voice.hxx"

#include <cmath>

#include "edm/constants.hxx"

namespace edm {

KickVoice::KickVoice(KickVoiceConfig config, double sample_rate) noexcept
    : config_(config),
      sample_rate_(sample_rate > 0.0 ? sample_rate : kDefaultSampleRate) {
  total_ = (config.seconds > 0.0)
               ? static_cast<std::size_t>(config.seconds * sample_rate_)
               : 0;
}

void KickVoice::note_on(int /*midi*/, float velocity) noexcept {
  oscillator_.reset();
  index_ = 0;
  velocity_ = velocity;
  active_ = true;
}

void KickVoice::note_off(int /*midi*/) noexcept {
  // One-shot: the gate does not cut a kick short.
}

float KickVoice::next() noexcept {
  if (!active_ || index_ >= total_) return 0.0f;
  const double t = static_cast<double>(index_) / sample_rate_;
  // Pitch drop: f1 + (f0 - f1) * exp(-t/slide). A non-positive slide
  // means "already at f1".
  const double f = (config_.slide_seconds > 0.0)
                       ? config_.f1_hz + (config_.f0_hz - config_.f1_hz) *
                                             std::exp(-t / config_.slide_seconds)
                       : config_.f1_hz;
  // Decay: exp(-t/decay). A non-positive decay means "no decay".
  const double env = (config_.decay_seconds > 0.0)
                         ? std::exp(-t / config_.decay_seconds)
                         : 1.0;
  oscillator_.set_frequency(f, sample_rate_);
  ++index_;
  return static_cast<float>(oscillator_.next() * env * config_.volume * velocity_);
}

void KickVoice::reset() noexcept {
  oscillator_.reset();
  index_ = 0;
  velocity_ = 0.0f;
  active_ = false;
}

} // namespace edm
