// SFX: the plugin API — plugins::Slide<W> through the generic
// render path (frame counts, audibility, determinism, slide
// direction), a CRTP plugin inheriting Plugin<Derived>, user-defined
// generators written in this file (not from the library —
// inheritance stays optional), and randomize_sfx determinism +
// ranges.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/plugins.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"

namespace {
constexpr int kSampleRate = 44100;
constexpr std::uint32_t kSeed = 1234; // the spike's seed

// The spike's proven preset values, now LOCAL to this test file —
// the library no longer names sounds (0.0.4 removed edm::sfx::*).
edm::SfxParams jump_params() {
  return edm::SfxParams{.wave = edm::Wave::Square,
                        .f0_hz = 300.0,
                        .f1_hz = 900.0,
                        .seconds = 0.25,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.08};
}

edm::SfxParams coin_params() {
  return edm::SfxParams{.wave = edm::Wave::Sine,
                        .f0_hz = 988.0,
                        .f1_hz = 1319.0,
                        .seconds = 0.30,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.10};
}

edm::SfxParams laser_params() {
  return edm::SfxParams{.wave = edm::Wave::Saw,
                        .f0_hz = 1200.0,
                        .f1_hz = 100.0,
                        .seconds = 0.30,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.12};
}

edm::SfxParams explosion_params() {
  return edm::SfxParams{.wave = edm::Wave::Noise,
                        .f0_hz = 800.0,
                        .f1_hz = 60.0,
                        .seconds = 0.60,
                        .volume = 0.7,
                        .attack_seconds = 0.005,
                        .decay_seconds = 0.45};
}

// Render through the generic path: a plugins::Slide<W> pulled by
// render_sfx. The switch is once per render, never per sample.
template <edm::Wave W>
void render_into(const edm::SfxParams& params, std::uint32_t seed, edm::Buffer& buffer) {
  edm::plugins::Slide<W> gen(params, edm::Rng(seed), kSampleRate);
  edm::render_sfx(gen, buffer.data());
}

edm::Buffer render(const edm::SfxParams& params, std::uint32_t seed) {
  edm::Buffer buffer(edm::sfx_frames(params, kSampleRate));
  switch (params.wave) {
    case edm::Wave::Sine:
      render_into<edm::Wave::Sine>(params, seed, buffer);
      break;
    case edm::Wave::Square:
      render_into<edm::Wave::Square>(params, seed, buffer);
      break;
    case edm::Wave::Saw:
      render_into<edm::Wave::Saw>(params, seed, buffer);
      break;
    case edm::Wave::Noise:
      render_into<edm::Wave::Noise>(params, seed, buffer);
      break;
  }
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

// A CRTP plugin, defined HERE in user code: inherits
// Plugin<ConstantPlugin> and implements next_impl() — the base
// owns the frame counter, so silence after the duration and
// finished() come for free.
struct ConstantPlugin : public edm::plugins::Plugin<ConstantPlugin> {
  float value;
  ConstantPlugin(float v, double sample_rate, double seconds)
      : edm::plugins::Plugin<ConstantPlugin>(sample_rate, seconds), value(v) {}
  float next_impl() noexcept { return value; }
};

// USER-DEFINED generators: written in this test file, not from the
// library. They prove render_sfx renders arbitrary user code
// without knowing what it is — and that inheriting the CRTP base
// is optional, not required.
struct ConstantGen {
  float value;
  float next() noexcept { return value; }
};

struct SineGen {
  double phase = 0.0;
  double increment;
  SineGen(double hz, double sample_rate) : increment(hz / sample_rate) {}
  float next() noexcept {
    const float v = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * phase));
    phase += increment;
    if (phase >= 1.0) phase -= 1.0;
    return v;
  }
};

static_assert(edm::SampleGenerator<edm::plugins::Slide<edm::Wave::Saw>>);
static_assert(edm::SampleGenerator<ConstantPlugin>);
static_assert(edm::SampleGenerator<ConstantGen>);
static_assert(edm::SampleGenerator<SineGen>);
} // namespace

TEST(Sfx, PresetFrameCounts) {
  EXPECT_EQ(edm::sfx_frames(jump_params(), kSampleRate), 11025);
  EXPECT_EQ(edm::sfx_frames(coin_params(), kSampleRate), 13230);
  EXPECT_EQ(edm::sfx_frames(laser_params(), kSampleRate), 13230);
  EXPECT_EQ(edm::sfx_frames(explosion_params(), kSampleRate), 26460);
}

