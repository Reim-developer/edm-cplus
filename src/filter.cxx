// Filter implementations. See filter.hxx.

#include "edm/filter.hxx"

#include <algorithm>
#include <cmath>

#include "edm/constants.hxx"

namespace edm {
namespace {
// Filter clamp limits (documented in filter.hxx): the cutoff is
// capped just below Nyquist, Q is kept in a sane band, and a
// default-constructed Highpass starts at 1 kHz.
constexpr double kMinCutoffHz = 1.0;
constexpr double kMaxCutoffRatio = 0.45;  // fraction of the sample rate
constexpr double kMinQ = 0.1;
constexpr double kMaxQ = 100.0;
constexpr double kDefaultHighpassCutoffHz = 1000.0;
} // namespace

void Biquad::set_lowpass(double cutoff_hz, double q, double sample_rate) noexcept {
  const double sr = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
  const double fc = std::clamp(cutoff_hz, kMinCutoffHz, kMaxCutoffRatio * sr);
  const double q_clamped = std::clamp(q, kMinQ, kMaxQ);
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
  const double sr = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
  const double fc = std::clamp(cutoff_hz, kMinCutoffHz, kMaxCutoffRatio * sr);
  a_ = std::exp(-2.0 * kPi * fc / sr);
}

Highpass::Highpass() { set(kDefaultHighpassCutoffHz, kDefaultSampleRate); }

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
