#pragma once

// Voice: the library's first polymorphic type. A voice is anything
// that can sound a note: note_on starts it, note_off releases it,
// next() renders one sample. The sequencer owns voices through
// std::unique_ptr<Voice> — this is where smart pointers enter the
// library (foreshadowed in the v0.0.1 design).
//
// midi_to_freq is the note-to-frequency conversion: A4 (MIDI 69)
// = 440 Hz, each semitone multiplies by the 12th root of 2. The
// reference pitch is a template parameter, so alternate concert
// pitch (432 Hz, 415 Hz baroque) is one template argument away;
// the default keeps every existing call working unchanged.

#include <cmath>

#include "edm/constants.hxx"

namespace edm {

// MIDI note number -> frequency in Hz, against a reference pitch
// for A4. Floating-point NTTP is C++20. Not constexpr: std::pow
// is not constexpr on the supported toolchains.
template <double ReferenceHz = kA4Frequency>
[[nodiscard]] double midi_to_freq(int midi) noexcept {
  return ReferenceHz *
         std::pow(2.0, (static_cast<double>(midi) - kA4MidiNote) / kSemitonesPerOctave);
}

class Voice {
 public:
  virtual ~Voice() = default;

  // Start a note (monophonic voices retrigger). velocity in 0..1.
  virtual void note_on(int midi, float velocity) noexcept = 0;
  // Release a note. Voices ignore note_off for notes they are not
  // currently sounding.
  virtual void note_off(int midi) noexcept = 0;
  // Render one sample. Never allocates.
  virtual float next() noexcept = 0;
  // Silence the voice and reset its state.
  virtual void reset() noexcept = 0;
};

// A voice with a settable lowpass filter. The Section's filter-cutoff
// automation binds to this interface — one dynamic_cast at bind time
// — not to a concrete voice type: SynthVoice is a template, so there
// is no single SynthVoice type to cast to, and a future filtered
// voice should bind without touching the Section.
class FilterableVoice : public Voice {
 public:
  // Move the lowpass cutoff (Hz). A cutoff <= 0 bypasses the filter.
  // If the voice was built bypassed, a positive cutoff ENABLES the
  // filter — a bound automation must do something; silently ignoring
  // it would be a trap.
  virtual void set_filter_cutoff(double hz) noexcept = 0;
};

} // namespace edm
