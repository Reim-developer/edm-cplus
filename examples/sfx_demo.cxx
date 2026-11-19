// sfx_demo: renders the 4 SFX presets + 1 randomized effect to WAVs.
// Usage: sfx_demo [output_dir]  (default: current directory)

#include <cstdio>
#include <string>

#include "edm/buffer.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"
#include "edm/version.hxx"
#include "edm/wav.hxx"

namespace {
bool render_one(const char* name, const edm::SfxParams& params, const std::string& dir,
                int sample_rate) {
  edm::Buffer buffer(edm::sfx_frames(params, sample_rate));
  edm::Rng rng(1234); // the spike's seed, fresh per effect
  edm::render_sfx(params, rng, buffer.data(), sample_rate);
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
  ok = render_one("jump", edm::sfx::jump(), dir, kSampleRate) && ok;
  ok = render_one("coin", edm::sfx::coin(), dir, kSampleRate) && ok;
  ok = render_one("laser", edm::sfx::laser(), dir, kSampleRate) && ok;
  ok = render_one("explosion", edm::sfx::explosion(), dir, kSampleRate) && ok;
  edm::Rng rng(7);
  const edm::SfxParams random = edm::randomize_sfx(rng);
  ok = render_one("random", random, dir, kSampleRate) && ok;

  if (!ok) return 1;
  std::printf("edm-cplus %s: wrote 5 SFX WAVs to %s/\n", edm::kVersionString, dir.c_str());
  return 0;
}
