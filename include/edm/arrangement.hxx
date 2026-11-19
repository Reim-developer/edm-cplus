#pragma once

// Arrangement: the song — a sequence of named Sections rendered
// in order at one BPM. Sections are moved in; the arrangement
// owns them, so there are no dangling references.
//
// The arrangement's BPM is the song's tempo; every section's
// sequencer MUST be built at the same BPM (there is no check —
// Sequencer does not store its BPM, only its step length in
// samples — so a mismatched section would play at the wrong
// tempo without complaint). The sample rate is used by
// render_to_wav.
//
// Sections render independently: each section resets its own
// sequencer, so no voice rings across a boundary in this version
// (cross-section tails belong to the future effects layer).

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "edm/section.hxx"

namespace edm {

class Arrangement {
 public:
  Arrangement(double bpm, double sample_rate);

  // Adds a section by move.
  void add_section(Section section);

  // Total frames of all sections. 0 for an empty arrangement.
  [[nodiscard]] std::size_t frames() const noexcept;
  [[nodiscard]] std::size_t section_count() const noexcept { return sections_.size(); }
  [[nodiscard]] double bpm() const noexcept { return bpm_; }
  [[nodiscard]] double sample_rate() const noexcept { return sample_rate_; }

  // Render all sections in order into the span. A span longer
  // than the arrangement has its tail silenced; a shorter span
  // renders what fits. Re-rendering reproduces bit-identically.
  void render(std::span<float> out);

  // Render the whole arrangement and write it as a 16-bit mono
  // WAV. Returns false if the file cannot be written. This is
  // the RAW render (no master chain) — the demo applies tanh +
  // normalize itself via render().
  bool render_to_wav(const std::string& path);

 private:
  double bpm_;
  double sample_rate_;
  std::vector<Section> sections_;
};

} // namespace edm
