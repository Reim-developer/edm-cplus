#pragma once

// Voice: the library's first polymorphic type. A voice is anything
// that can sound a note: note_on starts it, note_off releases it,
// next() renders one sample. The sequencer owns voices through
// std::unique_ptr<Voice> — this is where smart pointers enter the
// library (foreshadowed in the v0.0.1 design).
//
// midi_to_freq is the note-to-frequency conversion: A4 (MIDI 69)
// = 440 Hz, each semitone multiplies by the 12th root of 2.

#include <cmath>

namespace edm {

// MIDI note number -> frequency in Hz.
[[nodiscard]] inline double midi_to_freq(int midi) noexcept {
  return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
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

} // namespace edm
