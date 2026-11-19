#pragma once

// Owning PCM float buffer, std::vector-backed. Non-owning views are
// std::span<float> (see data()): render functions take a span and never
// allocate, so the DSP path stays allocation-free. resize() may
// allocate and is NOT for the DSP path — size buffers up front.

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace edm {

class Buffer {
 public:
  Buffer() = default;
  explicit Buffer(std::size_t frames) : samples_(frames, 0.0f) {}

  std::span<float> data() noexcept { return samples_; }
  std::span<const float> data() const noexcept { return samples_; }

  [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
  [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }

  // Zero-fills. May allocate; not for the DSP path.
  void resize(std::size_t frames) { samples_.assign(frames, 0.0f); }

  void clear() noexcept { std::fill(samples_.begin(), samples_.end(), 0.0f); }

  float& operator[](std::size_t i) noexcept { return samples_[i]; }
  float operator[](std::size_t i) const noexcept { return samples_[i]; }

 private:
  std::vector<float> samples_;
};

} // namespace edm
