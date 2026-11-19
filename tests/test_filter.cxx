// Filter behavior: lowpass passes DC and attenuates above cutoff;
// highpass blocks DC; block processing matches sample-by-sample.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "edm/filter.hxx"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kSampleRate = 44100;

double rms(const std::vector<float>& v, std::size_t from) {
  double sum = 0.0;
  for (std::size_t i = from; i < v.size(); ++i) {
    sum += static_cast<double>(v[i]) * v[i];
  }
  return std::sqrt(sum / static_cast<double>(v.size() - from));
}
} // namespace

// Lowpass passes DC at ~unity gain (RBJ DC gain is exactly 1).
TEST(Filter, LowpassPassesDc) {
  edm::Biquad lp;
  lp.set_lowpass(1000.0, 0.7, kSampleRate);
  float last = 0.0f;
  for (int i = 0; i < 4096; ++i) last = lp.process(1.0f);
  EXPECT_NEAR(last, 1.0, 1e-3);
}

// Lowpass attenuates well above cutoff (2nd order: ~1% at 10x cutoff).
TEST(Filter, LowpassAttenuatesAboveCutoff) {
  edm::Biquad lp;
  lp.set_lowpass(1000.0, 0.7, kSampleRate);
  std::vector<float> in(4410);
  std::vector<float> out(4410);
  for (std::size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(std::sin(2.0 * kPi * 10000.0 * static_cast<double>(i) / kSampleRate));
  }
  lp.process(in, out);
  const double ratio = rms(out, 1000) / rms(in, 1000);
  EXPECT_LT(ratio, 0.05);
}

// Highpass blocks DC.
TEST(Filter, HighpassBlocksDc) {
  edm::Highpass hp;
  hp.set(1000.0, kSampleRate);
  float last = 0.0f;
  for (int i = 0; i < 4096; ++i) last = hp.process(1.0f);
  EXPECT_LT(std::abs(last), 0.01f);
}

// Block processing matches sample-by-sample processing exactly.
TEST(Filter, BlockMatchesSampleBySample) {
  edm::Biquad a;
  edm::Biquad b;
  a.set_lowpass(2000.0, 1.0, kSampleRate);
  b.set_lowpass(2000.0, 1.0, kSampleRate);
  std::vector<float> in(512);
  std::vector<float> out_block(512);
  std::vector<float> out_sample(512);
  for (std::size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / kSampleRate));
  }
  a.process(in, out_block);
  for (std::size_t i = 0; i < in.size(); ++i) out_sample[i] = b.process(in[i]);
  bool same = true;
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (out_block[i] != out_sample[i]) same = false;
  }
  EXPECT_TRUE(same);
}

// Per-sample cutoff changes (a sweep) stay finite and bounded.
TEST(Filter, CutoffSweepStaysFiniteAndBounded) {
  edm::Biquad lp;
  bool ok = true;
  for (int i = 0; i < 2000; ++i) {
    const double frac = static_cast<double>(i) / 2000.0;
    lp.set_lowpass(100.0 * std::pow(100.0, frac), 0.9, kSampleRate);
    const float x = static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / kSampleRate));
    const float y = lp.process(x);
    if (!std::isfinite(y) || std::abs(y) > 2.0f) ok = false;
  }
  EXPECT_TRUE(ok);
}

// Degenerate settings cannot produce NaN.
TEST(Filter, DegenerateSettingsNoNan) {
  edm::Biquad lp;
  lp.set_lowpass(0.0, 0.0, kSampleRate);
  const float y = lp.process(1.0f);
  EXPECT_TRUE(std::isfinite(y));
}
