// Section: exact frames, pattern placement, long patterns playing
// once, determinism, volume + cutoff automation bindings.

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

#include "edm/buffer.hxx"
#include "edm/kick_voice.hxx"
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

// --- Sidechain ---

// A 1-bar section: kick 4-on-the-floor (steps 0/4/8/12) + a
// sustained sine bass (whole-bar note). Returns the section with
// the kick on track 0 and the bass on track 1.
edm::Section kick_and_bass_section(bool sidechain) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern(16);
  for (int step = 0; step < 16; step += 4) kick_pattern.set(step, 36, 1.0f);
  seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                std::move(kick_pattern));
  edm::Pattern bass_pattern(16);
  bass_pattern.set(0, 45, 1.0f, 16);  // whole-bar note
  add_note_track(seq, std::move(bass_pattern));
  edm::Section section("drop", 1, std::move(seq));
  if (sidechain) section.enable_sidechain(0);
  return section;
}

// The kick bus is unaffected by the sidechain machinery: a section
// with a kick + a silent music track, sidechain on, renders
// bit-identically to a kick-only section.
TEST(Section, SidechainKickUnaffected) {
  edm::Sequencer seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern(16);
  for (int step = 0; step < 16; step += 4) kick_pattern.set(step, 36, 1.0f);
  seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                std::move(kick_pattern));
  edm::Pattern empty_pattern(16);  // all rests — silent music bus
  add_note_track(seq, std::move(empty_pattern));
  edm::Section with_sidechain("drop", 1, std::move(seq));
  with_sidechain.enable_sidechain(0);

  edm::Sequencer kick_only_seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern2(16);
  for (int step = 0; step < 16; step += 4) kick_pattern2.set(step, 36, 1.0f);
  kick_only_seq.add_track(
      std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
      std::move(kick_pattern2));
  edm::Section kick_only("kick", 1, std::move(kick_only_seq));

  edm::Buffer a(with_sidechain.frames());
  with_sidechain.render(a.data());
  edm::Buffer b(kick_only.frames());
  kick_only.render(b.data());
  EXPECT_GT(window_peak(a, 0, a.size()), 0.1f);
  for (std::size_t i = 0; i < a.size(); ++i) {
    EXPECT_FLOAT_EQ(a[i], b[i]) << "sample " << i;
  }
}

// The pump: the music RMS in the 50 ms after a kick is under 60%
// of the unducked music in the same window (~0.48 expected from
// the 0.7 depth / 0.1 s release), and back above 85% once the
// ducking has recovered. The ducked music is recovered from the
// mix by subtracting the kick-only render (mix = kick + ducked
// music, so mix - kick isolates the ducked music).
TEST(Section, SidechainDucksMusic) {
  edm::Section section = kick_and_bass_section(true);
  edm::Buffer mix(section.frames());
  section.render(mix.data());

  edm::Sequencer kick_seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern(16);
  for (int step = 0; step < 16; step += 4) kick_pattern.set(step, 36, 1.0f);
  kick_seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                     std::move(kick_pattern));
  edm::Section kick_only("kick", 1, std::move(kick_seq));
  edm::Buffer kick(kick_only.frames());
  kick_only.render(kick.data());

  edm::Sequencer music_seq(kBpm, kSampleRate);
  edm::Pattern bass_pattern(16);
  bass_pattern.set(0, 45, 1.0f, 16);
  add_note_track(music_seq, std::move(bass_pattern));
  edm::Section music_only("bass", 1, std::move(music_seq));
  edm::Buffer music(music_only.frames());
  music_only.render(music.data());

  // Second kick (step 4, 0.469 s) — clear of the bass attack.
  const std::size_t kick2 = 4 * kStepFrames;
  const std::size_t ducked_from = kick2 + static_cast<std::size_t>(0.01 * kSampleRate);
  const std::size_t ducked_to = kick2 + static_cast<std::size_t>(0.06 * kSampleRate);
  const std::size_t recovered_from = kick2 + static_cast<std::size_t>(0.20 * kSampleRate);
  const std::size_t recovered_to = kick2 + static_cast<std::size_t>(0.25 * kSampleRate);

  auto ducked_music_rms = [&](std::size_t from, std::size_t to) {
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i) {
      const double d = static_cast<double>(mix[i] - kick[i]);
      sum += d * d;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
  };

  EXPECT_GT(window_rms(music, ducked_from, ducked_to), 0.1f);
  EXPECT_LT(ducked_music_rms(ducked_from, ducked_to),
            window_rms(music, ducked_from, ducked_to) * 0.6f);
  EXPECT_GT(ducked_music_rms(recovered_from, recovered_to),
            window_rms(music, recovered_from, recovered_to) * 0.85f);
}

