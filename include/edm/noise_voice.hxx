#pragma once

// NoiseVoice: config-driven noise percussion — hats, claps, snares.
// One voice, one config: the kind of sound is DATA (the config),
// not a type — the same principle as v0.0.4's SFX refactor (the
// library provides the mechanism, the caller names the sound).
//
// The voice is white noise through a one-pole highpass, under an
// exponential decay, gated to `seconds`. With bursts > 1 the first
// 40 ms alternate between full and quarter volume at 12 ms
// intervals (the clap texture from the spike: 3 loud pulses).
//
// The Rng is constructed here, seeded from the config — never
// global. It advances across notes (each hat gets fresh noise),
// and reset() rewinds it, so a re-render reproduces bit-identically.
// note_off is a no-op (one-shot); the pattern's gate does not cut
// a hat short.

#include <cstddef>

#include "edm/constants.hxx"
#include "edm/filter.hxx"
#include "edm/rng.hxx"
#include "edm/voice.hxx"

namespace edm {

struct NoiseVoiceConfig {
  double highpass_cutoff_hz = 7000.0;  // hats: 7000; clap: 1500; snare: 1800
  double decay_seconds = 0.05;         // hats: 0.05; clap: 0.09; snare: 0.12
  double seconds = 0.10;               // hats: 0.10; clap: 0.20; snare: 0.15
  int bursts = 1;                      // clap: 3; hats/snare: 1
  float volume = 0.5f;                 // hats: 0.5; clap: 0.6; snare: 1.0
  int seed = kDefaultSeed;
};

class NoiseVoice : public Voice {
 public:
  // A non-positive sample rate falls back to the default.
  NoiseVoice(NoiseVoiceConfig config, double sample_rate) noexcept;

  // Retriggers: resets the filter and the sample counter. The Rng
  // is NOT rewound here (each note gets fresh noise); reset()
  // rewinds it. The MIDI note is ignored; velocity scales it.
  void note_on(int midi, float velocity) noexcept override;
  // No-op: a noise hit always plays its full duration.
  void note_off(int midi) noexcept override;
  float next() noexcept override;
  void reset() noexcept override;

 private:
  NoiseVoiceConfig config_;
  double sample_rate_;
  Rng rng_;          // advances across notes
  Rng rng_initial_;  // the construction Rng — reset() restores it
  Highpass highpass_;
  std::size_t index_ = 0;  // samples since note_on
  std::size_t total_ = 0;  // samples in `seconds`
  float velocity_ = 0.0f;
  bool active_ = false;
};

} // namespace edm
