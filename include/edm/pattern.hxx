#pragma once

// Pattern: a loop of 16th-note steps. Each step is either a rest or a
// note (MIDI number) with a velocity and a gate length in steps.
// The gate length is here from the start so v0.0.4's sustained notes
// need no retrofit.
//
// Out-of-range access is safe by construction: set/clear ignore
// out-of-range steps, at() returns an empty step. A pattern never
// crashes the sequencer, whatever the caller hands it.
//
// No allocation after construction: the step vector is sized once.

#include <cstddef>
#include <optional>
#include <vector>

#include "edm/constants.hxx"

namespace edm {

struct Step {
  std::optional<int> note;  // MIDI number; nullopt = rest
  float velocity = kDefaultVelocity;  // 0..1
  int length_steps = 1;               // gate length in steps (>= 1)
};

class Pattern {
 public:
  // Step count; non-positive counts make an empty pattern.
  explicit Pattern(int steps);

  // Set a note on a step. Out-of-range steps are ignored.
  // length_steps is clamped to >= 1 (a gate of 0 steps is no note).
  void set(int step, int midi, float velocity = kDefaultVelocity, int length_steps = 1) noexcept;

  // Clear a step back to a rest. Out-of-range steps are ignored.
  void clear(int step) noexcept;

  // Read a step. Out-of-range steps return an empty step (a rest).
  [[nodiscard]] const Step& at(int step) const noexcept;

  [[nodiscard]] int size() const noexcept;

 private:
  [[nodiscard]] bool in_range(int step) const noexcept;

  std::vector<Step> steps_;
};

} // namespace edm