// Mix == kick + ducked music, sample by sample: the expected mix
// is computed independently here from a kick-only render, a
// music-only render, and the gain curve from the kick times.
TEST(Section, SidechainMixEqualsKickPlusDuckedMusic) {
  edm::Section section = kick_and_bass_section(true);
  edm::Buffer mix(section.frames());
  section.render(mix.data());

  // Kick-only render.
  edm::Sequencer kick_seq(kBpm, kSampleRate);
  edm::Pattern kick_pattern(16);
  for (int step = 0; step < 16; step += 4) kick_pattern.set(step, 36, 1.0f);
  kick_seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                     std::move(kick_pattern));
  edm::Section kick_only("kick", 1, std::move(kick_seq));
  edm::Buffer kick(kick_only.frames());
  kick_only.render(kick.data());

  // Music-only render.
  edm::Sequencer music_seq(kBpm, kSampleRate);
  edm::Pattern bass_pattern(16);
  bass_pattern.set(0, 45, 1.0f, 16);
  add_note_track(music_seq, std::move(bass_pattern));
  edm::Section music_only("bass", 1, std::move(music_seq));
  edm::Buffer music(music_only.frames());
  music_only.render(music.data());

  // Kick times (frames): steps 0/4/8/12.
  const std::size_t kick_times[4] = {0, 4 * kStepFrames, 8 * kStepFrames, 12 * kStepFrames};
  for (std::size_t i = 0; i < mix.size(); ++i) {
    std::size_t last_kick = 0;
    for (std::size_t k = 0; k < 4; ++k) {
      if (kick_times[k] <= i) last_kick = kick_times[k];
    }
    const double t = static_cast<double>(i - last_kick) / kSampleRate;
    const double gain = 1.0 - 0.7 * std::exp(-t / 0.1);
    const float expected = kick[i] + music[i] * static_cast<float>(gain);
    EXPECT_NEAR(mix[i], expected, 1e-6f) << "sample " << i;
  }
}

// No kicks means no ducking: a kick track with an empty pattern
// renders bit-identically with sidechain on or off.
TEST(Section, SidechainNoKicksMeansNoDucking) {
  auto build = [](bool sidechain) {
    edm::Sequencer seq(kBpm, kSampleRate);
    edm::Pattern empty_kick(16);  // no notes
    seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                  std::move(empty_kick));
    edm::Pattern bass_pattern(16);
    bass_pattern.set(0, 45, 1.0f, 16);
    add_note_track(seq, std::move(bass_pattern));
    edm::Section section("drop", 1, std::move(seq));
    if (sidechain) section.enable_sidechain(0);
    return section;
  };
  edm::Section with = build(true);
  edm::Section without = build(false);
  edm::Buffer a(with.frames());
  with.render(a.data());
  edm::Buffer b(without.frames());
  without.render(b.data());
  EXPECT_GT(window_peak(a, 0, a.size()), 0.1f);
  for (std::size_t i = 0; i < a.size(); ++i) {
    EXPECT_FLOAT_EQ(a[i], b[i]) << "sample " << i;
  }
}

// A sidechain render is deterministic: rendering twice reproduces
// bit-identically (buses reused, sequencer reset each pass).
TEST(Section, SidechainDeterminism) {
  edm::Section section = kick_and_bass_section(true);
  edm::Buffer first(section.frames());
  section.render(first.data());
  edm::Buffer second(section.frames());
  section.render(second.data());
  for (std::size_t i = 0; i < first.size(); ++i) {
    EXPECT_FLOAT_EQ(first[i], second[i]) << "sample " << i;
  }
}
