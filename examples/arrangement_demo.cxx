// arrangement_demo: the headline proof for v0.0.7 — an ORIGINAL
// 32-bar uplifting electro-house track in TheFatRat "Unity"'s
// style (128 BPM, D minor, supersaw lead, build -> drop), rebuilt
// with the library API (sections, automation, sidechain, the four
// EDM voices). NOT a note-for-note cover of "Unity".
//
// The output is NOT byte-identical to the spike's render
// (spike-game-audio/edm/edm.cpp): different code path — sequencer
// scheduling, block automation, double-render sidechain. What IS
// verified: valid WAV, 60.00 s, section RMS ordering (drop
// loudest, breakdown clearly lowest), determinism (render twice
// -> identical), peak < 1.0, no NaN.
//
// Known limits of this render (documented in the README): the
// lead is dry (no delay — the effects round), no cross-section
// tails, uniform sidechain ducking (the spike ducks the lead
// less than the bass).
//
// Usage: arrangement_demo [output.wav]
//   Default output: unity_style.wav in the current directory.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "edm/arrangement.hxx"
#include "edm/buffer.hxx"
#include "edm/constants.hxx"
#include "edm/kick_voice.hxx"
#include "edm/noise_voice.hxx"
#include "edm/pattern.hxx"
#include "edm/riser_voice.hxx"
#include "edm/section.hxx"
#include "edm/sequencer.hxx"
#include "edm/supersaw_voice.hxx"
#include "edm/synth_voice.hxx"
#include "edm/wav.hxx"

