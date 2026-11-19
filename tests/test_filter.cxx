// Filter behavior: lowpass passes DC and attenuates above cutoff;
// highpass blocks DC; block processing matches sample-by-sample.

#include <cmath>
#include <vector>

#include "check.hxx"
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

int main() {
  test::Context ctx;

  // Lowpass passes DC at ~unity gain (RBJ DC gain is exactly 1).
  {
    edm::Biquad lp;
    lp.set_lowpass(1000.0, 0.7, kSampleRate);
    float last = 0.0f;
    for (int i = 0; i < 4096; ++i) last = lp.process(1.0f);
    ctx.check_near(last, 1.0, 1e-3, "lowpass DC settles to ~1.0");
  }

  // Lowpass attenuates well above cutoff (2nd order: ~1% at 10x cutoff).
  {
    edm::Biquad lp;
    lp.set_lowpass(1000.0, 0.7, kSampleRate);
    std::vector<float> in(4410);
    std::vector<float> out(4410);
    for (std::size_t i = 0; i < in.size(); ++i) {
      in[i] = static_cast<float>(std::sin(2.0 * kPi * 10000.0 * static_cast<double>(i) / kSampleRate));
    }
    lp.process(in, out);
    const double ratio = rms(out, 1000) / rms(in, 1000);
    ctx.check(ratio < 0.05, "lowpass attenuates 10kHz at 1kHz cutoff (ratio < 0.05)");
  }

  // Highpass blocks DC.
  {
    edm::Highpass hp;
    hp.set(1000.0, kSampleRate);
    float last = 0.0f;
    for (int i = 0; i < 4096; ++i) last = hp.process(1.0f);
    ctx.check(std::abs(last) < 0.01f, "highpass DC decays to ~0");
  }

  // Block processing matches sample-by-sample processing exactly.
  {
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
    ctx.check(same, "block process == sample-by-sample process");
  }

  // Per-sample cutoff changes (a sweep) stay finite and bounded.
  {
    edm::Biquad lp;
    bool ok = true;
    for (int i = 0; i < 2000; ++i) {
      const double frac = static_cast<double>(i) / 2000.0;
      lp.set_lowpass(100.0 * std::pow(100.0, frac), 0.9, kSampleRate);
      const float x = static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / kSampleRate));
      const float y = lp.process(x);
      if (!std::isfinite(y) || std::abs(y) > 2.0f) ok = false;
    }
    ctx.check(ok, "cutoff sweep stays finite and bounded");
  }

  // Degenerate settings cannot produce NaN.
  {
    edm::Biquad lp;
    lp.set_lowpass(0.0, 0.0, kSampleRate);
    const float y = lp.process(1.0f);
    ctx.check(std::isfinite(y), "zero cutoff/Q clamped, no NaN");
  }

  return ctx.finish("filter");
}
