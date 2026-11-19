// sfx_demo: renders 4 SFX presets + 1 randomized effect to WAVs.
// The presets are LOCAL functions — the library does not name sounds
// (0.0.4 removed edm::sfx::*). Same proven values as the old presets.
// Most effects render through the params-based render_sfx overload
// (runtime params, dispatched once per call); laser renders through
// a directly-constructed plugins::Slide<Wave::Saw>, showing both
// paths to the same output.
// Usage: sfx_demo [output_dir]  (default: current directory)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "edm/buffer.hxx"
#include "edm/plugins.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"
#include "edm/version.hxx"
#include "edm/wav.hxx"

namespace {
constexpr double kPi = 3.14159265358979323846;

// A custom generator, defined HERE in user code — no library code
// knows what a "blip" is. A decaying sine with its own parameters.
struct Blip {
  double phase = 0.0;
  double increment;
  double decay;
  float level = 1.0f;

  Blip(double hz, double seconds, double sample_rate)
      : increment(hz / sample_rate), decay(1.0 / (seconds * sample_rate)) {}

  float next() noexcept {
    if (level <= 0.0f) return 0.0f;
    const float v = static_cast<float>(std::sin(2.0 * kPi * phase)) * level;
    phase += increment;
    if (phase >= 1.0) phase -= 1.0;
    level = std::max(0.0f, level - static_cast<float>(decay));
    return v;
  }
};

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

bool write_buffer(const char* name, const edm::Buffer& buffer, const std::string& dir,
                  int sample_rate) {
  const std::string path = dir + "/sfx_" + name + ".wav";
  if (!edm::write_wav(path, buffer.data(), sample_rate)) {
    std::fprintf(stderr, "failed to write %s\n", path.c_str());
    return false;
  }
  std::printf("sfx %-10s %zu frames (%.2fs)\n", name, buffer.size(),
              static_cast<double>(buffer.size()) / sample_rate);
  return true;
}

// Runtime-params path: the convenience overload dispatches once on
// params.wave to the matching Slide<W>.
bool render_one(const char* name, const edm::SfxParams& params, const std::string& dir,
                int sample_rate) {
  edm::Buffer buffer(edm::sfx_frames(params, sample_rate));
  edm::render_sfx(params, edm::Rng(1234), buffer.data(), sample_rate); // the spike's seed
  return write_buffer(name, buffer, dir, sample_rate);
}

// Compile-time path: a directly-constructed Slide<W>, rendered
// through the generic path. Same output as the overload (W wins;
// params.wave is ignored).
template <edm::Wave W>
bool render_direct(const char* name, const edm::SfxParams& params, const std::string& dir,
                   int sample_rate) {
  edm::Buffer buffer(edm::sfx_frames(params, sample_rate));
  edm::plugins::Slide<W> gen(params, edm::Rng(1234), sample_rate); // the spike's seed
  edm::render_sfx(gen, buffer.data());
  return write_buffer(name, buffer, dir, sample_rate);
}
} // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  constexpr int kSampleRate = 44100;

  bool ok = true;
  ok = render_one("jump", jump(), dir, kSampleRate) && ok;
  ok = render_one("coin", coin(), dir, kSampleRate) && ok;
  ok = render_direct<edm::Wave::Saw>("laser", laser(), dir, kSampleRate) && ok;
  ok = render_one("explosion", explosion(), dir, kSampleRate) && ok;
  edm::Rng rng(7);
  const edm::SfxParams random = edm::randomize_sfx(rng);
  ok = render_one("random", random, dir, kSampleRate) && ok;

  // Demonstrate the plugin pattern: a generator defined in this file,
  // rendered through the generic path. The demo fails if it renders
  // silence — the pattern is load-bearing, not decorative.
  edm::Buffer custom(4410);
  Blip blip(660.0, 0.1, kSampleRate);
  edm::render_sfx(blip, custom.data());
  float custom_peak = 0.0f;
  for (const float s : custom.data()) {
    custom_peak = std::max(custom_peak, std::abs(s));
  }
  std::printf("custom blip   %zu frames (peak %.2f) — user-defined generator\n",
              custom.size(), static_cast<double>(custom_peak));
  if (custom_peak <= 0.1f) {
    std::fprintf(stderr, "custom generator rendered silence\n");
    ok = false;
  }

  if (!ok) return 1;
  std::printf("edm-cplus %s: wrote 5 SFX WAVs to %s/\n", edm::kVersionString, dir.c_str());
  return 0;
}
