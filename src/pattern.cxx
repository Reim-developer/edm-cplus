// Pattern implementation. See pattern.hxx.
//
// The methods live here (not in the header) for a compiler reason:
// with -Werror, GCC's -Warray-bounds fires a false positive on the
// guarded subscript when set/clear/at are inlined into a caller that
// passes constant out-of-range indices — exactly what the
// out-of-range-safety test does. Neither the guard, .at(), nor a
// clamped index silences it; a separate translation unit keeps the
// constants out of the method bodies.

#include "edm/pattern.hxx"

namespace edm {

Pattern::Pattern(int steps) : steps_(steps > 0 ? static_cast<std::size_t>(steps) : 0) {}

void Pattern::set(int step, int midi, float velocity, int length_steps) noexcept {
  if (!in_range(step)) return;
  Step& s = steps_[static_cast<std::size_t>(step)];
  s.note = midi;
  s.velocity = velocity;
  s.length_steps = length_steps < 1 ? 1 : length_steps;
}

void Pattern::clear(int step) noexcept {
  if (!in_range(step)) return;
  steps_[static_cast<std::size_t>(step)] = Step{};
}

const Step& Pattern::at(int step) const noexcept {
  static const Step kEmpty;
  if (!in_range(step)) return kEmpty;
  return steps_[static_cast<std::size_t>(step)];
}

int Pattern::size() const noexcept { return static_cast<int>(steps_.size()); }

bool Pattern::in_range(int step) const noexcept {
  return step >= 0 && static_cast<std::size_t>(step) < steps_.size();
}

} // namespace edm
