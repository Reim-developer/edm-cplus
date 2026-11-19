#pragma once

// SupersawVoice: the EDM lead — 7 detuned saws (kSupersawVoices,
// kSupersawDetuneCents) summed at kSupersawGain, under a one-shot
// plucky envelope: min(1, t/attack) * exp(-t/decay).
//
// The envelope is deliberately NOT the ADSR Envelope: a supersaw
// lead note is a pluck — it decays away on its own, and the ADSR
// sustain stage would hold it flat instead. The envelope is driven
// by a sample counter, so it is exact and allocation-free.
//
// The voice carries NO filter: the classic supersaw is already
// bright, and filtering belongs to the section's cutoff automation
// (bind_filter_cutoff) — this voice is not filterable.
//
// note_off cuts hard (the one-shot envelope has no release stage).
// In practice the lead plays 8th-note lines, so the next note's
// retrigger masks the cut.

#include <array>
#include <cstddef>

#include "edm/constants.hxx"
#include "edm/oscillator.hxx"
#include "edm/voice.hxx"

namespace edm {

struct SupersawVoiceConfig {
  double attack_seconds = 0.005;
  double decay_seconds = 0.25;
  float volume = 1.15f;  // the spike's lead volume
};

class SupersawVoice : public Voice {
 public:
  // A non-positive sample rate falls back to the default.
  SupersawVoice(SupersawVoiceConfig config, double sample_rate) noexcept;

  // Retriggers: resets all 7 saw phases and the sample counter, so
  // consecutive identical notes render bit-identically.
  void note_on(int midi, float velocity) noexcept override;
  // Hard cut: the one-shot envelope has no release stage.
  void note_off(int midi) noexcept override;
  float next() noexcept override;
  void reset() noexcept override;

 private:
  SupersawVoiceConfig config_;
  double sample_rate_;
  std::array<Oscillator<Wave::Saw>, kSupersawVoices> saws_{};
  std::size_t index_ = 0;  // samples since note_on
  std::size_t total_ = 0;  // samples in the envelope (attack + 6*decay)
  float velocity_ = 0.0f;
  bool active_ = false;
};

} // namespace edm
