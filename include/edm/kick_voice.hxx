#pragma once

// KickVoice: the four-on-the-floor kick — a sine with a fast
// exponential pitch drop (150 -> 45 Hz) and exponential decay.
// One-shot: note_off is a no-op, so the pattern's gate does not cut
// a kick short (use 1-step gates on kick tracks).
//
// Ported from the EDM spike (edm.cpp), which proved the sound in a
// full 60-second render. The pitch drop uses Oscillator's
// per-sample set_frequency — the oscillator recomputes its phase
// increment on every call, so the sweep is exact.

#include <cstddef>

#include "edm/oscillator.hxx"
#include "edm/voice.hxx"

namespace edm {

struct KickVoiceConfig {
  double f0_hz = 150.0;        // pitch at the hit
  double f1_hz = 45.0;         // pitch it settles to
  double slide_seconds = 0.030;  // exp time constant of the drop
  double decay_seconds = 0.16;   // exp time constant of the decay
  double seconds = 0.30;         // total duration (silence after)
  float volume = 0.95f;
};

class KickVoice : public Voice {
 public:
  // A non-positive sample rate falls back to the default.
  KickVoice(KickVoiceConfig config, double sample_rate) noexcept;

  // Retriggers: resets the phase and the sample counter. The MIDI
  // note is ignored (a kick has a fixed pitch); velocity scales it.
  void note_on(int midi, float velocity) noexcept override;
  // No-op: a kick always plays its full duration.
  void note_off(int midi) noexcept override;
  float next() noexcept override;
  void reset() noexcept override;

 private:
  KickVoiceConfig config_;
  double sample_rate_;
  Oscillator<Wave::Sine> oscillator_;
  std::size_t index_ = 0;  // samples since note_on
  std::size_t total_ = 0;  // samples in `seconds`
  float velocity_ = 0.0f;
  bool active_ = false;
};

} // namespace edm
