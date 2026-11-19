#pragma once

// edm::plugins — the extension point for ALL user-extensible sound
// content: game SFX today, EDM voices and effects tomorrow. The
// namespace name stays generic on purpose (not `game`, not
// `sfx_plugins`).
//
// Plugin<Derived> is a CRTP base (static inheritance, NOT
// virtuals): it exists to share machinery, not to dispatch at
// runtime. Inherit it, implement `float next_impl() noexcept`,
// and the base owns the frame counter — silence after the duration
// and finished() come for free instead of being re-implemented
// per plugin. There is no `virtual` anywhere in the plugin path.
//
// Inheritance stays OPTIONAL: the SampleGenerator concept is still
// the contract. A trivial generator need not inherit anything;
// the base is a convenience for plugins that want the machinery.
//
// NO reset() on the base (deliberate): a base reset cannot know
// derived DSP state (phase, envelope) and would be a bug factory.
// Plugins are cheap value objects — fresh construction IS the reset.
//
// Templates, not runtime dispatch: Slide<W> selects its wave at
// compile time (if constexpr), and the convenience overload in
// sfx.cxx switches on the runtime wave ONCE per call. Binary size
// is not a constraint for this library (not embedded) — four
// instantiations instead of one runtime branch is the right trade.

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>

#include "edm/oscillator.hxx"
#include "edm/rng.hxx"
#include "edm/sfx.hxx"

namespace edm::plugins {

template <typename Derived>
class Plugin {
 public:
  Plugin(double sample_rate, double seconds) noexcept
      : sample_rate_(sample_rate),
        total_((seconds > 0.0 && sample_rate > 0.0)
                   ? static_cast<std::size_t>(seconds * sample_rate)
                   : 0) {}

  // 0.0f once the duration has elapsed, else Derived::next_impl().
  // The counter increments AFTER next_impl() runs, so inside
  // next_impl() frames_elapsed() is the 0-based index of the
  // sample being produced.
  float next() noexcept {
    static_assert(requires(Derived& d) {
      { d.next_impl() } noexcept -> std::convertible_to<float>;
    }, "plugins::Plugin<Derived> requires Derived to provide "
       "'float next_impl() noexcept'");
    if (finished()) return 0.0f;
    const float v = static_cast<Derived*>(this)->next_impl();
    ++frames_;
    return v;
  }

  [[nodiscard]] bool finished() const noexcept { return frames_ >= total_; }
  [[nodiscard]] std::size_t frames_elapsed() const noexcept { return frames_; }
  // Total frames in the duration (the truncated seconds*rate).
  // Plugins need the exact value — e.g. Slide's slide fraction —
  // and deriving it from elapsed time would round differently.
  [[nodiscard]] std::size_t frames_total() const noexcept { return total_; }
  [[nodiscard]] double sample_rate() const noexcept { return sample_rate_; }

 private:
  double sample_rate_;
  std::size_t total_;
  std::size_t frames_ = 0;
};

// Slide<W>: oscillator + linear frequency slide + AD envelope —
// the v0.0.2 model, now ONE built-in plugin. The wave is a
// template parameter: per-sample wave selection is compile-time
// (if constexpr), never a runtime switch per sample.
//
// The wave math is the v0.0.4 SlideSfx loop moved verbatim —
// including the advance-then-sample phase order and the double
// noise expression (rng.uniform() * 2.0 - 1.0, NOT rng.bipolar(),
// which rounds in float) — so output is bit-identical to v0.0.4.
// That is why Slide does NOT reuse Oscillator<W>: the Oscillator
// samples-then-advances and consumes noise via bipolar(), both of
// which would change the output.
//
// When constructing Slide<W> directly, W wins and params.wave is
// ignored (same rule as SynthVoice<W>). The Rng member stays in
// all instantiations (one word; used only by Noise).
template <Wave W>
class Slide : public Plugin<Slide<W>> {
 public:
  Slide(SfxParams params, Rng rng, double sample_rate) noexcept
      : Plugin<Slide<W>>(sample_rate, params.seconds), params_(params), rng_(rng) {}

  float next_impl() noexcept {
    const double sample_rate = this->sample_rate();
    const std::size_t index = this->frames_elapsed();
    const std::size_t total = this->frames_total();
    const double t = static_cast<double>(index) / sample_rate;
    const double frac = static_cast<double>(index) / static_cast<double>(total);
    const double freq = params_.f0_hz + (params_.f1_hz - params_.f0_hz) * frac;
    phase_ += freq / sample_rate;
    if (phase_ >= 1.0) phase_ -= 1.0;
    double env = 1.0;
    if (params_.attack_seconds > 0.0 && t < params_.attack_seconds) {
      env = t / params_.attack_seconds;
    }
    const double tail = params_.seconds - t;
    if (params_.decay_seconds > 0.0 && tail < params_.decay_seconds) {
      env = std::min(env, tail / params_.decay_seconds);
    }
    double sample = 0.0;
    if constexpr (W == Wave::Sine) {
      sample = std::sin(2.0 * kPi * phase_);
    } else if constexpr (W == Wave::Square) {
      sample = phase_ < 0.5 ? 0.8 : -0.8;
    } else if constexpr (W == Wave::Saw) {
      sample = 2.0 * phase_ - 1.0;
    } else {  // Wave::Noise
      sample = rng_.uniform() * 2.0 - 1.0;
    }
    return static_cast<float>(sample * params_.volume * env);
  }

 private:
  static constexpr double kPi = 3.14159265358979323846;

  SfxParams params_;
  Rng rng_;
  double phase_ = 0.0;
};

} // namespace edm::plugins
