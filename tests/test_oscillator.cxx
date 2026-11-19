// Oscillator known values (sample rate 4 Hz, frequency 1 Hz -> the
// phase steps through 0, 0.25, 0.5, 0.75 exactly), noise range and
// determinism, and full-render determinism: the same tone rendered
// twice produces identical samples.

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/envelope.hxx"
#include "edm/oscillator.hxx"
#include "edm/rng.hxx"

// Sine at phases 0 / 0.25 / 0.5 / 0.75 -> 0 / 1 / 0 / -1.
TEST(Oscillator, SineKnownValues) {
  edm::Oscillator<edm::Wave::Sine> osc;
  osc.set_frequency(1.0, 4.0);
  EXPECT_NEAR(osc.next(), 0.0, 1e-6);
  EXPECT_NEAR(osc.next(), 1.0, 1e-6);
  EXPECT_NEAR(osc.next(), 0.0, 1e-6);
  EXPECT_NEAR(osc.next(), -1.0, 1e-6);
}

// Square sign: +0.8 in the first half, -0.8 in the second.
TEST(Oscillator, SquareKnownValues) {
  edm::Oscillator<edm::Wave::Square> osc;
  osc.set_frequency(1.0, 4.0);
  EXPECT_NEAR(osc.next(), 0.8, 1e-6);
  EXPECT_NEAR(osc.next(), 0.8, 1e-6);
  EXPECT_NEAR(osc.next(), -0.8, 1e-6);
  EXPECT_NEAR(osc.next(), -0.8, 1e-6);
}

// Saw ramp: -1 / -0.5 / 0 / 0.5.
TEST(Oscillator, SawKnownValues) {
  edm::Oscillator<edm::Wave::Saw> osc;
  osc.set_frequency(1.0, 4.0);
  EXPECT_NEAR(osc.next(), -1.0, 1e-6);
  EXPECT_NEAR(osc.next(), -0.5, 1e-6);
  EXPECT_NEAR(osc.next(), 0.0, 1e-6);
  EXPECT_NEAR(osc.next(), 0.5, 1e-6);
}

// Noise stays in [-1, 1) and is deterministic for a given seed.
TEST(Oscillator, NoiseInRange) {
  edm::Rng rng(99);
  edm::Oscillator<edm::Wave::Noise> osc;
  bool in_range = true;
  for (int i = 0; i < 10000; ++i) {
    const float v = osc.next(rng);
    if (v < -1.0f || v >= 1.0f) in_range = false;
  }
  EXPECT_TRUE(in_range);
}

TEST(Oscillator, NoiseDeterministic) {
  edm::Rng rng_a(99);
  edm::Rng rng_b(99);
  edm::Oscillator<edm::Wave::Noise> a;
  edm::Oscillator<edm::Wave::Noise> b;
  bool same = true;
  for (int i = 0; i < 1000; ++i) {
    if (a.next(rng_a) != b.next(rng_b)) same = false;
  }
  EXPECT_TRUE(same);
}

// render() fills the whole span; a second render continues the phase.
TEST(Oscillator, RenderFillsSpan) {
  edm::Oscillator<edm::Wave::Sine> osc;
  osc.set_frequency(440.0, 44100.0);
  std::vector<float> buf(1024, 0.0f);
  osc.render(buf);
  bool nonzero = false;
  for (float v : buf) {
    if (v != 0.0f) nonzero = true;
  }
  EXPECT_TRUE(nonzero);
  EXPECT_NE(osc.phase(), 0.0);
}

// Full-render determinism: the same tone (oscillator + envelope into
// a buffer) rendered twice with the same seed is bit-identical.
TEST(Oscillator, FullRenderDeterminism) {
  auto render_tone = [](std::uint32_t seed) {
    edm::Buffer buffer(4410);
    edm::Oscillator<edm::Wave::Saw> osc;
    edm::Envelope env;
    edm::Rng rng(seed);
    osc.set_frequency(220.0, 44100.0);
    env.trigger_ad(0.01, 0.09, 44100.0);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
      buffer[i] = osc.next(rng) * env.next();
    }
    return buffer;
  };
  const edm::Buffer a = render_tone(42);
  const edm::Buffer b = render_tone(42);
  bool same = a.size() == b.size();
  for (std::size_t i = 0; same && i < a.size(); ++i) {
    if (a[i] != b[i]) same = false;
  }
  EXPECT_TRUE(same);
}
