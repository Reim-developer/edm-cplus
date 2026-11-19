#pragma once

// Automation: a parameter curve over bar positions — the mechanism
// that makes a section move (a filter opening across a build, a
// track fading in). Points are (bar position, value); evaluation
// interpolates between them and clamps outside the range.
//
// Linear is a straight interpolation; Exponential is geometric
// (value = a * (b/a)^t) — the right shape for filter sweeps, since
// pitch is logarithmic. Exponential segments need positive values;
// a segment with a non-positive endpoint falls back to Linear for
// that segment, so at() is total and never NaN.
//
// An empty automation is a no-op: Section skips it entirely, and
// at() returns 0.0 (documented — callers check empty() first).

#include <utility>
#include <vector>

namespace edm {

enum class Curve { Linear, Exponential };

class Automation {
 public:
  // Points are sorted by position at construction (the caller may
  // pass them in any order).
  Automation(std::vector<std::pair<double, double>> points, Curve curve);

  // Value at a bar position: interpolated between the surrounding
  // points, clamped to the first/last value outside the range.
  // Returns 0.0 for an empty automation.
  [[nodiscard]] double at(double bar) const noexcept;

  [[nodiscard]] bool empty() const noexcept { return points_.empty(); }

 private:
  std::vector<std::pair<double, double>> points_;
  Curve curve_;
};

} // namespace edm
