// SupersawVoice implementation. See supersaw_voice.hxx.

#include "edm/supersaw_voice.hxx"

#include <cmath>

namespace edm {

SupersawVoice::SupersawVoice(SupersawVoiceConfig config, double sample_rate) noexcept
    : config_(config),
      sample_rate_(sample_rate > 0.0 ? sample_rate : kDefaultSampleRate) {
  const double attack = config.attack_seconds > 0.0 ? config.attack_seconds : 0.0;
  const double decay = config.decay_seconds > 0.0 ? config.decay_seconds : 0.0;
  // The envelope ends at attack + 6*decay (exp(-6) ~= 0.0025 — the
  // hard cut there is inaudible under the next note's retrigger).
  total_ = static_cast<std::size_t>((attack + 6.0 * decay) * sample_rate_);
}

void SupersawVoice::note_on(int midi, float velocity) noexcept {
  const double freq = midi_to_freq(midi);
  for (std::size_t i = 0; i < kSupersawVoices; ++i) {
    // reset() first: set_frequency does NOT reset the phase, and a
    // retrigger must start all 7 saws clean (consecutive identical
    // notes render bit-identically).
    saws_[i].reset();
    saws_[i].set_frequency(freq * std::pow(2.0, kSupersawDetuneCents[i] / 1200.0),
                           sample_rate_);
  }
  index_ = 0;
  velocity_ = velocity;
  active_ = true;
}

void SupersawVoice::note_off(int /*midi*/) noexcept {
  active_ = false;  // hard cut — the envelope has no release stage
}

float SupersawVoice::next() noexcept {
  if (!active_ || index_ >= total_) return 0.0f;
  const double t = static_cast<double>(index_) / sample_rate_;
  // Plucky envelope: linear attack, exponential decay. Non-positive
  // attack/decay mean "instant" / "no decay".
  const double attack_env = (config_.attack_seconds > 0.0)
                                ? std::min(1.0, t / config_.attack_seconds)
                                : 1.0;
  const double decay_env = (config_.decay_seconds > 0.0)
                               ? std::exp(-t / config_.decay_seconds)
                               : 1.0;
  float sum = 0.0f;
  for (auto& saw : saws_) sum += saw.next();
  ++index_;
  return static_cast<float>(sum * (kSupersawGain / static_cast<double>(kSupersawVoices)) *
                            attack_env * decay_env * config_.volume * velocity_);
}

void SupersawVoice::reset() noexcept {
  for (auto& saw : saws_) saw.reset();
  index_ = 0;
  velocity_ = 0.0f;
  active_ = false;
}

} // namespace edm
