// Arrangement implementation. See arrangement.hxx.

#include "edm/arrangement.hxx"

#include <utility>

#include "edm/buffer.hxx"
#include "edm/wav.hxx"

namespace edm {

Arrangement::Arrangement(double bpm, double sample_rate)
    : bpm_(bpm), sample_rate_(sample_rate) {}

void Arrangement::add_section(Section section) {
  sections_.push_back(std::move(section));
}

std::size_t Arrangement::frames() const noexcept {
  std::size_t total = 0;
  for (const auto& section : sections_) total += section.frames();
  return total;
}

void Arrangement::render(std::span<float> out) {
  std::size_t offset = 0;
  for (auto& section : sections_) {
    if (offset >= out.size()) break;
    const std::size_t n = section.frames();
    const std::size_t avail = out.size() - offset;
    // An exact-size subspan: Section::render silences a longer
    // span's tail, which would zero the next section's region.
    section.render(out.subspan(offset, n < avail ? n : avail));
    offset += n;
    if (offset > out.size()) offset = out.size();
  }
  for (std::size_t i = offset; i < out.size(); ++i) out[i] = 0.0f;
}

bool Arrangement::render_to_wav(const std::string& path) {
  Buffer buffer(frames());
  render(buffer.data());
  return write_wav(path, buffer.data(), static_cast<int>(sample_rate_));
}

} // namespace edm
