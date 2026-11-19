// Envelope shape: AD rises then falls to 0; ADSR rises, decays to
// sustain, holds, and releases to ~0. Sample rate 1000 Hz keeps the
// stage lengths in exact sample counts.

#include "check.hxx"
#include "edm/envelope.hxx"

int main() {
  test::Context ctx;

  // AD: attack 10 samples, decay 50 samples.
  {
    edm::Envelope env;
    env.trigger_ad(0.01, 0.05, 1000.0);
    ctx.check(env.active(), "AD active after trigger");
    float after_5 = 0.0f;
    float after_10 = 0.0f;
    float after_35 = 0.0f;
    float after_60 = 0.0f;
    for (int i = 1; i <= 60; ++i) {
      const float v = env.next();
      if (i == 5) after_5 = v;
      if (i == 10) after_10 = v;
      if (i == 35) after_35 = v;
      if (i == 60) after_60 = v;
    }
    ctx.check_near(after_5, 0.5, 1e-6, "AD attack halfway -> 0.5");
    ctx.check_near(after_10, 1.0, 1e-6, "AD attack end -> 1.0");
    ctx.check_near(after_35, 0.5, 1e-6, "AD decay halfway -> 0.5");
    ctx.check_near(after_60, 0.0, 1e-6, "AD decay end -> 0.0");
    ctx.check(!env.active(), "AD idle after decay");
    ctx.check_near(env.next(), 0.0, 1e-6, "AD stays at 0 once idle");
  }

  // ADSR: attack 10, decay 10, sustain 0.5, release 10.
  {
    edm::Envelope env;
    env.set_adsr(0.01, 0.01, 0.5, 0.01, 1000.0);
    env.note_on();
    float after_10 = 0.0f;
    float after_20 = 0.0f;
    for (int i = 1; i <= 20; ++i) {
      const float v = env.next();
      if (i == 10) after_10 = v;
      if (i == 20) after_20 = v;
    }
    ctx.check_near(after_10, 1.0, 1e-6, "ADSR attack end -> 1.0");
    ctx.check_near(after_20, 0.5, 1e-6, "ADSR decay end -> sustain 0.5");
    // Sustain holds.
    bool holds = true;
    for (int i = 0; i < 100; ++i) {
      if (env.next() != 0.5f) holds = false;
    }
    ctx.check(holds, "ADSR sustain holds at 0.5");
    ctx.check(env.active(), "ADSR active while sustaining");
    // Release reaches ~0 in 10 samples, then idle.
    float after_release = 0.0f;
    env.note_off();
    for (int i = 0; i < 10; ++i) after_release = env.next();
    ctx.check_near(after_release, 0.0, 1e-6, "ADSR release end -> 0.0");
    ctx.check(!env.active(), "ADSR idle after release");
  }

  // Zero-length stages complete immediately, never hang.
  {
    edm::Envelope env;
    env.trigger_ad(0.0, 0.0, 1000.0);
    const float first = env.next();
    ctx.check_near(first, 1.0, 1e-6, "zero attack -> peak on first sample");
    const float second = env.next();
    ctx.check_near(second, 0.0, 1e-6, "zero decay -> 0 on second sample");
    ctx.check(!env.active(), "zero-length AD goes idle");
  }

  // note_off before note_on is a no-op; retrigger restarts from 0.
  {
    edm::Envelope env;
    env.set_adsr(0.01, 0.01, 0.5, 0.01, 1000.0);
    env.note_off();
    ctx.check(!env.active(), "note_off while idle is a no-op");
    env.note_on();
    (void)env.next();
    env.note_on();
    ctx.check_near(env.level(), 0.0, 1e-6, "retrigger restarts from 0");
  }

  return ctx.finish("envelope");
}
