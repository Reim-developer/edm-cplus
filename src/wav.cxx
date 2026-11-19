// WAV writer implementation. See wav.hxx.

#include "edm/wav.hxx"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace edm {

namespace {
// FILE handle owned by unique_ptr: fclose runs on every return path.
// (A named deleter struct: decltype(&std::fclose) trips
// -Wignored-attributes under -Werror.)
struct FileDeleter {
  void operator()(std::FILE* f) const noexcept {
    if (f != nullptr) std::fclose(f);
  }
};
} // namespace

bool write_wav(const std::string& path, std::span<const float> samples, int sample_rate) {
  if (sample_rate <= 0) return false;
  std::unique_ptr<std::FILE, FileDeleter> file(std::fopen(path.c_str(), "wb"));
  if (file == nullptr) return false;

  auto put = [&file](const void* p, std::size_t n) {
    return std::fwrite(p, 1, n, file.get()) == n;
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
