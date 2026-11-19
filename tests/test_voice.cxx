// Voice: midi_to_freq known values, SynthVoice frequency /
// note_off / retrigger / filter / factory behavior.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/synth_voice.hxx"
#include "edm/voice.hxx"

namespace {
constexpr int kSampleRate = 44100;

edm::SynthVoiceConfig plain_config() {
  edm::SynthVoiceConfig config;
  config.attack_seconds = 0.001;
  config.decay_seconds = 0.01;
  config.sustain_level = 1.0;
  config.release_seconds = 0.05;
  config.filter_cutoff_hz = 0.0;  // bypass
  config.volume = 0.5f;
  return config;
}

float peak(const edm::Buffer& buffer) {
  float max = 0.0f;
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    const float a = std::abs(buffer[i]);
    if (a > max) max = a;
  }
  return max;
}

float rms(const edm::Buffer& buffer) {
  double sum = 0.0;
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    sum += static_cast<double>(buffer[i]) * buffer[i];
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(buffer.size())));
}

// Zero-crossing rate over the whole buffer: crossings per second / 2
// is the frequency for sine/square/saw.
double zcr_freq(const edm::Buffer& buffer) {
  std::size_t crossings = 0;
  for (std::size_t i = 1; i < buffer.size(); ++i) {
    if ((buffer[i - 1] >= 0.0f) != (buffer[i] >= 0.0f)) ++crossings;
  }
  const double seconds = static_cast<double>(buffer.size()) / kSampleRate;
  return static_cast<double>(crossings) / (2.0 * seconds);
}

void render(edm::Voice& voice, edm::Buffer& buffer) {
  for (std::size_t i = 0; i < buffer.size(); ++i) buffer[i] = voice.next();
}
} // namespace

TEST(Voice, MidiToFreq) {
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(69), 440.0);
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(81), 880.0);
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(57), 220.0);
  EXPECT_NEAR(edm::midi_to_freq(60), 261.6255653005986, 0.001);
  EXPECT_NEAR(edm::midi_to_freq(0), 8.175798915643707, 0.001);
  // Alternate concert pitch: the template parameter is the A4 reference.
  EXPECT_DOUBLE_EQ(edm::midi_to_freq<432.0>(69), 432.0);
  EXPECT_DOUBLE_EQ(edm::midi_to_freq<432.0>(81), 864.0);
  EXPECT_NEAR(edm::midi_to_freq<432.0>(60), 256.86873684058776, 0.001);
}

// A sine voice on A4 renders at 440 Hz (within 5%).
TEST(SynthVoice, Frequency) {
  edm::SynthVoice<edm::Wave::Sine> voice(plain_config(), kSampleRate);
  voice.note_on(69, 1.0f);
  edm::Buffer buffer(4410);  // 0.1 s
  render(voice, buffer);
  EXPECT_GT(peak(buffer), 0.1f);
  EXPECT_NEAR(zcr_freq(buffer), 440.0, 440.0 * 0.05);
}

// note_off releases the envelope: after the release time the voice
// is exactly silent.
TEST(SynthVoice, NoteOffSilences) {
  edm::SynthVoice<edm::Wave::Saw> voice(plain_config(), kSampleRate);
  voice.note_on(69, 1.0f);
  edm::Buffer sounding(4410);
  render(voice, sounding);
  EXPECT_GT(peak(sounding), 0.1f);
  voice.note_off(69);
  edm::Buffer tail(8820);  // 0.2 s, release is 0.05 s
  render(voice, tail);
  // The last 0.1 s (past the release) is exactly silent.
  for (std::size_t i = 4410; i < tail.size(); ++i) {
    EXPECT_FLOAT_EQ(tail[i], 0.0f);
  }
}

// A retrigger restarts the envelope and the phase: the samples
// after a retrigger are bit-identical to a fresh note.
TEST(SynthVoice, RetriggerRestarts) {
  edm::SynthVoice<edm::Wave::Sine> a(plain_config(), kSampleRate);
  a.note_on(69, 1.0f);
  edm::Buffer warmup(2000);  // through attack + decay to sustain
  render(a, warmup);
  a.note_on(69, 1.0f);  // retrigger
  edm::Buffer after(200);
  render(a, after);

  edm::SynthVoice<edm::Wave::Sine> b(plain_config(), kSampleRate);
  b.note_on(69, 1.0f);
  edm::Buffer fresh(200);
  render(b, fresh);

  for (std::size_t i = 0; i < after.size(); ++i) {
    EXPECT_FLOAT_EQ(after[i], fresh[i]);
  }
}

// note_off for a stale note must not kill the sounding note.
TEST(SynthVoice, NoteOffIgnoresStaleNote) {
  edm::SynthVoice<edm::Wave::Sine> voice(plain_config(), kSampleRate);
  voice.note_on(60, 1.0f);
  voice.note_on(64, 1.0f);  // retrigger: 64 is sounding now
  voice.note_off(60);       // stale: must not release
  edm::Buffer buffer(4410);
  render(voice, buffer);
  EXPECT_GT(peak(buffer), 0.1f);
  voice.note_off(64);  // the real note: releases
  edm::Buffer tail(8820);
  render(voice, tail);
  for (std::size_t i = 4410; i < tail.size(); ++i) {
    EXPECT_FLOAT_EQ(tail[i], 0.0f);
  }
}

