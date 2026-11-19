// Filter implementations. See filter.hxx.

#include "edm/filter.hxx"

#include <algorithm>
#include <cmath>

namespace edm {
namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void Biquad::set_lowpass(double cutoff_hz, double q, double sample_rate) noexcept {
  const double sr = sample_rate > 0.0 ? sample_rate : 44100.0;
  const double fc = std::clamp(cutoff_hz, 1.0, 0.45 * sr);
  const double q_clamped = std::clamp(q, 0.1, 100.0);
  const double w0 = 2.0 * kPi * fc / sr;
  const double alpha = std::sin(w0) / (2.0 * q_clamped);
  const double c = std::cos(w0);
  const double a0 = 1.0 + alpha;
  b0_ = ((1.0 - c) / 2.0) / a0;
  b1_ = (1.0 - c) / a0;
  b2_ = ((1.0 - c) / 2.0) / a0;
  a1_ = (-2.0 * c) / a0;
  a2_ = (1.0 - alpha) / a0;
}

void Biquad::reset() noexcept {
  x1_ = 0.0;
  x2_ = 0.0;
  y1_ = 0.0;
  y2_ = 0.0;
}

float Biquad::process(float x) noexcept {
  const double y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
  x2_ = x1_;
  x1_ = x;
  y2_ = y1_;
  y1_ = y;
  return static_cast<float>(y);
}

void Biquad::process(std::span<const float> in, std::span<float> out) noexcept {
  const std::size_t n = std::min(in.size(), out.size());
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = process(in[i]);
  }
}

void Highpass::set(double cutoff_hz, double sample_rate) noexcept {
  const double sr = sample_rate > 0.0 ? sample_rate : 44100.0;
  const double fc = std::clamp(cutoff_hz, 1.0, 0.45 * sr);
  a_ = std::exp(-2.0 * kPi * fc / sr);
}

Highpass::Highpass() { set(1000.0, 44100.0); }

void Highpass::reset() noexcept {
  y_ = 0.0;
  x_prev_ = 0.0;
}

float Highpass::process(float x) noexcept {
  y_ = a_ * (y_ + x - x_prev_);
  x_prev_ = x;
  return static_cast<float>(y_);
}

void Highpass::process(std::span<const float> in, std::span<float> out) noexcept {
  const std::size_t n = std::min(in.size(), out.size());
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = process(in[i]);
  }
}

} // namespace edm
