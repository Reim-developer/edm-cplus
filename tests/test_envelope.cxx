// Envelope shape: AD rises then falls to 0; ADSR rises, decays to
// sustain, holds, and releases to ~0. Sample rate 1000 Hz keeps the
// stage lengths in exact sample counts.

#include <gtest/gtest.h>

#include "edm/envelope.hxx"

// AD: attack 10 samples, decay 50 samples.
TEST(Envelope, AdShape) {
  edm::Envelope env;
  env.trigger_ad(0.01, 0.05, 1000.0);
  EXPECT_TRUE(env.active());
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
  EXPECT_NEAR(after_5, 0.5, 1e-6);
  EXPECT_NEAR(after_10, 1.0, 1e-6);
  EXPECT_NEAR(after_35, 0.5, 1e-6);
  EXPECT_NEAR(after_60, 0.0, 1e-6);
  EXPECT_FALSE(env.active());
  EXPECT_NEAR(env.next(), 0.0, 1e-6);
}

// ADSR: attack 10, decay 10, sustain 0.5, release 10.
TEST(Envelope, AdsrShape) {
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
  EXPECT_NEAR(after_10, 1.0, 1e-6);
  EXPECT_NEAR(after_20, 0.5, 1e-6);
  // Sustain holds.
  bool holds = true;
  for (int i = 0; i < 100; ++i) {
    if (env.next() != 0.5f) holds = false;
  }
  EXPECT_TRUE(holds);
  EXPECT_TRUE(env.active());
  // Release reaches ~0 in 10 samples, then idle.
  float after_release = 0.0f;
  env.note_off();
  for (int i = 0; i < 10; ++i) after_release = env.next();
  EXPECT_NEAR(after_release, 0.0, 1e-6);
  EXPECT_FALSE(env.active());
}

// Zero-length stages complete immediately, never hang.
TEST(Envelope, ZeroLengthStages) {
  edm::Envelope env;
  env.trigger_ad(0.0, 0.0, 1000.0);
  const float first = env.next();
  EXPECT_NEAR(first, 1.0, 1e-6);
  const float second = env.next();
  EXPECT_NEAR(second, 0.0, 1e-6);
  EXPECT_FALSE(env.active());
}

// note_off before note_on is a no-op; retrigger restarts from 0.
TEST(Envelope, NoteOffRetrigger) {
  edm::Envelope env;
  env.set_adsr(0.01, 0.01, 0.5, 0.01, 1000.0);
  env.note_off();
  EXPECT_FALSE(env.active());
  env.note_on();
  (void)env.next();
  env.note_on();
  EXPECT_NEAR(env.level(), 0.0, 1e-6);
}
