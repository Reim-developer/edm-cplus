// sequence_demo: renders a 2-bar loop to WAV — a bass line (saw,
// lowpassed) + a lead line (square), 140 BPM, A minor.
// Usage: sequence_demo [output_dir]  (default: current directory)

#include <cstdio>
#include <string>
#include <utility>

#include "edm/buffer.hxx"
#include "edm/pattern.hxx"
#include "edm/sequencer.hxx"
#include "edm/synth_voice.hxx"
#include "edm/version.hxx"
#include "edm/wav.hxx"

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  constexpr int kSampleRate = 44100;
  constexpr double kBpm = 140.0;

  edm::Sequencer seq(kBpm, kSampleRate);

  // Bass: quarter-note roots, gate 2 steps (8th notes).
  // Bar 1: A2; bar 2: F2, F2, G2, E2.
  edm::Pattern bass(32);
  bass.set(0, 45, 0.9f, 2);
  bass.set(4, 45, 0.9f, 2);
  bass.set(8, 45, 0.9f, 2);
  bass.set(12, 45, 0.9f, 2);
  bass.set(16, 41, 0.9f, 2);
  bass.set(20, 41, 0.9f, 2);
  bass.set(24, 43, 0.9f, 2);
  bass.set(28, 40, 0.9f, 2);
  edm::SynthVoiceConfig bass_config;
  bass_config.wave = edm::Wave::Saw;
  bass_config.attack_seconds = 0.005;
  bass_config.decay_seconds = 0.05;
  bass_config.sustain_level = 0.8;
  bass_config.release_seconds = 0.05;
  bass_config.filter_cutoff_hz = 800.0;
  bass_config.volume = 0.6f;
  seq.add_track(edm::make_synth_voice(bass_config, kSampleRate), std::move(bass));

  // Lead: 8th-note A-minor line, one note per 2 steps.
  const int lead_notes[] = {69, 72, 76, 72, 74, 76, 79, 76,
                            77, 76, 74, 72, 74, 72, 69, 67};
  edm::Pattern lead(32);
  for (int i = 0; i < 16; ++i) lead.set(2 * i, lead_notes[i]);
  edm::SynthVoiceConfig lead_config;
  lead_config.wave = edm::Wave::Square;
  lead_config.attack_seconds = 0.005;
  lead_config.decay_seconds = 0.08;
  lead_config.sustain_level = 0.6;
  lead_config.release_seconds = 0.08;
  lead_config.volume = 0.35f;
  seq.add_track(edm::make_synth_voice(lead_config, kSampleRate), std::move(lead));

  edm::Buffer out(seq.frames_for_bars(2));
  seq.render(out.data());

  const std::string path = dir + "/sequence_demo.wav";
  if (!edm::write_wav(path, out.data(), kSampleRate)) {
    std::fprintf(stderr, "failed to write %s\n", path.c_str());
    return 1;
  }
  std::printf("edm-cplus %s: wrote %s (%zu frames, %.2fs, 140 BPM, 2 bars)\n",
              edm::kVersionString, path.c_str(), out.size(),
              static_cast<double>(out.size()) / kSampleRate);
  return 0;
}
