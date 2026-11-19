// Rng determinism: two RNGs with the same seed produce identical streams.

#include <cstdint>
#include <gtest/gtest.h>

#include "edm/rng.hxx"

TEST(Rng, SameSeedIdenticalRawStream) {
  edm::Rng a(42);
  edm::Rng b(42);
  bool same = true;
  for (int i = 0; i < 1000; ++i) {
    if (a.next() != b.next()) same = false;
  }
  EXPECT_TRUE(same);
}

TEST(Rng, SameSeedIdenticalUniformStream) {
  edm::Rng a(1234);
  edm::Rng b(1234);
  bool same = true;
  for (int i = 0; i < 1000; ++i) {
    if (a.uniform() != b.uniform()) same = false;
  }
  EXPECT_TRUE(same);
}

TEST(Rng, DifferentSeedsDiffer) {
  edm::Rng a(1);
  edm::Rng b(2);
  bool differ = false;
  for (int i = 0; i < 100; ++i) {
    if (a.next() != b.next()) differ = true;
  }
  EXPECT_TRUE(differ);
}

TEST(Rng, UniformInRange) {
  edm::Rng rng(7);
  bool in_range = true;
  for (int i = 0; i < 10000; ++i) {
    const double u = rng.uniform();
    if (u < 0.0 || u >= 1.0) in_range = false;
  }
  EXPECT_TRUE(in_range);
}

TEST(Rng, BipolarInRange) {
  edm::Rng rng(7);
  bool in_range = true;
  for (int i = 0; i < 10000; ++i) {
    const float v = rng.bipolar();
    if (v < -1.0f || v >= 1.0f) in_range = false;
  }
  EXPECT_TRUE(in_range);
}

// Seed 0 must not lock the generator up (xorshift with state 0 is stuck).
TEST(Rng, SeedZeroVaries) {
  edm::Rng rng(0);
  bool varies = false;
  const std::uint32_t first = rng.next();
  for (int i = 0; i < 100; ++i) {
    if (rng.next() != first) varies = true;
  }
  EXPECT_TRUE(varies);
}
