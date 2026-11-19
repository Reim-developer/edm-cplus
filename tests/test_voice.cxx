// Voice: midi_to_freq known values.

#include <gtest/gtest.h>

#include "edm/voice.hxx"

TEST(Voice, MidiToFreq) {
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(69), 440.0);
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(81), 880.0);
  EXPECT_DOUBLE_EQ(edm::midi_to_freq(57), 220.0);
  EXPECT_NEAR(edm::midi_to_freq(60), 261.6255653005986, 0.001);
  EXPECT_NEAR(edm::midi_to_freq(0), 8.175798915643707, 0.001);
}
