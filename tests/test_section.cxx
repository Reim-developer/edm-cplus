// Section: exact frames, pattern placement, long patterns playing
// once, determinism, volume + cutoff automation bindings.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/pattern.hxx"
#include "edm/section.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"
#include "edm/voice.hxx"

namespace {
constexpr int kSampleRate = 44100;
constexpr double kBpm = 128.0;
// At 128 BPM / 44.1 kHz a 16th-note step is 5168 frames.
constexpr std::size_t kStepFrames = 5168;
constexpr std::size_t kBarFrames = 16 * kStepFrames;  // 82688

edm::SynthVoiceConfig plain_config(edm::Wave wave) {
  edm::SynthVoiceConfig config;
  config.wave = wave;
  config.attack_seconds = 0.001;
  config.decay_seconds = 0.01;
  config.sustain_level = 1.0;
  config.release_seconds = 0.02;
  config.filter_cutoff_hz = 0.0;  // bypass
  config.volume = 0.5f;
  return config;
}

void add_note_track(edm::Sequencer& seq, edm::Pattern pattern,
                    edm::Wave wave = edm::Wave::Sine) {
  seq.add_track(edm::make_synth_voice(plain_config(wave), kSampleRate),
                std::move(pattern));
}

// A Voice that is NOT filterable — for the bind-failure test.
struct DummyVoice : public edm::Voice {
  void note_on(int, float) noexcept override {}
  void note_off(int) noexcept override {}
  float next() noexcept override { return 0.0f; }
  void reset() noexcept override {}
};

float window_peak(const edm::Buffer& b, std::size_t from, std::size_t to) {
  float max = 0.0f;
  for (std::size_t i = from; i < to; ++i) {
    const float a = std::abs(b[i]);
    if (a > max) max = a;
  }
  return max;
}

float window_rms(const edm::Buffer& b, std::size_t from, std::size_t to) {
  double sum = 0.0;
  for (std::size_t i = from; i < to; ++i) {
    sum += static_cast<double>(b[i]) * b[i];
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
}

bool window_silent(const edm::Buffer& b, std::size_t from, std::size_t to) {
  for (std::size_t i = from; i < to; ++i) {
    if (b[i] != 0.0f) return false;
  }
  return true;
}
} // namespace

TEST(Section, NameBarsFrames) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Section section("drop", 8, std::move(seq));
  EXPECT_EQ(section.name(), "drop");
  EXPECT_EQ(section.bars(), 8);
  EXPECT_EQ(section.frames(), 8 * kBarFrames);
}

TEST(Section, ExactFrameCountAndTail) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  add_note_track(seq, std::move(pattern));
  edm::Section section("intro", 1, std::move(seq));

  // An over-long span renders the section, then silence.
  edm::Buffer buffer(section.frames() + 1000);
  for (std::size_t i = 0; i < buffer.size(); ++i) buffer[i] = 1.0f;  // poison
  section.render(buffer.data());
  EXPECT_GT(window_peak(buffer, 0, kBarFrames), 0.1f);
  EXPECT_TRUE(window_silent(buffer, kBarFrames, buffer.size()));

  // An under-long span renders what fits, no crash.
  edm::Buffer short_buffer(kBarFrames / 2);
  section.render(short_buffer.data());
  EXPECT_GT(window_peak(short_buffer, 0, short_buffer.size()), 0.1f);

  // Zero bars: empty section, renders silence.
  edm::Sequencer empty_seq(kBpm, kSampleRate);
  edm::Section empty("empty", 0, std::move(empty_seq));
  EXPECT_EQ(empty.frames(), 0u);
  edm::Buffer silent(100);
  for (std::size_t i = 0; i < silent.size(); ++i) silent[i] = 1.0f;
  empty.render(silent.data());
  EXPECT_TRUE(window_silent(silent, 0, silent.size()));
}

// A note on step 4 of a 1-bar section sounds from step 4 — and the
// steps before it are exactly silent.
TEST(Section, PlaysPatternsOnTheRightSteps) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(4, 69);
  add_note_track(seq, std::move(pattern), edm::Wave::Square);
  edm::Section section("one", 1, std::move(seq));

  edm::Buffer buffer(section.frames());
  section.render(buffer.data());
  for (std::size_t s = 0; s < 4; ++s) {
    EXPECT_TRUE(window_silent(buffer, s * kStepFrames, (s + 1) * kStepFrames))
        << "step " << s;
  }
  EXPECT_GT(window_peak(buffer, 4 * kStepFrames, 5 * kStepFrames), 0.1f);
}

// The pattern-length rule: a 64-step pattern over a 4-bar section
// plays exactly once — notes only in the last 16 steps sound only
// in the last bar (the snare-roll shape).
TEST(Section, LongPatternPlaysOnce) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern roll(64);
  for (int step = 48; step < 64; ++step) roll.set(step, 60);
  add_note_track(seq, std::move(roll));
  edm::Section section("build", 4, std::move(seq));

  edm::Buffer buffer(section.frames());
  section.render(buffer.data());
  for (std::size_t bar = 0; bar < 3; ++bar) {
    EXPECT_TRUE(window_silent(buffer, bar * kBarFrames, (bar + 1) * kBarFrames))
        << "bar " << bar;
  }
  EXPECT_GT(window_peak(buffer, 3 * kBarFrames, 4 * kBarFrames), 0.1f);
}

