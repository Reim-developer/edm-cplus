#pragma once

// SynthVoice: one Oscillator + ADSR Envelope + optional Biquad
// lowpass, behind the Voice interface. Monophonic: a new note
// retriggers (one voice = one note at a time; chords come from
// multiple tracks).
//
// The wave is a template parameter — compile-time selection, like
// Oscillator itself — and SynthVoiceConfig carries it at runtime
// for the make_synth_voice factory, which switches once at
// construction. When constructing SynthVoice<W> directly, W wins
// and config.wave is ignored.
//
// Noise voices consume an explicit seeded Rng passed to the
// constructor (by value — Rng is one word); non-noise waves ignore
// it. Same seed -> same noise stream, always: determinism must be
// visible, never hidden state.

#include <memory>
#include <optional>

#include "edm/envelope.hxx"
#include "edm/filter.hxx"
#include "edm/oscillator.hxx"
#include "edm/rng.hxx"
#include "edm/voice.hxx"

namespace edm {

struct SynthVoiceConfig {
  Wave wave = Wave::Saw;
  double attack_seconds = 0.01;
  double decay_seconds = 0.1;
  double sustain_level = 0.7;
  double release_seconds = 0.1;
  double filter_cutoff_hz = 0.0;  // 0 = bypass (no filter)
  float volume = 0.5f;
};

template <Wave W>
class SynthVoice : public Voice {
 public:
  SynthVoice(SynthVoiceConfig config, double sample_rate, Rng rng = Rng(1234)) noexcept
      : config_(config), sample_rate_(sample_rate), rng_(rng) {
    envelope_.set_adsr(config.attack_seconds, config.decay_seconds, config.sustain_level,
                       config.release_seconds, sample_rate);
    if (config.filter_cutoff_hz > 0.0) {
      filter_.set_lowpass(config.filter_cutoff_hz, 0.7, sample_rate);
    }
  }

  void note_on(int midi, float velocity) noexcept override {
    oscillator_.set_frequency(midi_to_freq(midi), sample_rate_);
    oscillator_.reset();  // retrigger restarts from phase 0
    envelope_.note_on();
    velocity_ = velocity;
    active_midi_ = midi;
  }

  void note_off(int midi) noexcept override {
    // Only release the note this voice is sounding; a stale
    // note_off must not kill a retriggered note.
    if (active_midi_.has_value() && active_midi_.value() == midi) {
      envelope_.note_off();
      active_midi_.reset();
    }
  }

  float next() noexcept override {
    float s = oscillator_.next(rng_) * envelope_.next() * velocity_ * config_.volume;
    if (config_.filter_cutoff_hz > 0.0) s = filter_.process(s);
    return s;
  }

  void reset() noexcept override {
    oscillator_.reset();
    envelope_.reset();
    filter_.reset();
    velocity_ = 0.0f;
    active_midi_.reset();
  }

 private:
  SynthVoiceConfig config_;
  double sample_rate_;
  Rng rng_;
  Oscillator<W> oscillator_;
  Envelope envelope_;
  Biquad filter_;
  float velocity_ = 0.0f;
  std::optional<int> active_midi_;
};

// Factory: runtime wave -> the right SynthVoice instantiation.
// The switch happens once, at construction — never per sample.
[[nodiscard]] std::unique_ptr<Voice> make_synth_voice(const SynthVoiceConfig& config,
                                                      double sample_rate,
                                                      Rng rng = Rng(1234));

} // namespace edm
