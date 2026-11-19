// Oscillator known values (sample rate 4 Hz, frequency 1 Hz -> the
// phase steps through 0, 0.25, 0.5, 0.75 exactly), noise range and
// determinism, and full-render determinism: the same tone rendered
// twice produces identical samples.

#include <vector>

#include "check.hxx"
#include "edm/buffer.hxx"
#include "edm/envelope.hxx"
#include "edm/oscillator.hxx"
#include "edm/rng.hxx"

int main() {
  test::Context ctx;

  // Sine at phases 0 / 0.25 / 0.5 / 0.75 -> 0 / 1 / 0 / -1.
  {
    edm::Oscillator<edm::Wave::Sine> osc;
    osc.set_frequency(1.0, 4.0);
    ctx.check_near(osc.next(), 0.0, 1e-6, "sine phase 0 -> 0");
    ctx.check_near(osc.next(), 1.0, 1e-6, "sine phase 0.25 -> 1");
    ctx.check_near(osc.next(), 0.0, 1e-6, "sine phase 0.5 -> 0");
    ctx.check_near(osc.next(), -1.0, 1e-6, "sine phase 0.75 -> -1");
  }

  // Square sign: +0.8 in the first half, -0.8 in the second.
  {
    edm::Oscillator<edm::Wave::Square> osc;
    osc.set_frequency(1.0, 4.0);
    ctx.check_near(osc.next(), 0.8, 1e-6, "square phase 0 -> +0.8");
    ctx.check_near(osc.next(), 0.8, 1e-6, "square phase 0.25 -> +0.8");
    ctx.check_near(osc.next(), -0.8, 1e-6, "square phase 0.5 -> -0.8");
    ctx.check_near(osc.next(), -0.8, 1e-6, "square phase 0.75 -> -0.8");
  }

  // Saw ramp: -1 / -0.5 / 0 / 0.5.
  {
    edm::Oscillator<edm::Wave::Saw> osc;
    osc.set_frequency(1.0, 4.0);
    ctx.check_near(osc.next(), -1.0, 1e-6, "saw phase 0 -> -1");
    ctx.check_near(osc.next(), -0.5, 1e-6, "saw phase 0.25 -> -0.5");
    ctx.check_near(osc.next(), 0.0, 1e-6, "saw phase 0.5 -> 0");
    ctx.check_near(osc.next(), 0.5, 1e-6, "saw phase 0.75 -> 0.5");
  }

  // Noise stays in [-1, 1) and is deterministic for a given seed.
  {
    edm::Rng rng(99);
    edm::Oscillator<edm::Wave::Noise> osc;
    bool in_range = true;
    for (int i = 0; i < 10000; ++i) {
      const float v = osc.next(rng);
      if (v < -1.0f || v >= 1.0f) in_range = false;
    }
    ctx.check(in_range, "noise in [-1, 1) over 10000 samples");
  }
  {
    edm::Rng rng_a(99);
    edm::Rng rng_b(99);
    edm::Oscillator<edm::Wave::Noise> a;
    edm::Oscillator<edm::Wave::Noise> b;
    bool same = true;
    for (int i = 0; i < 1000; ++i) {
      if (a.next(rng_a) != b.next(rng_b)) same = false;
    }
    ctx.check(same, "noise deterministic for a given seed");
  }

  // render() fills the whole span; a second render continues the phase.
  {
    edm::Oscillator<edm::Wave::Sine> osc;
    osc.set_frequency(440.0, 44100.0);
    std::vector<float> buf(1024, 0.0f);
    osc.render(buf);
    bool nonzero = false;
    for (float v : buf) {
      if (v != 0.0f) nonzero = true;
    }
    ctx.check(nonzero, "render() fills the span");
    ctx.check(osc.phase() != 0.0, "render() advances the phase");
  }

  // Full-render determinism: the same tone (oscillator + envelope into
  // a buffer) rendered twice with the same seed is bit-identical.
  {
    auto render_tone = [](std::uint32_t seed) {
      edm::Buffer buffer(4410);
      edm::Oscillator<edm::Wave::Saw> osc;
      edm::Envelope env;
      edm::Rng rng(seed);
      osc.set_frequency(220.0, 44100.0);
      env.trigger_ad(0.01, 0.09, 44100.0);
      for (std::size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = osc.next(rng) * env.next();
      }
      return buffer;
    };
    const edm::Buffer a = render_tone(42);
    const edm::Buffer b = render_tone(42);
    bool same = a.size() == b.size();
    for (std::size_t i = 0; same && i < a.size(); ++i) {
      if (a[i] != b[i]) same = false;
    }
    ctx.check(same, "same tone rendered twice -> identical samples");
  }

  return ctx.finish("oscillator");
}
