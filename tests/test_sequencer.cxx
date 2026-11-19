// Sequencer: frame helpers, step-boundary accuracy, gate lengths,
// looping, mixing, determinism, silence.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/pattern.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"

namespace {
constexpr int kSampleRate = 44100;
constexpr double kBpm = 140.0;
// At 140 BPM / 44.1 kHz a 16th-note step is exactly 4725 frames.
constexpr std::size_t kStepFrames = 4725;

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

// Window for step s (0-based) in a 140 BPM render.
std::size_t step_begin(std::size_t s) { return s * kStepFrames; }
std::size_t step_end(std::size_t s) { return (s + 1) * kStepFrames; }
} // namespace

TEST(Sequencer, FrameHelpers) {
  const edm::Sequencer seq(140.0, kSampleRate);
  EXPECT_EQ(seq.frames_per_step(), 4725);
  EXPECT_EQ(seq.frames_per_bar(), 75600);
  EXPECT_EQ(seq.frames_for_bars(2), 151200);
  EXPECT_EQ(seq.frames_for_bars(0), 0);
  EXPECT_EQ(seq.frames_for_bars(-1), 0);
  // 128 BPM: 5167.97 frames/step, rounded to 5168.
  const edm::Sequencer seq128(128.0, kSampleRate);
  EXPECT_EQ(seq128.frames_per_step(), 5168);
  // Degenerate tempos fall back to 120 BPM.
  const edm::Sequencer seq_bad(0.0, kSampleRate);
  EXPECT_EQ(seq_bad.frames_per_step(), 5513);  // 120 BPM, 5512.5 rounded
}

// A note on every step of a 1-bar pattern: rendering exactly
// frames_per_bar() frames sounds all 16 steps.
TEST(Sequencer, OneBarRendersAllSteps) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  for (int i = 0; i < 16; ++i) pattern.set(i, 60 + i);
  add_note_track(seq, std::move(pattern));

  edm::Buffer buffer(seq.frames_per_bar());
  seq.render(buffer.data());
  for (std::size_t s = 0; s < 16; ++s) {
    EXPECT_GT(window_peak(buffer, step_begin(s), step_end(s)), 0.05f)
        << "step " << s << " is silent";
  }
}

// Sample-exact boundaries: a square note on step 4 sounds from the
// first sample of step 4 — and the steps before it are exactly
// silent (the note does not leak early).
TEST(Sequencer, StepBoundaryAccuracy) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(4, 69);  // length 1, release 0.02 s
  add_note_track(seq, std::move(pattern), edm::Wave::Square);

  edm::Buffer buffer(seq.frames_per_bar());
  seq.render(buffer.data());
  // Steps 0-3 exactly silent.
  for (std::size_t s = 0; s < 4; ++s) {
    EXPECT_TRUE(window_silent(buffer, step_begin(s), step_end(s))) << "step " << s;
  }
  // The first sample of step 4 is already sounding (square starts
  // at +/-0.8 immediately), the last sample of step 3 is silent.
  EXPECT_NE(buffer[step_begin(4)], 0.0f);
  EXPECT_FLOAT_EQ(buffer[step_begin(4) - 1], 0.0f);
  EXPECT_GT(window_peak(buffer, step_begin(4), step_end(4)), 0.1f);
  // The release tail (0.02 s) fits inside step 5; steps 6-15 silent.
  for (std::size_t s = 6; s < 16; ++s) {
    EXPECT_TRUE(window_silent(buffer, step_begin(s), step_end(s))) << "step " << s;
  }
}

// A gate of 2 steps sounds through the second step; a gate of 1
// does not (only its release tail, inside the first half).
TEST(Sequencer, GateLengthTwoSteps) {
  const std::size_t half = kStepFrames / 2;
  {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern pattern(16);
    pattern.set(0, 69, 0.8f, 2);  // gate: steps 0-1
    add_note_track(seq, std::move(pattern));
    edm::Buffer buffer(seq.frames_per_bar());
    seq.render(buffer.data());
    // Second half of step 1 is still sounding.
    EXPECT_GT(window_rms(buffer, step_begin(1) + half, step_end(1)), 0.05f);
    // Second half of step 2 is silent (released at step 2).
    EXPECT_TRUE(window_silent(buffer, step_begin(2) + half, step_end(2)));
  }
  {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern pattern(16);
    pattern.set(0, 69, 0.8f, 1);  // gate: step 0 only
    add_note_track(seq, std::move(pattern));
    edm::Buffer buffer(seq.frames_per_bar());
    seq.render(buffer.data());
    // Second half of step 1 is silent (release ended in step 1's
    // first half: 0.02 s = 882 frames < 2362).
    EXPECT_TRUE(window_silent(buffer, step_begin(1) + half, step_end(1)));
  }
}

