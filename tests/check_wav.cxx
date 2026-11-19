// WAV validity check for the tone example (CTest wav_validity test).
//
// Usage: check_wav <wav_path> <expected_rate> <expected_seconds>
//
// Reads the WAV with std::ifstream and checks it is a valid 16-bit
// mono WAV at the expected rate and duration, and non-silent.
// Exits 0 on success, 1 with a message on any failure.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::uint32_t u32_at(const std::vector<unsigned char>& d, std::size_t off) {
  return static_cast<std::uint32_t>(d[off]) | (static_cast<std::uint32_t>(d[off + 1]) << 8) |
         (static_cast<std::uint32_t>(d[off + 2]) << 16) |
         (static_cast<std::uint32_t>(d[off + 3]) << 24);
}

std::uint16_t u16_at(const std::vector<unsigned char>& d, std::size_t off) {
  return static_cast<std::uint16_t>(d[off] | (d[off + 1] << 8));
}

int fail(const char* message) {
  std::cerr << "check_wav: " << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 4) return fail("usage: check_wav <wav_path> <expected_rate> <expected_seconds>");
  const char* path = argv[1];
  const int expected_rate = std::atoi(argv[2]);
  const double expected_seconds = std::atof(argv[3]);
  if (expected_rate <= 0 || expected_seconds <= 0.0) return fail("bad expected rate/seconds");

  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return fail("cannot open file");
  const std::streampos end = in.tellg();
  if (end < 44) return fail("file too short for a WAV header");
  std::vector<unsigned char> d(static_cast<std::size_t>(end));
  in.seekg(0, std::ios::beg);
  in.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(d.size()));
  if (!in) return fail("cannot read file");

  if (std::memcmp(d.data(), "RIFF", 4) != 0) return fail("missing RIFF marker");
  if (std::memcmp(d.data() + 8, "WAVE", 4) != 0) return fail("missing WAVE marker");
  if (std::memcmp(d.data() + 12, "fmt ", 4) != 0) return fail("missing fmt marker");
  if (u16_at(d, 20) != 1) return fail("not PCM format");
  if (u16_at(d, 22) != 1) return fail("not mono");
  if (u16_at(d, 34) != 16) return fail("not 16-bit");
  if (u32_at(d, 24) != static_cast<std::uint32_t>(expected_rate)) return fail("wrong sample rate");
  if (std::memcmp(d.data() + 36, "data", 4) != 0) return fail("missing data marker");
  const std::uint32_t data_bytes = u32_at(d, 40);
  if (44 + data_bytes != d.size()) return fail("data size does not match file size");

  const double duration = static_cast<double>(data_bytes / 2) / static_cast<double>(expected_rate);
  if (std::abs(duration - expected_seconds) > 0.01) return fail("wrong duration");

  std::int32_t peak = 0;
  for (std::size_t i = 44; i + 1 < d.size(); i += 2) {
    const std::int16_t v = static_cast<std::int16_t>(u16_at(d, i));
    const std::int32_t a = v < 0 ? -static_cast<std::int32_t>(v) : v;
    if (a > peak) peak = a;
  }
  if (peak < 1000) return fail("output is silent");

  std::cout << "OK: " << path << " (" << duration << "s, " << expected_rate
            << " Hz, mono, 16-bit, peak " << peak << ")\n";
  return 0;
}
