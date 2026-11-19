// SFX: preset frame counts and audibility, render determinism,
// frequency-slide direction (via zero-crossing rate), and
// randomize_sfx determinism + ranges.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"

namespace {
constexpr int kSampleRate = 44100;
constexpr std::uint32_t kSeed = 1234; // the spike's seed

edm::Buffer render(const edm::SfxParams& params, std::uint32_t seed) {
  edm::Buffer buffer(edm::sfx_frames(params, kSampleRate));
  edm::Rng rng(seed);
  edm::render_sfx(params, rng, buffer.data(), kSampleRate);
  return buffer;
}

float peak(const edm::Buffer& buffer) {
  float max = 0.0f;
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    const float a = std::abs(buffer[i]);
    if (a > max) max = a;
  }
  return max;
}

// Zero-crossing rate over [from, to): sign changes per sample.
// Proportional to frequency for square/saw, so it proves the slide
// direction without an FFT.
double zcr(const edm::Buffer& buffer, std::size_t from, std::size_t to) {
  std::size_t crossings = 0;
  for (std::size_t i = from + 1; i < to; ++i) {
    if ((buffer[i - 1] >= 0.0f) != (buffer[i] >= 0.0f)) ++crossings;
  }
  return static_cast<double>(crossings) / static_cast<double>(to - from - 1);
}

bool same_params(const edm::SfxParams& a, const edm::SfxParams& b) {
  return a.wave == b.wave && a.f0_hz == b.f0_hz && a.f1_hz == b.f1_hz &&
         a.seconds == b.seconds && a.volume == b.volume &&
         a.attack_seconds == b.attack_seconds && a.decay_seconds == b.decay_seconds;
}
} // namespace

TEST(Sfx, PresetFrameCounts) {
  EXPECT_EQ(edm::sfx_frames(edm::sfx::jump(), kSampleRate), 11025);
  EXPECT_EQ(edm::sfx_frames(edm::sfx::coin(), kSampleRate), 13230);
  EXPECT_EQ(edm::sfx_frames(edm::sfx::laser(), kSampleRate), 13230);
  EXPECT_EQ(edm::sfx_frames(edm::sfx::explosion(), kSampleRate), 26460);
}

TEST(Sfx, PresetsRenderNonSilent) {
  EXPECT_GT(peak(render(edm::sfx::jump(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(edm::sfx::coin(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(edm::sfx::laser(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(edm::sfx::explosion(), kSeed)), 0.1f);
}

// Same params + same seed -> identical samples (bit-exact), including
// the noise-based explosion.
TEST(Sfx, Determinism) {
  const edm::SfxParams presets[] = {edm::sfx::jump(), edm::sfx::coin(), edm::sfx::laser(),
                                    edm::sfx::explosion()};
  for (const edm::SfxParams& params : presets) {
    const edm::Buffer a = render(params, kSeed);
    const edm::Buffer b = render(params, kSeed);
    bool same = a.size() == b.size();
    for (std::size_t i = 0; same && i < a.size(); ++i) {
      if (a[i] != b[i]) same = false;
    }
    EXPECT_TRUE(same);
  }
}

// jump slides 300 -> 900 Hz: the second half crosses zero ~1.67x as
// often as the first half (average frequencies 750 vs 450 Hz).
TEST(Sfx, JumpSlideRises) {
  const edm::Buffer buffer = render(edm::sfx::jump(), kSeed);
  const std::size_t mid = buffer.size() / 2;
  const double first = zcr(buffer, 0, mid);
  const double second = zcr(buffer, mid, buffer.size());
  EXPECT_GT(second, first * 1.3);
}

// laser slides 1200 -> 100 Hz: the first half crosses zero ~2.47x as
// often as the second half (average frequencies 925 vs 375 Hz).
TEST(Sfx, LaserSlideFalls) {
  const edm::Buffer buffer = render(edm::sfx::laser(), kSeed);
  const std::size_t mid = buffer.size() / 2;
  const double first = zcr(buffer, 0, mid);
  const double second = zcr(buffer, mid, buffer.size());
  EXPECT_GT(first, second * 1.3);
}

TEST(Sfx, RandomizeDeterminism) {
  edm::Rng rng_a(7);
  edm::Rng rng_b(7);
  EXPECT_TRUE(same_params(edm::randomize_sfx(rng_a), edm::randomize_sfx(rng_b)));
}

TEST(Sfx, RandomizeInRange) {
  edm::Rng rng(7);
  for (int i = 0; i < 100; ++i) {
    const edm::SfxParams p = edm::randomize_sfx(rng);
    EXPECT_TRUE(p.wave == edm::Wave::Sine || p.wave == edm::Wave::Square ||
                p.wave == edm::Wave::Saw || p.wave == edm::Wave::Noise);
    EXPECT_GE(p.f0_hz, 100.0);
    EXPECT_LT(p.f0_hz, 1200.0);
    EXPECT_GE(p.f1_hz, 50.0);
    EXPECT_LT(p.f1_hz, 1500.0);
    EXPECT_GE(p.seconds, 0.1);
    EXPECT_LT(p.seconds, 0.8);
    EXPECT_GE(p.volume, 0.3);
    EXPECT_LT(p.volume, 0.8);
    EXPECT_GE(p.attack_seconds, 0.001);
    EXPECT_LT(p.attack_seconds, 0.05);
    EXPECT_GE(p.decay_seconds, 0.05);
    EXPECT_LT(p.decay_seconds, 0.5);
  }
}
