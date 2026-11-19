#pragma once

// Filters. Biquad is an RBJ lowpass with settable cutoff/Q; coefficients
// may be recomputed per sample, which is how filter sweeps work (the
// build-up sweep in an EDM track is a lowpass opening over time).
// Highpass is a one-pole highpass for hats and transient body.
// State is double precision; process() is float in / float out.
//
// Ported from the EDM spike (edm.cpp), which proved both filters in a
// full 60-second render.

#include <cstddef>
#include <span>

namespace edm {

class Biquad {
 public:
  Biquad() = default;

  // Cutoff is clamped to [1 Hz, 0.45 * sample_rate], Q to [0.1, 100],
  // so degenerate settings cannot produce NaN. Default state is a
  // passthrough (unity) until set_lowpass is called.
  void set_lowpass(double cutoff_hz, double q, double sample_rate) noexcept;
  void reset() noexcept;

  float process(float x) noexcept;
  // Block form: identical output to calling process() sample by sample.
  // in.size() must equal out.size().
  void process(std::span<const float> in, std::span<float> out) noexcept;

 private:
  double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
  double x1_ = 0.0, x2_ = 0.0, y1_ = 0.0, y2_ = 0.0;
};

class Highpass {
 public:
  // Default: 1000 Hz at 44.1 kHz, so a default-constructed filter is usable.
  Highpass();

  void set(double cutoff_hz, double sample_rate) noexcept;
  void reset() noexcept;

  float process(float x) noexcept;
  void process(std::span<const float> in, std::span<float> out) noexcept;

 private:
  double a_ = 1.0;
  double y_ = 0.0;
  double x_prev_ = 0.0;
};

} // namespace edm
