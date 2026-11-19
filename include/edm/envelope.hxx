#pragma once

// AD and ADSR envelopes, per-sample, no allocation. Levels are computed
// from per-stage sample counters (not accumulated floats), so stage
// boundaries land exactly: attack reaches 1.0 on its last sample,
// decay reaches sustain on its last sample, release reaches 0.
//
// AD (one-shot): trigger_ad() runs attack -> decay -> idle.
// ADSR (gated): set_adsr() configures, note_on() starts attack, the
// envelope holds at sustain until note_off() starts the release.
// Zero-length stages complete immediately.

#include <cstddef>
#include <span>

namespace edm {

class Envelope {
 public:
  Envelope() = default;

  // One-shot AD: attack to 1.0, then decay to 0, then idle.
  void trigger_ad(double attack_seconds, double decay_seconds, double sample_rate) noexcept {
    sustain_ = 0.0;
    attack_samples_ = to_samples(attack_seconds, sample_rate);
    decay_samples_ = to_samples(decay_seconds, sample_rate);
    release_samples_ = 0;
    stage_ = Stage::Attack;
    stage_samples_ = 0;
    level_ = 0.0f;
  }

  // Configure ADSR. Does not start the envelope; call note_on().
  void set_adsr(double attack_seconds, double decay_seconds, double sustain_level,
                double release_seconds, double sample_rate) noexcept {
    sustain_ = sustain_level < 0.0 ? 0.0 : (sustain_level > 1.0 ? 1.0 : sustain_level);
    attack_samples_ = to_samples(attack_seconds, sample_rate);
    decay_samples_ = to_samples(decay_seconds, sample_rate);
    release_samples_ = to_samples(release_seconds, sample_rate);
  }

  void note_on() noexcept {
    stage_ = Stage::Attack;
    stage_samples_ = 0;
    level_ = 0.0f;
  }

  void note_off() noexcept {
    if (stage_ == Stage::Attack || stage_ == Stage::Decay || stage_ == Stage::Sustain) {
      stage_ = Stage::Release;
      stage_samples_ = 0;
      release_from_ = level_;
    }
  }

  float next() noexcept {
    switch (stage_) {
      case Stage::Idle:
        return 0.0f;
      case Stage::Attack: {
        ++stage_samples_;
        if (attack_samples_ == 0 || stage_samples_ >= attack_samples_) {
          level_ = 1.0f;
          stage_ = Stage::Decay;
          stage_samples_ = 0;
        } else {
          level_ = static_cast<float>(static_cast<double>(stage_samples_) /
                                      static_cast<double>(attack_samples_));
        }
        return level_;
      }
      case Stage::Decay: {
        ++stage_samples_;
        if (decay_samples_ == 0 || stage_samples_ >= decay_samples_) {
          level_ = static_cast<float>(sustain_);
          stage_ = sustain_ > 0.0 ? Stage::Sustain : Stage::Idle;
          stage_samples_ = 0;
        } else {
          level_ = static_cast<float>(1.0 - (1.0 - sustain_) *
                                                (static_cast<double>(stage_samples_) /
                                                 static_cast<double>(decay_samples_)));
        }
        return level_;
      }
      case Stage::Sustain:
        return static_cast<float>(sustain_);
      case Stage::Release: {
        ++stage_samples_;
        if (release_samples_ == 0 || stage_samples_ >= release_samples_) {
          level_ = 0.0f;
          stage_ = Stage::Idle;
          stage_samples_ = 0;
        } else {
          level_ = static_cast<float>(release_from_ *
                                      (1.0 - static_cast<double>(stage_samples_) /
                                                 static_cast<double>(release_samples_)));
        }
        return level_;
      }
    }
    return 0.0f; // unreachable
  }

  // Render the envelope curve into a destination span. Never allocates.
  void render(std::span<float> out) noexcept {
    for (float& s : out) s = next();
  }

  [[nodiscard]] float level() const noexcept { return level_; }
  [[nodiscard]] bool active() const noexcept { return stage_ != Stage::Idle; }

  void reset() noexcept {
    stage_ = Stage::Idle;
    stage_samples_ = 0;
    level_ = 0.0f;
  }

 private:
  enum class Stage { Idle, Attack, Decay, Sustain, Release };

  static std::size_t to_samples(double seconds, double sample_rate) noexcept {
    if (seconds <= 0.0 || sample_rate <= 0.0) return 0;
    return static_cast<std::size_t>(seconds * sample_rate);
  }

  Stage stage_ = Stage::Idle;
  std::size_t stage_samples_ = 0;
  std::size_t attack_samples_ = 0;
  std::size_t decay_samples_ = 0;
  std::size_t release_samples_ = 0;
  double sustain_ = 0.0;
  double release_from_ = 0.0;
  float level_ = 0.0f;
};

} // namespace edm
