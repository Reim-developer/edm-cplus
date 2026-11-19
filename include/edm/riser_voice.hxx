#pragma once

// RiserVoice: the build-up riser — white noise through a lowpass
// whose cutoff sweeps exponentially from f0 to f1 over the note's
// duration, with the gain rising linearly (volume * frac) alongside.
// The caller computes `seconds` (2 bars at 128 BPM = 3.75 s) and
// places one long note at the end of a build.
//
// The cutoff follows its exponential target through a per-sample
// lerp (kRiserCutoffSmoothing) so coefficient changes are smooth.
// note_off is a no-op (one-shot): a riser always plays its full
// duration.

#include <cstddef>

#include "edm/constants.hxx"
#include "edm/filter.hxx"
#include "edm/rng.hxx"
#include "edm/voice.hxx"

namespace edm {

struct RiserVoiceConfig {
  double f0_hz = 400.0;   // cutoff at the start
  double f1_hz = 8000.0;  // cutoff at the end
  double seconds = 3.75;  // 2 bars at 128 BPM — caller-computed
  float volume = 0.5f;
  int seed = kDefaultSeed;
};

class RiserVoice : public Voice {
 public:
  // A non-positive sample rate falls back to the default.
  RiserVoice(RiserVoiceConfig config, double sample_rate) noexcept;

  // Retriggers: resets the filter, the cutoff, and the sample
  // counter. The Rng is NOT rewound here (each riser gets fresh
  // noise); reset() rewinds it. The MIDI note is ignored; velocity
  // scales it.
  void note_on(int midi, float velocity) noexcept override;
  // No-op: a riser always plays its full duration.
  void note_off(int midi) noexcept override;
  float next() noexcept override;
  void reset() noexcept override;

 private:
  RiserVoiceConfig config_;
  double sample_rate_;
  Rng rng_;
  Rng rng_initial_;
  Biquad lowpass_;
  double current_cutoff_hz_;
  std::size_t index_ = 0;  // samples since note_on
  std::size_t total_ = 0;  // samples in `seconds`
  float velocity_ = 0.0f;
  bool active_ = false;
};

} // namespace edm
