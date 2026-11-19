// Automation: linear / exponential interpolation, clamping,
// empty no-op, unsorted input, degenerate segments.

#include <cmath>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "edm/section.hxx"

namespace {
edm::Automation linear(std::vector<std::pair<double, double>> points) {
  return edm::Automation(std::move(points), edm::Curve::Linear);
}

edm::Automation expo(std::vector<std::pair<double, double>> points) {
  return edm::Automation(std::move(points), edm::Curve::Exponential);
}
} // namespace

TEST(Automation, LinearExactValues) {
  const edm::Automation a = linear({{0.0, 100.0}, {2.0, 200.0}, {4.0, 400.0}});
  EXPECT_DOUBLE_EQ(a.at(0.0), 100.0);
  EXPECT_DOUBLE_EQ(a.at(1.0), 150.0);
  EXPECT_DOUBLE_EQ(a.at(2.0), 200.0);
  EXPECT_DOUBLE_EQ(a.at(3.0), 300.0);
  EXPECT_DOUBLE_EQ(a.at(4.0), 400.0);
  EXPECT_DOUBLE_EQ(a.at(0.5), 125.0);
}

TEST(Automation, ExponentialExactValues) {
  // The build sweep: 500 -> 6000 Hz over 4 bars, exponential.
  const edm::Automation a = expo({{0.0, 500.0}, {4.0, 6000.0}});
  EXPECT_DOUBLE_EQ(a.at(0.0), 500.0);
  EXPECT_DOUBLE_EQ(a.at(4.0), 6000.0);
  // Midpoint is the geometric mean: 500 * sqrt(12).
  EXPECT_NEAR(a.at(2.0), 500.0 * std::sqrt(12.0), 1e-6);
  // Quarter point: 500 * 12^0.25.
  EXPECT_NEAR(a.at(1.0), 500.0 * std::pow(12.0, 0.25), 1e-6);
}

TEST(Automation, ClampOutsideRange) {
  const edm::Automation a = linear({{1.0, 100.0}, {3.0, 300.0}});
  EXPECT_DOUBLE_EQ(a.at(-1.0), 100.0);
  EXPECT_DOUBLE_EQ(a.at(0.0), 100.0);
  EXPECT_DOUBLE_EQ(a.at(99.0), 300.0);
}

TEST(Automation, EmptyIsNoOp) {
  const edm::Automation a = linear({});
  EXPECT_TRUE(a.empty());
  EXPECT_DOUBLE_EQ(a.at(0.0), 0.0);
  EXPECT_DOUBLE_EQ(a.at(5.0), 0.0);
  const edm::Automation e = expo({});
  EXPECT_TRUE(e.empty());
  EXPECT_DOUBLE_EQ(e.at(2.0), 0.0);
}

TEST(Automation, SinglePoint) {
  const edm::Automation a = linear({{2.0, 42.0}});
  EXPECT_DOUBLE_EQ(a.at(0.0), 42.0);
  EXPECT_DOUBLE_EQ(a.at(2.0), 42.0);
  EXPECT_DOUBLE_EQ(a.at(9.0), 42.0);
}

TEST(Automation, UnsortedInputIsSorted) {
  const edm::Automation a = linear({{4.0, 400.0}, {0.0, 100.0}, {2.0, 200.0}});
  EXPECT_DOUBLE_EQ(a.at(1.0), 150.0);
  EXPECT_DOUBLE_EQ(a.at(3.0), 300.0);
}

TEST(Automation, ExponentialNonPositiveFallsBackToLinear) {
  // A segment with a non-positive endpoint cannot be geometric —
  // it falls back to linear instead of producing NaN.
  const edm::Automation a = expo({{0.0, 0.0}, {2.0, 100.0}});
  EXPECT_DOUBLE_EQ(a.at(1.0), 50.0);
  const edm::Automation b = expo({{0.0, 100.0}, {2.0, 0.0}});
  EXPECT_DOUBLE_EQ(b.at(1.0), 50.0);
  const edm::Automation c = expo({{0.0, -5.0}, {2.0, -15.0}});
  EXPECT_DOUBLE_EQ(c.at(1.0), -10.0);
}

TEST(Automation, DuplicatePositionIsSafe) {
  const edm::Automation a =
      linear({{0.0, 100.0}, {2.0, 200.0}, {2.0, 300.0}, {4.0, 400.0}});
  EXPECT_DOUBLE_EQ(a.at(1.0), 150.0);
  EXPECT_DOUBLE_EQ(a.at(2.0), 300.0);  // the right point wins
  EXPECT_DOUBLE_EQ(a.at(3.0), 350.0);
  EXPECT_FALSE(std::isnan(a.at(2.0)));
}
