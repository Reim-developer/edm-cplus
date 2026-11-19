// sfx_demo: renders 4 SFX presets + 1 randomized effect to WAVs.
// The presets are LOCAL functions — the library does not name sounds
// (0.0.4 removed edm::sfx::*). Same proven values as the old presets,
// rendered through the generic path via SlideSfx.
// Usage: sfx_demo [output_dir]  (default: current directory)

#include <cstdio>
#include <string>

#include "edm/buffer.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"
#include "edm/version.hxx"
#include "edm/wav.hxx"

namespace {
// Local presets — sound identities are user content, not library
// knowledge.
edm::SfxParams jump() {
  return edm::SfxParams{.wave = edm::Wave::Square,
                        .f0_hz = 300.0,
                        .f1_hz = 900.0,
                        .seconds = 0.25,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.08};
}

edm::SfxParams coin() {
  return edm::SfxParams{.wave = edm::Wave::Sine,
                        .f0_hz = 988.0,
                        .f1_hz = 1319.0,
                        .seconds = 0.30,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.10};
}

edm::SfxParams laser() {
  return edm::SfxParams{.wave = edm::Wave::Saw,
                        .f0_hz = 1200.0,
                        .f1_hz = 100.0,
                        .seconds = 0.30,
                        .volume = 0.5,
                        .attack_seconds = 0.01,
                        .decay_seconds = 0.12};
}

edm::SfxParams explosion() {
  return edm::SfxParams{.wave = edm::Wave::Noise,
                        .f0_hz = 800.0,
                        .f1_hz = 60.0,
                        .seconds = 0.60,
                        .volume = 0.7,
                        .attack_seconds = 0.005,
                        .decay_seconds = 0.45};
}

bool render_one(const char* name, const edm::SfxParams& params, const std::string& dir,
                int sample_rate) {
  edm::Buffer buffer(edm::sfx_frames(params, sample_rate));
  edm::SlideSfx gen(params, edm::Rng(1234), sample_rate); // the spike's seed, fresh per effect
  edm::render_sfx(gen, buffer.data());
  const std::string path = dir + "/sfx_" + name + ".wav";
  if (!edm::write_wav(path, buffer.data(), sample_rate)) {
    std::fprintf(stderr, "failed to write %s\n", path.c_str());
    return false;
  }
  std::printf("sfx %-10s %zu frames (%.2fs)\n", name, buffer.size(),
              static_cast<double>(buffer.size()) / sample_rate);
  return true;
}
} // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  constexpr int kSampleRate = 44100;

  bool ok = true;
  ok = render_one("jump", jump(), dir, kSampleRate) && ok;
  ok = render_one("coin", coin(), dir, kSampleRate) && ok;
  ok = render_one("laser", laser(), dir, kSampleRate) && ok;
  ok = render_one("explosion", explosion(), dir, kSampleRate) && ok;
  edm::Rng rng(7);
  const edm::SfxParams random = edm::randomize_sfx(rng);
  ok = render_one("random", random, dir, kSampleRate) && ok;

  if (!ok) return 1;
  std::printf("edm-cplus %s: wrote 5 SFX WAVs to %s/\n", edm::kVersionString, dir.c_str());
  return 0;
}
