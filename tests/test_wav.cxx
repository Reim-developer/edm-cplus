// WAV writer: header layout, sample quantization, failure paths.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

#include "edm/wav.hxx"

namespace {
// Read the whole file back for inspection. Uses std::ifstream, not
// std::fopen: MSVC deprecates fopen (C4996), an error under /WX.
std::vector<unsigned char> read_file(const char* path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return {};
  const std::streampos end = in.tellg();
  if (end < 0) return {};
  std::vector<unsigned char> data(static_cast<std::size_t>(end));
  in.seekg(0, std::ios::beg);
  if (!data.empty()) {
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    data.resize(static_cast<std::size_t>(in.gcount()));
  }
  return data;
}

std::uint32_t u32_at(const std::vector<unsigned char>& d, std::size_t off) {
  return static_cast<std::uint32_t>(d[off]) | (static_cast<std::uint32_t>(d[off + 1]) << 8) |
         (static_cast<std::uint32_t>(d[off + 2]) << 16) |
         (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

std::uint16_t u16_at(const std::vector<unsigned char>& d, std::size_t off) {
  return static_cast<std::uint16_t>(d[off] | (d[off + 1] << 8));
}
} // namespace

// Write 4 known samples, read the header back field by field.
TEST(Wav, HeaderLayout) {
  const std::vector<float> samples{0.0f, 0.5f, -0.5f, 1.0f};
  EXPECT_TRUE(edm::write_wav("test_wav_out.wav", samples, 44100));
  const std::vector<unsigned char> d = read_file("test_wav_out.wav");
  ASSERT_EQ(d.size(), 44 + 8);
  EXPECT_EQ(std::memcmp(d.data(), "RIFF", 4), 0);
  EXPECT_EQ(u32_at(d, 4), 36 + 8);
  EXPECT_EQ(std::memcmp(d.data() + 8, "WAVE", 4), 0);
  EXPECT_EQ(std::memcmp(d.data() + 12, "fmt ", 4), 0);
  EXPECT_EQ(u32_at(d, 16), 16);
  EXPECT_EQ(u16_at(d, 20), 1);
  EXPECT_EQ(u16_at(d, 22), 1);
  EXPECT_EQ(u32_at(d, 24), 44100);
  EXPECT_EQ(u32_at(d, 28), 44100 * 2);
  EXPECT_EQ(u16_at(d, 32), 2);
  EXPECT_EQ(u16_at(d, 34), 16);
  EXPECT_EQ(std::memcmp(d.data() + 36, "data", 4), 0);
  EXPECT_EQ(u32_at(d, 40), 8);
  // Sample quantization: 0.0 -> 0, 0.5 -> ~16384, -0.5 -> ~-16384, 1.0 -> 32767.
  const auto sample_at = [&d](std::size_t i) {
    return static_cast<std::int16_t>(u16_at(d, 44 + i * 2));
  };
  EXPECT_EQ(sample_at(0), 0);
  EXPECT_LE(std::abs(sample_at(1) - 16384), 1);
  EXPECT_LE(std::abs(sample_at(2) - (-16384)), 1);
  EXPECT_EQ(sample_at(3), 32767);
  std::remove("test_wav_out.wav");
}

// Out-of-range samples clamp instead of wrapping.
TEST(Wav, Clamping) {
  const std::vector<float> samples{2.0f, -2.0f};
  EXPECT_TRUE(edm::write_wav("test_wav_clip.wav", samples, 44100));
  const std::vector<unsigned char> d = read_file("test_wav_clip.wav");
  ASSERT_EQ(d.size(), 44 + 4);
  EXPECT_EQ(static_cast<std::int16_t>(u16_at(d, 44)), 32767);
  EXPECT_EQ(static_cast<std::int16_t>(u16_at(d, 46)), -32767);
  std::remove("test_wav_clip.wav");
}

// Empty input writes a valid 0-frame WAV.
TEST(Wav, EmptyInput) {
  const std::vector<float> empty;
  EXPECT_TRUE(edm::write_wav("test_wav_empty.wav", empty, 44100));
  const std::vector<unsigned char> d = read_file("test_wav_empty.wav");
  ASSERT_EQ(d.size(), 44);
  EXPECT_EQ(u32_at(d, 40), 0);
  std::remove("test_wav_empty.wav");
}

// Failure paths return false.
TEST(Wav, FailurePaths) {
  EXPECT_FALSE(edm::write_wav("no_such_dir_xyz/out.wav", std::vector<float>{0.0f}, 44100));
  EXPECT_FALSE(edm::write_wav("test_wav_bad.wav", std::vector<float>{0.0f}, 0));
  EXPECT_FALSE(edm::write_wav("test_wav_bad.wav", std::vector<float>{0.0f}, -44100));
}
