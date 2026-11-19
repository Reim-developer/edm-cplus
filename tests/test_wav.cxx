// WAV writer: header layout, sample quantization, failure paths.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include "check.hxx"
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

int main() {
  test::Context ctx;

  // Write 4 known samples, read the header back field by field.
  {
    const std::vector<float> samples{0.0f, 0.5f, -0.5f, 1.0f};
    ctx.check(edm::write_wav("test_wav_out.wav", samples, 44100), "write_wav returns true");
    const std::vector<unsigned char> d = read_file("test_wav_out.wav");
    ctx.check(d.size() == 44 + 8, "file size is header + data");
    if (d.size() >= 44) {
      ctx.check(std::memcmp(d.data(), "RIFF", 4) == 0, "RIFF marker");
      ctx.check(u32_at(d, 4) == 36 + 8, "RIFF chunk size");
      ctx.check(std::memcmp(d.data() + 8, "WAVE", 4) == 0, "WAVE marker");
      ctx.check(std::memcmp(d.data() + 12, "fmt ", 4) == 0, "fmt marker");
      ctx.check(u32_at(d, 16) == 16, "fmt chunk size 16");
      ctx.check(u16_at(d, 20) == 1, "PCM format");
      ctx.check(u16_at(d, 22) == 1, "mono");
      ctx.check(u32_at(d, 24) == 44100, "sample rate 44100");
      ctx.check(u32_at(d, 28) == 44100 * 2, "byte rate");
      ctx.check(u16_at(d, 32) == 2, "block align 2");
      ctx.check(u16_at(d, 34) == 16, "16 bits per sample");
      ctx.check(std::memcmp(d.data() + 36, "data", 4) == 0, "data marker");
      ctx.check(u32_at(d, 40) == 8, "data chunk size");
      // Sample quantization: 0.0 -> 0, 0.5 -> ~16384, -0.5 -> ~-16384, 1.0 -> 32767.
      const auto sample_at = [&d](std::size_t i) {
        return static_cast<std::int16_t>(u16_at(d, 44 + i * 2));
      };
      ctx.check(sample_at(0) == 0, "sample 0.0 -> 0");
      ctx.check(std::abs(sample_at(1) - 16384) <= 1, "sample 0.5 -> ~16384");
      ctx.check(std::abs(sample_at(2) - (-16384)) <= 1, "sample -0.5 -> ~-16384");
      ctx.check(sample_at(3) == 32767, "sample 1.0 -> 32767");
    }
    std::remove("test_wav_out.wav");
  }

  // Out-of-range samples clamp instead of wrapping.
  {
    const std::vector<float> samples{2.0f, -2.0f};
    ctx.check(edm::write_wav("test_wav_clip.wav", samples, 44100), "clipping write returns true");
    const std::vector<unsigned char> d = read_file("test_wav_clip.wav");
    if (d.size() == 44 + 4) {
      ctx.check(static_cast<std::int16_t>(u16_at(d, 44)) == 32767, "2.0 clamps to 32767");
      ctx.check(static_cast<std::int16_t>(u16_at(d, 46)) == -32767, "-2.0 clamps to -32767");
    } else {
      ctx.check(false, "clipping file has expected size");
    }
    std::remove("test_wav_clip.wav");
  }

  // Empty input writes a valid 0-frame WAV.
  {
    const std::vector<float> empty;
    ctx.check(edm::write_wav("test_wav_empty.wav", empty, 44100), "empty write returns true");
    const std::vector<unsigned char> d = read_file("test_wav_empty.wav");
    ctx.check(d.size() == 44, "empty WAV is header only");
    if (d.size() == 44) ctx.check(u32_at(d, 40) == 0, "empty WAV data size 0");
    std::remove("test_wav_empty.wav");
  }

  // Failure paths return false.
  ctx.check(!edm::write_wav("no_such_dir_xyz/out.wav", std::vector<float>{0.0f}, 44100),
            "unwritable path returns false");
  ctx.check(!edm::write_wav("test_wav_bad.wav", std::vector<float>{0.0f}, 0),
            "sample rate 0 returns false");
  ctx.check(!edm::write_wav("test_wav_bad.wav", std::vector<float>{0.0f}, -44100),
            "negative sample rate returns false");

  return ctx.finish("wav");
}
