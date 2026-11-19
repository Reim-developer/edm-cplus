#pragma once

// Deterministic PRNG (xorshift32). The only randomness in the library.
// Same seed -> same stream, always. The RNG is always an explicit,
// seeded object: determinism must be visible, never hidden global state.
//
// Ported from the game-audio spike (spike.cpp), which proved it with a
// byte-identical determinism check.

#include <cstdint>

namespace edm {

class Rng {
 public:
  explicit Rng(std::uint32_t seed) noexcept : state_(seed != 0 ? seed : 0x9E3779B9u) {}

  // Next raw 32-bit value.
  std::uint32_t next() noexcept {
    std::uint32_t s = state_;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    state_ = s;
    return s;
  }

  // Uniform double in [0, 1).
  double uniform() noexcept { return static_cast<double>(next() >> 8) * kUniformScale; }

  // Uniform float in [0, 1).
  float uniform_f() noexcept { return static_cast<float>(uniform()); }

  // Uniform float in [-1, 1). The noise sample.
  float bipolar() noexcept { return uniform_f() * 2.0f - 1.0f; }

 private:
  // Scale for uniform(): next() >> 8 keeps the top 24 bits, so there
  // are 2^24 = 16777216 possible values — multiply by 1/2^24 to land
  // in [0, 1).
  static constexpr double kUniformScale = 1.0 / 16777216.0;

  std::uint32_t state_;
};

} // namespace edm