namespace {
constexpr double kBpm = 128.0;
constexpr int kSampleRate = 44100;
constexpr int kBars = 32;

// Chord roots per bar: Dm - Bb - F - C (i-VI-III-VII), MIDI.
constexpr int kRoots[4] = {38, 34, 41, 36};  // D2 Bb1 F2 C2
// Chord tones for the breakdown arpeggio (pluck register).
constexpr int kTones[4][4] = {
    {74, 77, 81, 86},  // Dm: D F A D
    {70, 74, 77, 82},  // Bb: Bb D F Bb
    {77, 81, 84, 89},  // F:  F A C F
    {72, 76, 79, 84},  // C:  C E G C
};
// Original 2-bar lead riff, 8th notes, D minor.
constexpr int kRiff[16] = {74, 77, 81, 77, 79, 77, 76, 74,
                           72, 74, 76, 77, 79, 81, 79, 77};
// Arpeggio shape within a bar (8 steps, repeated twice).
constexpr int kArpShape[8] = {0, 1, 2, 3, 3, 2, 1, 0};

int fail(const char* message) {
  std::fprintf(stderr, "arrangement_demo: %s\n", message);
  return 1;
}

// Kick: four-on-the-floor, velocity 1.0 (the Pattern default is
// 0.8 — a kick at 0.8 would be a quiet kick).
edm::Pattern kick_pattern() {
  edm::Pattern pattern(16);
  for (int step = 0; step < 16; step += 4) pattern.set(step, 36, 1.0f);
  return pattern;
}

// Bass: offbeat 8ths (steps 2,6,10,14, gate 2) on the chord root
// of each bar. Section-length pattern: the root changes per bar,
// so a 16-step loop cannot play it. `first_bar` skips the early
// bars (the intro's bass enters at bar 2).
edm::Pattern bass_pattern(int bars, int first_bar) {
  edm::Pattern pattern(bars * 16);
  for (int bar = first_bar; bar < bars; ++bar) {
    for (int step = 2; step < 16; step += 4) {
      pattern.set(bar * 16 + step, kRoots[bar % 4], 1.0f, 2);
    }
  }
  return pattern;
}

edm::SynthVoiceConfig bass_config(double cutoff_hz, double volume) {
  edm::SynthVoiceConfig config;
  config.wave = edm::Wave::Saw;
  // ADSR approximation of the spike's exp(-t/0.18) bass decay.
  config.attack_seconds = 0.005;
  config.decay_seconds = 0.18;
  config.sustain_level = 0.0;
  config.release_seconds = 0.05;
  config.filter_cutoff_hz = cutoff_hz;
  config.filter_q = 0.9;
  config.volume = static_cast<float>(volume);
  return config;
}

// Lead: the 2-bar riff, 8th notes (gate 2), on the supersaw.
edm::Pattern lead_pattern() {
  edm::Pattern pattern(32);
  for (int eighth = 0; eighth < 16; ++eighth) {
    pattern.set(eighth * 2, kRiff[eighth], 1.0f, 2);
  }
  return pattern;
}

// Hats: offbeat 8ths (steps 2,6,10,14), all sections.
edm::Pattern hats_pattern() {
  edm::Pattern pattern(16);
  for (int step = 2; step < 16; step += 4) pattern.set(step, 42, 1.0f);
  return pattern;
}

// Quiet 16ths in the drops: the odd steps only (the even offbeat
// 8ths are already covered by the hats track).
edm::Pattern hats16_pattern() {
  edm::Pattern pattern(16);
  for (int step = 1; step < 16; step += 2) pattern.set(step, 42, 1.0f);
  return pattern;
}

// Clap on beats 2 & 4 (steps 4, 12) in the drops.
edm::Pattern clap_pattern() {
  edm::Pattern pattern(16);
  pattern.set(4, 39, 1.0f);
  pattern.set(12, 39, 1.0f);
  return pattern;
}

// Riser: one long note starting at step 32 (bar 2) of a 64-step
// pattern — the last 2 bars of a 4-bar build. The voice is
// one-shot (3.75 s = 2 bars at 128 BPM), so the gate is cosmetic.
edm::Pattern riser_pattern() {
  edm::Pattern pattern(64);
  pattern.set(32, 60, 1.0f, 32);
  return pattern;
}

// Snare roll: 16ths in the last bar of a 4-bar build (steps
// 48-63), volume ramping 0.10 -> 0.40. The ramp is a PATTERN
// (per-step velocity), not voice state.
edm::Pattern snare_roll_pattern() {
  edm::Pattern pattern(64);
  for (int step = 0; step < 16; ++step) {
    const float velocity = 0.10f + 0.30f * (static_cast<float>(step) / 15.0f);
    pattern.set(48 + step, 38, velocity);
  }
  return pattern;
}

// Breakdown arpeggio: 16th plucks from the chord tones of each
// bar, shape {0,1,2,3,3,2,1,0} repeated twice per bar.
edm::Pattern arp_pattern(int bars) {
  edm::Pattern pattern(bars * 16);
  for (int bar = 0; bar < bars; ++bar) {
    for (int step = 0; step < 16; ++step) {
      pattern.set(bar * 16 + step, kTones[bar % 4][kArpShape[step % 8]], 1.0f);
    }
  }
  return pattern;
}

edm::NoiseVoiceConfig hats_config(float volume) {
  edm::NoiseVoiceConfig config;
  config.highpass_cutoff_hz = 6000.0;
  config.decay_seconds = 0.018;
  config.seconds = 0.045;
  config.volume = volume;
  return config;
}

void add_kick(edm::Sequencer& seq) {
  seq.add_track(std::make_unique<edm::KickVoice>(edm::KickVoiceConfig{}, kSampleRate),
                kick_pattern());
}

void add_hats(edm::Sequencer& seq, float volume) {
  seq.add_track(std::make_unique<edm::NoiseVoice>(hats_config(volume), kSampleRate),
                hats_pattern());
}

// Track order: kick is ALWAYS track 0 (the sidechain references
// it), bass track 1 where present.

edm::Section build_intro() {
  edm::Sequencer seq(kBpm, kSampleRate);
  add_kick(seq);
  seq.add_track(edm::make_synth_voice(bass_config(700.0, 0.35), kSampleRate),
                bass_pattern(4, 2));  // bass enters at bar 2
  add_hats(seq, 0.14f);
  edm::Section section("intro", 4, std::move(seq));
  section.enable_sidechain(0);
  return section;
}

edm::Section build_build() {
  edm::Sequencer seq(kBpm, kSampleRate);
  add_kick(seq);
  seq.add_track(edm::make_synth_voice(bass_config(500.0, 0.55), kSampleRate),
                bass_pattern(4, 0));
  add_hats(seq, 0.14f);
  edm::RiserVoiceConfig riser;
  seq.add_track(std::make_unique<edm::RiserVoice>(riser, kSampleRate), riser_pattern());
  edm::NoiseVoiceConfig snare;
  snare.highpass_cutoff_hz = 1800.0;
  snare.decay_seconds = 0.03;
  snare.seconds = 0.06;
  snare.volume = 1.0f;  // the ramp is the pattern's velocity
  seq.add_track(std::make_unique<edm::NoiseVoice>(snare, kSampleRate),
                snare_roll_pattern());
  edm::Section section("build", 4, std::move(seq));
  // The build sweep: bass cutoff 500 -> 6000 Hz, exponential,
  // across the 4 bars.
  if (!section.bind_filter_cutoff(
          1, edm::Automation({{0.0, 500.0}, {4.0, 6000.0}}, edm::Curve::Exponential))) {
    std::fprintf(stderr, "arrangement_demo: cutoff bind failed (build)\n");
    std::exit(1);
  }
  section.enable_sidechain(0);
  return section;
}

edm::Section build_drop() {
  edm::Sequencer seq(kBpm, kSampleRate);
  add_kick(seq);
  seq.add_track(edm::make_synth_voice(bass_config(2200.0, 0.55), kSampleRate),
                bass_pattern(8, 0));
  seq.add_track(std::make_unique<edm::SupersawVoice>(edm::SupersawVoiceConfig{},
                                                     kSampleRate),
                lead_pattern());
  add_hats(seq, 0.14f);
  edm::NoiseVoiceConfig hats16;
  hats16.highpass_cutoff_hz = 7000.0;
  hats16.decay_seconds = 0.012;
  hats16.seconds = 0.030;
  hats16.volume = 0.06f;
  seq.add_track(std::make_unique<edm::NoiseVoice>(hats16, kSampleRate), hats16_pattern());
  edm::NoiseVoiceConfig clap;
  clap.highpass_cutoff_hz = 1000.0;
  clap.decay_seconds = 0.09;
  clap.seconds = 0.22;
  clap.bursts = 3;
  clap.volume = 0.45f;
  seq.add_track(std::make_unique<edm::NoiseVoice>(clap, kSampleRate), clap_pattern());
  edm::Section section("drop", 8, std::move(seq));
  section.enable_sidechain(0);
  return section;
}

edm::Section build_breakdown() {
  edm::Sequencer seq(kBpm, kSampleRate);
  // No triangle wave in the library — sine plucks (the spike's
  // triangle is soft; sine is the closest available voice).
  edm::SynthVoiceConfig arp;
  arp.wave = edm::Wave::Sine;
  arp.attack_seconds = 0.004;
  arp.decay_seconds = 0.09;
  arp.sustain_level = 0.0;
  arp.release_seconds = 0.05;
  arp.filter_cutoff_hz = 0.0;
  arp.volume = 0.30f;
  seq.add_track(edm::make_synth_voice(arp, kSampleRate), arp_pattern(4));
  add_hats(seq, 0.07f);  // quiet hats in the breakdown (spike's 0.07)
  edm::Section section("breakdown", 4, std::move(seq));
  // NO sidechain in the breakdown (no kick to duck against).
  return section;
}

struct SectionStats {
  const char* name;
  int first_bar;  // 0-based
  int bars;
};

float section_rms(const edm::Buffer& buffer, std::size_t from, std::size_t to) {
  double sum = 0.0;
  for (std::size_t i = from; i < to; ++i) {
    sum += static_cast<double>(buffer[i]) * buffer[i];
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(to - from)));
}

