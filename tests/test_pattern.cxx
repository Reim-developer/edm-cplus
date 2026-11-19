// Pattern: set/clear/at, out-of-range safety, size, default values.

#include <gtest/gtest.h>

#include "edm/pattern.hxx"

TEST(Pattern, Size) {
  EXPECT_EQ(edm::Pattern(16).size(), 16);
  EXPECT_EQ(edm::Pattern(1).size(), 1);
  EXPECT_EQ(edm::Pattern(0).size(), 0);
  EXPECT_EQ(edm::Pattern(-5).size(), 0);
}

TEST(Pattern, DefaultStepValues) {
  const edm::Pattern pattern(16);
  for (int i = 0; i < pattern.size(); ++i) {
    EXPECT_FALSE(pattern.at(i).note.has_value());
    EXPECT_FLOAT_EQ(pattern.at(i).velocity, 0.8f);
    EXPECT_EQ(pattern.at(i).length_steps, 1);
  }
}

TEST(Pattern, SetAndClear) {
  edm::Pattern pattern(16);
  pattern.set(3, 60, 0.9f, 2);
  ASSERT_TRUE(pattern.at(3).note.has_value());
  EXPECT_EQ(pattern.at(3).note.value(), 60);
  EXPECT_FLOAT_EQ(pattern.at(3).velocity, 0.9f);
  EXPECT_EQ(pattern.at(3).length_steps, 2);
  // Other steps untouched.
  EXPECT_FALSE(pattern.at(2).note.has_value());
  EXPECT_FALSE(pattern.at(4).note.has_value());
  pattern.clear(3);
  EXPECT_FALSE(pattern.at(3).note.has_value());
  EXPECT_FLOAT_EQ(pattern.at(3).velocity, 0.8f);
  EXPECT_EQ(pattern.at(3).length_steps, 1);
}

TEST(Pattern, SetDefaults) {
  edm::Pattern pattern(16);
  pattern.set(0, 69);
  ASSERT_TRUE(pattern.at(0).note.has_value());
  EXPECT_EQ(pattern.at(0).note.value(), 69);
  EXPECT_FLOAT_EQ(pattern.at(0).velocity, 0.8f);
  EXPECT_EQ(pattern.at(0).length_steps, 1);
}

TEST(Pattern, LengthClampedToAtLeastOne) {
  edm::Pattern pattern(16);
  pattern.set(0, 60, 0.8f, 0);
  EXPECT_EQ(pattern.at(0).length_steps, 1);
  pattern.set(1, 60, 0.8f, -3);
  EXPECT_EQ(pattern.at(1).length_steps, 1);
}

TEST(Pattern, OutOfRangeIsSafe) {
  edm::Pattern pattern(16);
  // No crash, no effect.
  pattern.set(-1, 60);
  pattern.set(16, 60);
  pattern.set(100, 60);
  pattern.clear(-1);
  pattern.clear(16);
  EXPECT_EQ(pattern.size(), 16);
  EXPECT_FALSE(pattern.at(0).note.has_value());
  // Out-of-range reads return an empty step.
  EXPECT_FALSE(pattern.at(-1).note.has_value());
  EXPECT_FALSE(pattern.at(16).note.has_value());
  EXPECT_FALSE(pattern.at(100).note.has_value());
  // An empty pattern is safe too.
  const edm::Pattern empty(0);
  EXPECT_FALSE(empty.at(0).note.has_value());
}
