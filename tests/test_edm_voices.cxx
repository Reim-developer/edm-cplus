// EDM voices: KickVoice pitch drop / duration / one-shot behavior.
// (SupersawVoice, NoiseVoice, RiserVoice tests join this file in
// later commits of the v0.0.7 round.)

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>

#include "edm/buffer.hxx"
#include "edm/constants.hxx"
#include "edm/kick_voice.hxx"
#include "edm/oscillator.hxx"
#include "edm/supersaw_voice.hxx"
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

float window_rms(const edm::Buffer& b, std::size_t from, std::size_t to) {
  double sum = 0.0;
  for (std::size_t i = from; i < to; ++i) {
    sum += static_cast<double>(b[i]) * b[i];
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
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

// --- SupersawVoice ---

// The stack adds energy over a single saw at the same per-saw
// gain (0.35/7): 7 detuned saws vs 1 — even fully decorrelated
// the RMS ratio is sqrt(7) ~= 2.65.
TEST(SupersawVoice, EnergyAboveSingleSaw) {
  edm::SupersawVoiceConfig config;
  edm::SupersawVoice stack(config, kSampleRate);
  stack.note_on(69, 1.0f);
  edm::Buffer stacked(static_cast<std::size_t>(0.5 * kSampleRate));
  render(stack, stacked);

  // One saw, same envelope, same per-saw gain and volume.
  edm::Oscillator<edm::Wave::Saw> single;
  single.set_frequency(edm::midi_to_freq(69), kSampleRate);
  edm::Buffer alone(stacked.size());
  for (std::size_t i = 0; i < alone.size(); ++i) {
    const double t = static_cast<double>(i) / kSampleRate;
    const double env = std::min(1.0, t / config.attack_seconds) *
                       std::exp(-t / config.decay_seconds);
    alone[i] = static_cast<float>(single.next() *
                                  (edm::kSupersawGain / edm::kSupersawVoices) * env *
                                  config.volume);
  }

  const std::size_t skip = static_cast<std::size_t>(0.05 * kSampleRate);
  const float stack_rms = window_rms(stacked, skip, stacked.size());
  const float single_rms = window_rms(alone, skip, alone.size());
  EXPECT_GT(stack_rms, 0.02f);  // sounding, after the decay window
  EXPECT_GT(stack_rms, single_rms * 2.0f);
}

// Consecutive identical notes render bit-identically: the 7 saw
// phases reset on every note_on.
TEST(SupersawVoice, ConsecutiveNotesIdentical) {
  edm::SupersawVoice voice(edm::SupersawVoiceConfig{}, kSampleRate);
  voice.note_on(69, 1.0f);
  edm::Buffer first(static_cast<std::size_t>(0.3 * kSampleRate));
  render(voice, first);
  voice.note_on(69, 1.0f);  // retrigger, same note
  edm::Buffer second(first.size());
  render(voice, second);
  EXPECT_GT(window_peak(first, 0, first.size()), 0.1f);
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}

// Two voices with the same config and notes render bit-identically.
TEST(SupersawVoice, Deterministic) {
  edm::SupersawVoice a(edm::SupersawVoiceConfig{}, kSampleRate);
  edm::SupersawVoice b(edm::SupersawVoiceConfig{}, kSampleRate);
  a.note_on(74, 0.9f);
  b.note_on(74, 0.9f);
  edm::Buffer buffer_a(static_cast<std::size_t>(0.5 * kSampleRate));
  edm::Buffer buffer_b(buffer_a.size());
  render(a, buffer_a);
  render(b, buffer_b);
  for (std::size_t i = 0; i < buffer_a.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer_a[i], buffer_b[i]) << "sample " << i;
  }
}

// note_off cuts hard (documented: the one-shot envelope has no
// release stage) — exact silence after the cut.
TEST(SupersawVoice, NoteOffCutsHard) {
  edm::SupersawVoice voice(edm::SupersawVoiceConfig{}, kSampleRate);
  voice.note_on(69, 1.0f);
  edm::Buffer buffer(static_cast<std::size_t>(0.3 * kSampleRate));
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    if (i == static_cast<std::size_t>(0.1 * kSampleRate)) voice.note_off(69);
    buffer[i] = voice.next();
  }
  EXPECT_GT(window_peak(buffer, 0, static_cast<std::size_t>(0.1 * kSampleRate)), 0.1f);
  for (std::size_t i = static_cast<std::size_t>(0.1 * kSampleRate); i < buffer.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer[i], 0.0f) << "sample " << i;
  }
}

// The envelope ends at attack + 6*decay (1.505 s with the default
// config): silence well past the end, sound before it.
TEST(SupersawVoice, SilenceAfterEnvelope) {
  edm::SupersawVoice voice(edm::SupersawVoiceConfig{}, kSampleRate);
  voice.note_on(69, 1.0f);
  edm::Buffer buffer(static_cast<std::size_t>(2.0 * kSampleRate));
  render(voice, buffer);
  // Still sounding well before the end (the envelope is exp decay,
  // so the last 100 ms before the gate are near-silent by design).
  EXPECT_GT(window_peak(buffer, static_cast<std::size_t>(1.0 * kSampleRate),
                        static_cast<std::size_t>(1.2 * kSampleRate)),
            0.001f);
  for (std::size_t i = static_cast<std::size_t>(1.6 * kSampleRate); i < buffer.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer[i], 0.0f) << "sample " << i;
  }
}