float section_peak(const edm::Buffer& buffer, std::size_t from, std::size_t to) {
  float max = 0.0f;
  for (std::size_t i = from; i < to; ++i) {
    const float a = std::abs(buffer[i]);
    if (a > max) max = a;
  }
  return max;
}
} // namespace

int main(int argc, char** argv) {
  const std::string out_path = argc > 1 ? argv[1] : "unity_style.wav";

  edm::Arrangement song(kBpm, kSampleRate);
  song.add_section(build_intro());
  song.add_section(build_build());
  song.add_section(build_drop());
  song.add_section(build_breakdown());
  song.add_section(build_build());  // build2
  song.add_section(build_drop());   // drop2

  const std::size_t total = song.frames();
  const double seconds = static_cast<double>(total) / kSampleRate;
  if (std::abs(seconds - 60.0) > 0.001) return fail("wrong duration (not 60.00s)");

  // Render twice: determinism check.
  edm::Buffer first(total);
  song.render(first.data());
  edm::Buffer second(total);
  song.render(second.data());
  for (std::size_t i = 0; i < total; ++i) {
    if (first[i] != second[i]) return fail("non-deterministic render");
  }

  // Master chain (the spike's): tanh soft clip, then normalize
  // to kMixNormalizePeak.
  for (std::size_t i = 0; i < total; ++i) {
    first[i] = static_cast<float>(std::tanh(first[i]));
  }
  first.normalize(edm::kMixNormalizePeak);

  // Checks: finite samples, peak within [-1, 1], non-silent.
  float peak = 0.0f;
  for (std::size_t i = 0; i < total; ++i) {
    if (!std::isfinite(first[i])) return fail("non-finite sample");
    const float a = std::abs(first[i]);
    if (a > peak) peak = a;
  }
  if (peak > 1.0f) return fail("peak above 1.0");
  if (peak < 0.5f) return fail("render suspiciously quiet");

  if (!edm::write_wav(out_path, first.data(), kSampleRate)) {
    return fail("could not write the WAV file");
  }

  std::printf("%s: %zu samples (%.2fs), 128 BPM, D minor, %d bars\n",
              out_path.c_str(), total, seconds, kBars);
  // Per-section RMS/peak, like the spike's main().
  static const SectionStats kStats[6] = {
      {"intro ", 0, 4}, {"build ", 4, 4}, {"drop  ", 8, 8},
      {"break ", 16, 4}, {"build2", 20, 4}, {"drop2 ", 24, 8},
  };
  const std::size_t bar_frames = total / static_cast<std::size_t>(kBars);
  float rms[6] = {};
  for (int s = 0; s < 6; ++s) {
    const std::size_t from = static_cast<std::size_t>(kStats[s].first_bar) * bar_frames;
    const std::size_t to = from + static_cast<std::size_t>(kStats[s].bars) * bar_frames;
    rms[s] = section_rms(first, from, to);
    std::printf("  %s bars %2d-%2d  RMS %.3f  peak %.3f\n", kStats[s].name,
                kStats[s].first_bar + 1, kStats[s].first_bar + kStats[s].bars,
                rms[s], section_peak(first, from, to));
  }

  // Section RMS ordering: the drop is the loudest section, the
  // build rises into it, the intro sits below the build, and the
  // breakdown is clearly the quietest. The drop/build margin is
  // thin (~0.001 — the build is already dense with the riser and
  // the snare roll), but the render is deterministic, so the
  // ordering is a pinned property: a future change that flips it
  // must be a conscious decision, not an accident.
  if (!(rms[2] > rms[1])) return fail("RMS ordering: drop not above build");
  if (!(rms[1] > rms[0])) return fail("RMS ordering: build not above intro");
  if (!(rms[0] > rms[3])) return fail("RMS ordering: intro not above breakdown");
  if (!(rms[5] > rms[4])) return fail("RMS ordering: drop2 not above build2");
  return 0;
}