// Rendering a section twice reproduces it bit-identically (the
// render resets the sequencer first).
TEST(Section, RerenderIsDeterministic) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  pattern.set(8, 76);
  add_note_track(seq, std::move(pattern));
  edm::Section section("loop", 2, std::move(seq));

  edm::Buffer first(section.frames());
  section.render(first.data());
  edm::Buffer second(section.frames());
  section.render(second.data());
  EXPECT_GT(window_peak(first, 0, first.size()), 0.1f);
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}

// A volume ramp 0 -> 1 over the bar: the second half is far louder
// than the first (RMS ratio ~2.6 for a linear ramp).
TEST(Section, VolumeAutomationRamps) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69, 1.0f, 16);  // whole-bar note
  add_note_track(seq, std::move(pattern));
  edm::Section section("fade", 1, std::move(seq));
  ASSERT_TRUE(section.bind_track_volume(
      0, edm::Automation({{0.0, 0.0}, {1.0, 1.0}}, edm::Curve::Linear)));

  edm::Buffer buffer(section.frames());
  section.render(buffer.data());
  const std::size_t half = buffer.size() / 2;
  const float first_rms = window_rms(buffer, 0, half);
  const float second_rms = window_rms(buffer, half, buffer.size());
  EXPECT_GT(second_rms, first_rms * 2.0f);
  EXPECT_GT(second_rms, 0.1f);
}

// A cutoff automation bound to a SynthVoice track changes the
// output: a constant 500 Hz cutoff on a bypassed saw voice cuts
// the energy like a statically filtered voice.
TEST(Section, CutoffAutomationChangesOutput) {
  auto render_with_cutoff = [](bool bound) {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern pattern(16);
    pattern.set(0, 81, 1.0f, 16);
    add_note_track(seq, std::move(pattern), edm::Wave::Saw);
    edm::Section section("bass", 1, std::move(seq));
    if (bound) {
      const bool ok = section.bind_filter_cutoff(
          0, edm::Automation({{0.0, 500.0}, {1.0, 500.0}}, edm::Curve::Linear));
      EXPECT_TRUE(ok);
    }
    edm::Buffer buffer(section.frames());
    section.render(buffer.data());
    return buffer;
  };
  const edm::Buffer dry = render_with_cutoff(false);
  const edm::Buffer wet = render_with_cutoff(true);
  EXPECT_GT(window_rms(dry, 0, dry.size()), 0.1f);
  EXPECT_LT(window_rms(wet, 0, wet.size()), window_rms(dry, 0, dry.size()) * 0.5f);
}

// Binding failures: a bad track index, an empty curve, and a voice
// that is not filterable all return false. Volume needs no cast,
// so it binds fine on a non-filterable voice.
TEST(Section, BindFailures) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  seq.add_track(std::make_unique<DummyVoice>(), std::move(pattern));
  edm::Section section("dummy", 1, std::move(seq));

  EXPECT_FALSE(section.bind_filter_cutoff(
      0, edm::Automation({{0.0, 500.0}}, edm::Curve::Linear)));
  EXPECT_FALSE(section.bind_filter_cutoff(
      9, edm::Automation({{0.0, 500.0}}, edm::Curve::Linear)));
  EXPECT_FALSE(section.bind_track_volume(
      9, edm::Automation({{0.0, 1.0}}, edm::Curve::Linear)));
  EXPECT_FALSE(section.bind_filter_cutoff(
      0, edm::Automation({}, edm::Curve::Linear)));
  EXPECT_FALSE(
      section.bind_track_volume(0, edm::Automation({}, edm::Curve::Linear)));
  EXPECT_TRUE(section.bind_track_volume(
      0, edm::Automation({{0.0, 1.0}}, edm::Curve::Linear)));
}

// Rebinding replaces the previous curve: a volume ramp bound, then
// replaced by a constant 1.0, renders at full volume throughout.
TEST(Section, RebindReplacesCurve) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69, 1.0f, 16);
  add_note_track(seq, std::move(pattern));
  edm::Section section("fade", 1, std::move(seq));
  ASSERT_TRUE(section.bind_track_volume(
      0, edm::Automation({{0.0, 0.0}, {1.0, 1.0}}, edm::Curve::Linear)));
  ASSERT_TRUE(section.bind_track_volume(
      0, edm::Automation({{0.0, 1.0}}, edm::Curve::Linear)));

  edm::Buffer buffer(section.frames());
  section.render(buffer.data());
  const std::size_t half = buffer.size() / 2;
  // Full volume throughout: the first half is as loud as the second
  // (within the attack transient's small effect).
  EXPECT_GT(window_rms(buffer, 0, half), window_rms(buffer, half, buffer.size()) * 0.9f);
}