// note_off with no active note is a safe no-op.
TEST(SynthVoice, NoteOffWithNoNoteIsSafe) {
  edm::SynthVoice<edm::Wave::Sine> voice(plain_config(), kSampleRate);
  voice.note_off(60);
  edm::Buffer buffer(100);
  render(voice, buffer);
  EXPECT_FLOAT_EQ(peak(buffer), 0.0f);
}

// A 500 Hz lowpass on an 880 Hz saw cuts most of the energy
// (the fundamental and all harmonics sit above the cutoff).
TEST(SynthVoice, FilterCutsHarmonics) {
  edm::SynthVoiceConfig bypass = plain_config();
  edm::SynthVoice<edm::Wave::Saw> dry(bypass, kSampleRate);
  dry.note_on(81, 1.0f);
  edm::Buffer dry_buffer(4410);
  render(dry, dry_buffer);

  edm::SynthVoiceConfig filtered = plain_config();
  filtered.filter_cutoff_hz = 500.0;
  edm::SynthVoice<edm::Wave::Saw> wet(filtered, kSampleRate);
  wet.note_on(81, 1.0f);
  edm::Buffer wet_buffer(4410);
  render(wet, wet_buffer);

  EXPECT_GT(rms(dry_buffer), 0.1f);
  EXPECT_LT(rms(wet_buffer), rms(dry_buffer) * 0.5f);
}

// filter_q is wired, not decorative: the same filtered note renders
// differently at q 0.7 and q 5.0 (the resonance changes every
// filter coefficient, so the samples differ from the first one).
TEST(SynthVoice, FilterQChangesOutput) {
  edm::SynthVoiceConfig low_q = plain_config();
  low_q.filter_cutoff_hz = 500.0;
  low_q.filter_q = 0.7;
  edm::SynthVoice<edm::Wave::Saw> voice_low(low_q, kSampleRate);
  voice_low.note_on(81, 1.0f);
  edm::Buffer buffer_low(4410);
  render(voice_low, buffer_low);

  edm::SynthVoiceConfig high_q = plain_config();
  high_q.filter_cutoff_hz = 500.0;
  high_q.filter_q = 5.0;
  edm::SynthVoice<edm::Wave::Saw> voice_high(high_q, kSampleRate);
  voice_high.note_on(81, 1.0f);
  edm::Buffer buffer_high(4410);
  render(voice_high, buffer_high);

  EXPECT_GT(peak(buffer_low), 0.0f);
  EXPECT_GT(peak(buffer_high), 0.0f);
  std::size_t differing = 0;
  for (std::size_t i = 0; i < buffer_low.size(); ++i) {
    if (buffer_low[i] != buffer_high[i]) ++differing;
  }
  EXPECT_GT(differing, 0u);
}

// The factory builds the wave the config names: a sine voice peaks
// at the volume, a square voice at 0.8x the volume.
TEST(SynthVoice, FactorySelectsWave) {
  edm::SynthVoiceConfig sine_config = plain_config();
  sine_config.wave = edm::Wave::Sine;
  const std::unique_ptr<edm::Voice> sine =
      edm::make_synth_voice(sine_config, kSampleRate);
  ASSERT_NE(sine, nullptr);
  sine->note_on(69, 1.0f);
  edm::Buffer sine_buffer(4410);
  render(*sine, sine_buffer);
  EXPECT_GT(peak(sine_buffer), 0.45f);

  edm::SynthVoiceConfig square_config = plain_config();
  square_config.wave = edm::Wave::Square;
  const std::unique_ptr<edm::Voice> square =
      edm::make_synth_voice(square_config, kSampleRate);
  ASSERT_NE(square, nullptr);
  square->note_on(69, 1.0f);
  edm::Buffer square_buffer(4410);
  render(*square, square_buffer);
  EXPECT_GT(peak(square_buffer), 0.35f);
  EXPECT_LT(peak(square_buffer), 0.45f);
}

// Noise voices are deterministic through the factory: same seed,
// same samples.
TEST(SynthVoice, FactoryNoiseDeterminism) {
  edm::SynthVoiceConfig config = plain_config();
  config.wave = edm::Wave::Noise;
  const std::unique_ptr<edm::Voice> a =
      edm::make_synth_voice(config, kSampleRate, edm::Rng(42));
  const std::unique_ptr<edm::Voice> b =
      edm::make_synth_voice(config, kSampleRate, edm::Rng(42));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  a->note_on(60, 1.0f);
  b->note_on(60, 1.0f);
  edm::Buffer buffer_a(4410);
  edm::Buffer buffer_b(4410);
  render(*a, buffer_a);
  render(*b, buffer_b);
  EXPECT_GT(peak(buffer_a), 0.1f);
  for (std::size_t i = 0; i < buffer_a.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer_a[i], buffer_b[i]);
  }
}
