// EDM voices: KickVoice pitch drop / duration / one-shot behavior.
// (SupersawVoice, NoiseVoice, RiserVoice tests join this file in
// later commits of the v0.0.7 round.)

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>

#include "edm/buffer.hxx"
#include "edm/kick_voice.hxx"
#include "edm/voice.hxx"

namespace {
constexpr int kSampleRate = 44100;

float window_peak(const edm::Buffer& b, std::size_t from, std::size_t to) {
  float max = 0.0f;
  for (std::size_t i = from; i < to; ++i) {
    const float a = std::abs(b[i]);
    if (a > max) max = a;
  }
  return max;
}

// Zero-crossing rate over a window, as a frequency (crossings per
// second / 2).
double window_zcr_freq(const edm::Buffer& b, std::size_t from, std::size_t to) {
  std::size_t crossings = 0;
  for (std::size_t i = from + 1; i < to; ++i) {
    if ((b[i - 1] >= 0.0f) != (b[i] >= 0.0f)) ++crossings;
  }
  const double seconds = static_cast<double>(to - from) / kSampleRate;
  return static_cast<double>(crossings) / (2.0 * seconds);
}

void render(edm::Voice& voice, edm::Buffer& buffer) {
  for (std::size_t i = 0; i < buffer.size(); ++i) buffer[i] = voice.next();
}
} // namespace

// The pitch drops 150 -> 45 Hz: the zero-crossing rate in the first
// 100 ms is well above the rate in the last 100 ms (~75 Hz vs
// ~45 Hz from the spike's numbers).
TEST(KickVoice, PitchDrops) {
  edm::KickVoice kick(edm::KickVoiceConfig{}, kSampleRate);
  kick.note_on(36, 1.0f);
  edm::Buffer buffer(static_cast<std::size_t>(0.30 * kSampleRate));
  render(kick, buffer);
  EXPECT_GT(window_peak(buffer, 0, buffer.size()), 0.5f);
  const double early = window_zcr_freq(buffer, 0, static_cast<std::size_t>(0.1 * kSampleRate));
  const double late =
      window_zcr_freq(buffer, static_cast<std::size_t>(0.2 * kSampleRate), buffer.size());
  EXPECT_GT(early, late * 1.3);
}

// Silence after the 0.30 s duration — exactly, not just quiet.
TEST(KickVoice, SilenceAfterDuration) {
  edm::KickVoice kick(edm::KickVoiceConfig{}, kSampleRate);
  kick.note_on(36, 1.0f);
  edm::Buffer buffer(static_cast<std::size_t>(0.5 * kSampleRate));
  render(kick, buffer);
  EXPECT_GT(window_peak(buffer, 0, static_cast<std::size_t>(0.3 * kSampleRate)), 0.1f);
  for (std::size_t i = static_cast<std::size_t>(0.3 * kSampleRate); i < buffer.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer[i], 0.0f) << "sample " << i;
  }
}

// note_off is a no-op: a kick with a note_off mid-decay renders
// bit-identically to one without it.
TEST(KickVoice, NoteOffIsNoOp) {
  edm::KickVoice a(edm::KickVoiceConfig{}, kSampleRate);
  a.note_on(36, 1.0f);
  edm::Buffer buffer_a(static_cast<std::size_t>(0.3 * kSampleRate));
  for (std::size_t i = 0; i < buffer_a.size(); ++i) {
    if (i == static_cast<std::size_t>(0.1 * kSampleRate)) a.note_off(36);
    buffer_a[i] = a.next();
  }

  edm::KickVoice b(edm::KickVoiceConfig{}, kSampleRate);
  b.note_on(36, 1.0f);
  edm::Buffer buffer_b(buffer_a.size());
  render(b, buffer_b);

  EXPECT_GT(window_peak(buffer_a, static_cast<std::size_t>(0.1 * kSampleRate),
                        buffer_a.size()),
            0.1f);
  for (std::size_t i = 0; i < buffer_a.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer_a[i], buffer_b[i]) << "sample " << i;
  }
}

// A retrigger restarts the pitch drop and the phase: the samples
// after a retrigger are bit-identical to a fresh note.
TEST(KickVoice, RetriggerRestarts) {
  edm::KickVoice a(edm::KickVoiceConfig{}, kSampleRate);
  a.note_on(36, 1.0f);
  edm::Buffer warmup(static_cast<std::size_t>(0.15 * kSampleRate));
  render(a, warmup);
  a.note_on(36, 1.0f);  // retrigger
  edm::Buffer after(static_cast<std::size_t>(0.1 * kSampleRate));
  render(a, after);

  edm::KickVoice b(edm::KickVoiceConfig{}, kSampleRate);
  b.note_on(36, 1.0f);
  edm::Buffer fresh(after.size());
  render(b, fresh);

  for (std::size_t i = 0; i < after.size(); ++i) {
    EXPECT_FLOAT_EQ(after[i], fresh[i]) << "sample " << i;
  }
}