// Patterns loop: bar 2 is bit-identical to bar 1 (the release ends
// inside step 1, so the voice is idle again at the loop point).
TEST(Sequencer, PatternsLoop) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  add_note_track(seq, std::move(pattern));

  edm::Buffer buffer(seq.frames_for_bars(2));
  seq.render(buffer.data());
  const std::size_t bar = seq.frames_per_bar();
  for (std::size_t i = 0; i < bar; ++i) {
    EXPECT_FLOAT_EQ(buffer[i], buffer[bar + i]) << "sample " << i;
  }
}

// Tracks may have different pattern lengths: a 12-step pattern's
// note fires at global steps 0, 12, 24 (polyrhythm, for free).
TEST(Sequencer, PatternsOfDifferentLengths) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(12);
  pattern.set(0, 69);
  add_note_track(seq, std::move(pattern));

  edm::Buffer buffer(seq.frames_for_bars(2));
  seq.render(buffer.data());
  EXPECT_GT(window_peak(buffer, step_begin(0), step_end(0)), 0.1f);
  EXPECT_GT(window_peak(buffer, step_begin(12), step_end(12)), 0.1f);
  EXPECT_GT(window_peak(buffer, step_begin(24), step_end(24)), 0.1f);
  // Step 6 is silent (no note there in either loop).
  EXPECT_TRUE(window_silent(buffer, step_begin(6), step_end(6)));
}

// Two tracks mix by summation: the mix is bit-identical to the
// sum of the two tracks rendered alone.
TEST(Sequencer, TwoTracksMix) {
  auto render_track = [](int midi) {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern pattern(16);
    pattern.set(0, midi);
    add_note_track(seq, std::move(pattern));
    edm::Buffer buffer(seq.frames_per_bar());
    seq.render(buffer.data());
    return buffer;
  };

  edm::Sequencer mix(kBpm, kSampleRate);
  edm::Pattern a(16);
  a.set(0, 69);
  add_note_track(mix, std::move(a));
  edm::Pattern b(16);
  b.set(0, 76);
  add_note_track(mix, std::move(b));
  edm::Buffer mixed(mix.frames_per_bar());
  mix.render(mixed.data());

  const edm::Buffer alone_a = render_track(69);
  const edm::Buffer alone_b = render_track(76);
  for (std::size_t i = 0; i < mixed.size(); ++i) {
    EXPECT_FLOAT_EQ(mixed[i], alone_a[i] + alone_b[i]) << "sample " << i;
  }
}

// Same setup rendered twice from fresh sequencers -> identical
// samples, including the seeded noise voice.
TEST(Sequencer, Determinism) {
  auto render_both = []() {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern melody(16);
    melody.set(0, 69);
    melody.set(8, 72);
    add_note_track(seq, std::move(melody));
    edm::Pattern noise(16);
    noise.set(4, 60);
    edm::SynthVoiceConfig config = plain_config(edm::Wave::Noise);
    seq.add_track(edm::make_synth_voice(config, kSampleRate, edm::Rng(42)),
                  std::move(noise));
    edm::Buffer buffer(seq.frames_per_bar());
    seq.render(buffer.data());
    return buffer;
  };
  const edm::Buffer first = render_both();
  const edm::Buffer second = render_both();
  EXPECT_GT(window_peak(first, 0, first.size()), 0.1f);
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}

// An empty pattern renders silence, and so does a sequencer with
// no tracks at all.
TEST(Sequencer, EmptyPatternRendersSilence) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern empty(16);
  add_note_track(seq, std::move(empty));
  edm::Buffer buffer(seq.frames_per_bar());
  seq.render(buffer.data());
  EXPECT_TRUE(window_silent(buffer, 0, buffer.size()));

  edm::Sequencer bare(kBpm, kSampleRate);
  edm::Buffer bare_buffer(bare.frames_per_bar());
  bare.render(bare_buffer.data());
  EXPECT_TRUE(window_silent(bare_buffer, 0, bare_buffer.size()));
}

// reset() rewinds the playhead and the voices: rendering again
// after a reset reproduces the first bar bit-identically.
TEST(Sequencer, ResetRewinds) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  pattern.set(8, 76);
  add_note_track(seq, std::move(pattern));

  edm::Buffer first(seq.frames_per_bar());
  seq.render(first.data());
  seq.reset();
  edm::Buffer second(seq.frames_per_bar());
  seq.render(second.data());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}
