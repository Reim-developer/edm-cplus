// tone: the first thing edm-cplus renders. A 1-second sine sweeping
// 220 -> 880 Hz through an AD envelope and a lowpass, written to a WAV.
// Usage: tone [output.wav]  (default: tone.wav)

#include <cstdio>

#include "edm/buffer.hxx"
#include "edm/envelope.hxx"
#include "edm/filter.hxx"
#include "edm/oscillator.hxx"
#include "edm/rng.hxx"
#include "edm/version.hxx"
#include "edm/wav.hxx"

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "tone.wav";
  constexpr int kSampleRate = 44100;
  constexpr double kSeconds = 1.0;

  edm::Buffer buffer(static_cast<std::size_t>(kSeconds * kSampleRate));
  edm::Oscillator<edm::Wave::Sine> osc;
  edm::Envelope env;
  edm::Biquad lowpass;
  edm::Rng rng(42); // unused by the sine wave; the API is uniform

  lowpass.set_lowpass(2000.0, 0.7, kSampleRate);
  env.trigger_ad(0.05, 0.9, kSampleRate);
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    const double frac = static_cast<double>(i) / static_cast<double>(buffer.size());
    osc.set_frequency(220.0 + 660.0 * frac, kSampleRate);
    buffer[i] = lowpass.process(osc.next(rng) * env.next());
  }

  if (!edm::write_wav(path, buffer.data(), kSampleRate)) {
    std::fprintf(stderr, "failed to write %s\n", path);
    return 1;
  }
  std::printf("edm-cplus %s: wrote %s (%.2fs, %d Hz, mono, 16-bit)\n", edm::kVersionString, path,
              kSeconds, kSampleRate);
  return 0;
}
