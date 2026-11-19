// Arrangement: total frames, section order, determinism, empty
// silence, section independence (no kick leaks into the
// breakdown), render_to_wav.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

#include "edm/arrangement.hxx"
#include "edm/buffer.hxx"
#include "edm/kick_voice.hxx"
#include "edm/pattern.hxx"
#include "edm/section.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"

namespace {
constexpr int kSampleRate = 44100;
constexpr double kBpm = 128.0;
constexpr std::size_t kBarFrames = 16 * 5168;  // 82688 at 128 BPM / 44.1 kHz

edm::SynthVoiceConfig plain_config(edm::Wave wave) {
  edm::SynthVoiceConfig config;
  config.wave = wave;
  config.attack_seconds = 0.001;
  config.decay_seconds = 0.01;
  config.sustain_level = 1.0;
  config.release_seconds = 0.02;
  config.filter_cutoff_hz = 0.0;
  config.volume = 0.5f;
  return config;
}

// A 1-bar section with a single whole-bar sine note at `midi`.
edm::Section note_section(const char* name, int midi) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern pattern(16);
  pattern.set(0, midi, 1.0f, 16);
  seq.add_track(edm::make_synth_voice(plain_config(edm::Wave::Sine), kSampleRate),
                std::move(pattern));
  return edm::Section(name, 1, std::move(seq));
}

float window_peak(const edm::Buffer& b, std::size_t from, std::size_t to) {
  float max = 0.0f;
  for (std::size_t i = from; i < to; ++i) {
    const float a = std::abs(b[i]);
    if (a > max) max = a;
  }
  return max;
}

double window_zcr_freq(const edm::Buffer& b, std::size_t from, std::size_t to) {
  std::size_t crossings = 0;
  for (std::size_t i = from + 1; i < to; ++i) {
    if ((b[i - 1] >= 0.0f) != (b[i] >= 0.0f)) ++crossings;
  }
  const double seconds = static_cast<double>(to - from) / kSampleRate;
  return static_cast<double>(crossings) / (2.0 * seconds);
}
} // namespace

TEST(Arrangement, TotalFrames) {
  edm::Arrangement song(kBpm, kSampleRate);
  EXPECT_EQ(song.frames(), 0u);
  EXPECT_EQ(song.section_count(), 0u);
  song.add_section(note_section("a", 69));
  edm::Sequencer seq(kBpm, kSampleRate);
  song.add_section(edm::Section("b", 2, std::move(seq)));
  EXPECT_EQ(song.section_count(), 2u);
  EXPECT_EQ(song.frames(), 3 * kBarFrames);
  EXPECT_DOUBLE_EQ(song.bpm(), kBpm);
  EXPECT_DOUBLE_EQ(song.sample_rate(), kSampleRate);
}

// Sections render in order: the first bar is MIDI 69 (440 Hz),
// the second bar is MIDI 81 (880 Hz) — the zero-crossing rate
// doubles at the boundary.
TEST(Arrangement, Order) {
  edm::Arrangement song(kBpm, kSampleRate);
  song.add_section(note_section("low", 69));
  song.add_section(note_section("high", 81));
  edm::Buffer buffer(song.frames());
  song.render(buffer.data());
  const double first = window_zcr_freq(buffer, 0, kBarFrames);
  const double second = window_zcr_freq(buffer, kBarFrames, 2 * kBarFrames);
  EXPECT_GT(first, 300.0);
  EXPECT_LT(first, 600.0);
  EXPECT_GT(second, first * 1.5);
}

// Rendering twice reproduces bit-identically.
TEST(Arrangement, Determinism) {
  edm::Arrangement song(kBpm, kSampleRate);
  song.add_section(note_section("a", 69));
  song.add_section(note_section("b", 81));
  edm::Buffer first(song.frames());
  song.render(first.data());
  edm::Buffer second(song.frames());
  song.render(second.data());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}

// An empty arrangement renders silence into the whole span.
TEST(Arrangement, EmptySilence) {
  edm::Arrangement song(kBpm, kSampleRate);
  edm::Buffer buffer(1000);
  for (std::size_t i = 0; i < buffer.size(); ++i) buffer[i] = 1.0f;
  song.render(buffer.data());
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer[i], 0.0f) << "sample " << i;
  }
}

// Sections render independently: a drop section with a kick
// followed by a breakdown section without one — the breakdown
// region is bit-identical to the breakdown section rendered
// alone, and has no kick in it (peak far below the kick's 0.95).
TEST(Arrangement, NoKickInBreakdown) {
  // Drop: kick 4-on-the-floor + bass.
  edm::Sequencer drop_seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern(16);
  for (int step = 0; step < 16; step += 4) kick_pattern.set(step, 36, 1.0f);
  drop_seq.add_track(
      std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
      std::move(kick_pattern));
  edm::Section drop("drop", 1, std::move(drop_seq));

  // Breakdown: a sine arpeggio note, no kick track at all.
  edm::Section breakdown = note_section("breakdown", 81);

  edm::Arrangement song(kBpm, kSampleRate);
  song.add_section(std::move(drop));
  song.add_section(std::move(breakdown));
  edm::Buffer mix(song.frames());
  song.render(mix.data());

  // The drop bar has the kick (peak near 0.95).
  EXPECT_GT(window_peak(mix, 0, kBarFrames), 0.8f);
  // The breakdown bar has no kick: peak is the sine level, and
  // the ZCR is the sine's 880 Hz, not the kick's ~45-150 Hz.
  EXPECT_LT(window_peak(mix, kBarFrames, 2 * kBarFrames), 0.7f);
  EXPECT_GT(window_zcr_freq(mix, kBarFrames, 2 * kBarFrames), 600.0);

  // And the breakdown region equals the section rendered alone.
  edm::Section alone = note_section("breakdown", 81);
  edm::Buffer solo(alone.frames());
  alone.render(solo.data());
  for (std::size_t i = 0; i < solo.size(); ++i) {
    EXPECT_FLOAT_EQ(mix[kBarFrames + i], solo[i]) << "sample " << i;
  }
}

// render_to_wav writes a valid 16-bit mono WAV of the right size.
TEST(Arrangement, RenderToWav) {
  edm::Arrangement song(kBpm, kSampleRate);
  song.add_section(note_section("a", 69));
  const std::string path = "test_arrangement_tmp.wav";
  ASSERT_TRUE(song.render_to_wav(path));

  std::ifstream file(path, std::ios::binary | std::ios::ate);
  ASSERT_TRUE(file.good());
  const std::streamoff size = file.tellg();
  // 44-byte header + 2 bytes per sample.
  EXPECT_EQ(size, 44 + static_cast<std::streamoff>(song.frames() * 2));
  file.close();
  std::error_code ec;
  std::filesystem::remove(path, ec);
  EXPECT_FALSE(ec);
}
