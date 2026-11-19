// Buffer: normalize scales the peak; silent stays silent.

#include <cstddef>
#include <gtest/gtest.h>

#include "edm/buffer.hxx"

namespace {
float peak(const edm::Buffer& buffer) {
  float max = 0.0f;
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    const float a = buffer[i] < 0.0f ? -buffer[i] : buffer[i];
    if (a > max) max = a;
  }
  return max;
}
} // namespace

// normalize(0.89) makes the peak exactly 0.89 and preserves the
// sample ratios (a linear scale, not a reshape).
TEST(Buffer, NormalizeToPeak) {
  edm::Buffer buffer(4);
  buffer[0] = 0.5f;
  buffer[1] = -0.25f;
  buffer[2] = 0.1f;
  buffer[3] = 0.0f;
  buffer.normalize(0.89f);
  EXPECT_FLOAT_EQ(peak(buffer), 0.89f);
  EXPECT_FLOAT_EQ(buffer[0], 0.89f);
  EXPECT_FLOAT_EQ(buffer[1], -0.445f);
  EXPECT_FLOAT_EQ(buffer[2], 0.178f);
  EXPECT_FLOAT_EQ(buffer[3], 0.0f);
}

// A silent buffer stays silent: normalize must not divide by zero.
TEST(Buffer, NormalizeSilentStaysSilent) {
  edm::Buffer buffer(1024);  // all zeros
  buffer.normalize(0.89f);
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_FLOAT_EQ(buffer[i], 0.0f);
  }
}

// A non-positive peak is a no-op, and an empty buffer is safe.
TEST(Buffer, NormalizeNonPositivePeakIsNoOp) {
  edm::Buffer buffer(2);
  buffer[0] = 0.5f;
  buffer[1] = -0.5f;
  buffer.normalize(0.0f);
  EXPECT_FLOAT_EQ(buffer[0], 0.5f);
  EXPECT_FLOAT_EQ(buffer[1], -0.5f);
  buffer.normalize(-1.0f);
  EXPECT_FLOAT_EQ(buffer[0], 0.5f);
  EXPECT_FLOAT_EQ(buffer[1], -0.5f);

  edm::Buffer empty;
  empty.normalize(0.89f);  // must not crash
  EXPECT_TRUE(empty.empty());
}
