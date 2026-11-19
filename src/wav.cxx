// WAV writer implementation. See wav.hxx.
//
// Uses std::ofstream, not std::fopen: MSVC deprecates fopen (C4996),
// which is an error under /WX. ofstream is RAII by construction —
// the file closes on every return path, no deleter needed.

#include "edm/wav.hxx"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace edm {

bool write_wav(const std::string& path, std::span<const float> samples, int sample_rate) {
  if (sample_rate <= 0) return false;
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;

  auto put = [&out](const void* p, std::size_t n) {
    out.write(static_cast<const char*>(p), static_cast<std::streamsize>(n));
    return static_cast<bool>(out);
  };
  auto put32 = [&put](std::uint32_t v) { return put(&v, 4); };
  auto put16 = [&put](std::uint16_t v) { return put(&v, 2); };

  const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * 2);
  const std::uint32_t rate = static_cast<std::uint32_t>(sample_rate);

  bool ok = true;
  ok = ok && put("RIFF", 4);
  ok = ok && put32(36 + data_bytes);
  ok = ok && put("WAVE", 4);
  ok = ok && put("fmt ", 4);
  ok = ok && put32(16);      // PCM format chunk size
  ok = ok && put16(1);       // PCM
  ok = ok && put16(1);       // mono
  ok = ok && put32(rate);    // sample rate
  ok = ok && put32(rate * 2); // byte rate
  ok = ok && put16(2);       // block align
  ok = ok && put16(16);      // bits per sample
  ok = ok && put("data", 4);
  ok = ok && put32(data_bytes);
  for (float s : samples) {
    const float clamped = std::clamp(s, -1.0f, 1.0f);
    const std::int16_t v = static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
    ok = ok && put16(static_cast<std::uint16_t>(v));
    if (!ok) break;
  }
  return ok;
}

} // namespace edm