TEST(Sfx, PresetsRenderNonSilent) {
  EXPECT_GT(peak(render(jump_params(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(coin_params(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(laser_params(), kSeed)), 0.1f);
  EXPECT_GT(peak(render(explosion_params(), kSeed)), 0.1f);
}

// Same params + same seed -> identical samples (bit-exact), including
// the noise-based explosion.
TEST(Sfx, Determinism) {
  const edm::SfxParams presets[] = {jump_params(), coin_params(), laser_params(),
                                    explosion_params()};
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
  const edm::Buffer buffer = render(jump_params(), kSeed);
  const std::size_t mid = buffer.size() / 2;
  const double first = zcr(buffer, 0, mid);
  const double second = zcr(buffer, mid, buffer.size());
  EXPECT_GT(second, first * 1.3);
}

// laser slides 1200 -> 100 Hz: the first half crosses zero ~2.47x as
// often as the second half (average frequencies 925 vs 375 Hz).
TEST(Sfx, LaserSlideFalls) {
  const edm::Buffer buffer = render(laser_params(), kSeed);
  const std::size_t mid = buffer.size() / 2;
  const double first = zcr(buffer, 0, mid);
  const double second = zcr(buffer, mid, buffer.size());
  EXPECT_GT(first, second * 1.3);
}

// The params-based overload is the generic path in disguise:
// bit-identical output to rendering a Slide<W> directly.
TEST(Sfx, ConvenienceOverloadMatchesGenericPath) {
  const edm::SfxParams params = explosion_params();
  const edm::Buffer via_generic = render(params, kSeed);
  edm::Buffer via_overload(edm::sfx_frames(params, kSampleRate));
  edm::render_sfx(params, edm::Rng(kSeed), via_overload.data(), kSampleRate);
  ASSERT_EQ(via_generic.size(), via_overload.size());
  for (std::size_t i = 0; i < via_generic.size(); ++i) {
    EXPECT_EQ(via_generic[i], via_overload[i]);
  }
}

// When constructing Slide<W> directly, W wins and params.wave is
// ignored: the same params with different waves render bit-identical
// output through Slide<Wave::Sine>.
TEST(Sfx, SlideTemplateWaveWins) {
  const edm::SfxParams saw_params = laser_params(); // Wave::Saw
  edm::SfxParams sine_params = laser_params();
  sine_params.wave = edm::Wave::Sine;
  edm::Buffer via_saw(edm::sfx_frames(saw_params, kSampleRate));
  edm::plugins::Slide<edm::Wave::Sine> gen_a(saw_params, edm::Rng(kSeed), kSampleRate);
  edm::render_sfx(gen_a, via_saw.data());
  edm::Buffer via_sine(edm::sfx_frames(sine_params, kSampleRate));
  edm::plugins::Slide<edm::Wave::Sine> gen_b(sine_params, edm::Rng(kSeed), kSampleRate);
  edm::render_sfx(gen_b, via_sine.data());
  ASSERT_EQ(via_saw.size(), via_sine.size());
  for (std::size_t i = 0; i < via_saw.size(); ++i) {
    EXPECT_EQ(via_saw[i], via_sine[i]);
  }
}

// A CRTP plugin: the base owns the frame counter, so the plugin
// only implements next_impl() — every sample is the constant for
// exactly the duration, then silence, and finished() flips.
TEST(Sfx, CrtpPlugin) {
  edm::Buffer buffer(4410); // 0.1s at 44100 Hz
  ConstantPlugin gen(0.25f, kSampleRate, 0.1);
  EXPECT_FALSE(gen.finished());
  EXPECT_EQ(gen.frames_elapsed(), 0);
  EXPECT_EQ(gen.frames_total(), 4410);
  edm::render_sfx(gen, buffer.data());
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_EQ(buffer[i], 0.25f);
  }
  EXPECT_TRUE(gen.finished());
  EXPECT_EQ(gen.frames_elapsed(), 4410);
  // Past the duration, next() is silence without calling next_impl().
  EXPECT_EQ(gen.next(), 0.0f);
}

// A generator written here, in user code: every sample is exactly
// the constant — the library renders it without knowing what it is.
TEST(Sfx, UserDefinedConstantGenerator) {
  edm::Buffer buffer(100);
  ConstantGen gen{0.25f};
  edm::render_sfx(gen, buffer.data());
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_EQ(buffer[i], 0.25f);
  }
}

// A stateful user-defined generator: a 440 Hz sine, loud and
// deterministic (two generators with the same setup render
// identical samples).
TEST(Sfx, UserDefinedSineGenerator) {
  edm::Buffer buffer(4410);
  SineGen gen(440.0, kSampleRate);
  edm::render_sfx(gen, buffer.data());
  EXPECT_GT(peak(buffer), 0.9f);
  edm::Buffer again(4410);
  SineGen gen2(440.0, kSampleRate);
  edm::render_sfx(gen2, again.data());
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_EQ(buffer[i], again[i]);
  }
}

// Slide<W> returns silence once the effect's duration has elapsed
// (a span longer than the effect is not stretched).
TEST(Sfx, GeneratorSilenceAfterDuration) {
  const std::size_t frames = edm::sfx_frames(jump_params(), kSampleRate);
  edm::Buffer buffer(frames + 100);
  edm::plugins::Slide<edm::Wave::Square> gen(jump_params(), edm::Rng(kSeed), kSampleRate);
  edm::render_sfx(gen, buffer.data());
  for (std::size_t i = frames; i < buffer.size(); ++i) {
    EXPECT_EQ(buffer[i], 0.0f);
  }
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
