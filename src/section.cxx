// Automation implementation. See section.hxx.

#include "edm/section.hxx"

#include <algorithm>
#include <cmath>

namespace edm {

Automation::Automation(std::vector<std::pair<double, double>> points, Curve curve)
    : points_(std::move(points)), curve_(curve) {
  std::sort(points_.begin(), points_.end(),
            [](const std::pair<double, double>& a, const std::pair<double, double>& b) {
              return a.first < b.first;
            });
}

double Automation::at(double bar) const noexcept {
  if (points_.empty()) return 0.0;
  if (points_.size() == 1 || bar <= points_.front().first) return points_.front().second;
  if (bar >= points_.back().first) return points_.back().second;
  // Find the segment [i, i+1] containing bar. The scan uses <= so
  // that at an exact point position the segment STARTING there wins
  // (for duplicate positions, the right point wins: a step change
  // at bar 2 means "from bar 2 on, the new value"). Point counts
  // are single digits in practice; a linear scan is the clear choice.
  std::size_t i = 0;
  while (i + 1 < points_.size() && points_[i + 1].first <= bar) ++i;
  const double x0 = points_[i].first;
  const double x1 = points_[i + 1].first;
  const double a = points_[i].second;
  const double b = points_[i + 1].second;
  if (x1 <= x0) return b;  // duplicate positions: the right point wins
  const double t = (bar - x0) / (x1 - x0);
  if (curve_ == Curve::Exponential && a > 0.0 && b > 0.0) {
    return a * std::pow(b / a, t);
  }
  return a + (b - a) * t;  // Linear, or the exponential fallback
}

} // namespace edm
