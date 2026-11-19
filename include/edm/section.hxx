#pragma once

// Automation: a parameter curve over bar positions — the mechanism
// that makes a section move (a filter opening across a build, a
// track fading in). Points are (bar position, value); evaluation
// interpolates between them and clamps outside the range.
//
// Linear is a straight interpolation; Exponential is geometric
// (value = a * (b/a)^t) — the right shape for filter sweeps, since
// pitch is logarithmic. Exponential segments need positive values;
// a segment with a non-positive endpoint falls back to Linear for
// that segment, so at() is total and never NaN.
//
// An empty automation is a no-op: Section skips it entirely, and
// at() returns 0.0 (documented — callers check empty() first).
//
// Section: a named span of bars with its own sequencer — one part
// of a song ("intro", "drop"). The name is USER content (no enum
// of section kinds — same principle as the v0.0.4 preset removal:
// the library provides the mechanism, the user names the parts).
// The sequencer is moved in; Section owns it, so there are no
// dangling references. Section boundaries reset the playhead: each
// section renders from its own sequencer start, and voices do NOT
// ring across sections in this version (cross-section tails belong
// to the future effects layer).
//
// Patterns may be ANY length: a pattern whose length equals the
// section length (bars*16 steps) plays exactly once — that is how
// a snare roll in the last bar of a build is written (a 64-step
// pattern over a 4-bar section, notes only in the last 16 steps).
//
// Rendering is BLOCKED (64 frames per block): automations are
// evaluated at each block boundary and applied, then the block is
// rendered. Cutoff steps at that rate are inaudible (no zipper),
// and the cost is one biquad coefficient recompute per automated
// voice per block — not per sample. This keeps the Sequencer
// untouched (no automation hook in it).

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "edm/pattern.hxx"
#include "edm/sequencer.hxx"
#include "edm/voice.hxx"

namespace edm {

enum class Curve { Linear, Exponential };

class Automation {
 public:
  // Points are sorted by position at construction (the caller may
  // pass them in any order).
  Automation(std::vector<std::pair<double, double>> points, Curve curve);

  // Value at a bar position: interpolated between the surrounding
  // points, clamped to the first/last value outside the range.
  // Returns 0.0 for an empty automation.
  [[nodiscard]] double at(double bar) const noexcept;

  [[nodiscard]] bool empty() const noexcept { return points_.empty(); }

 private:
  std::vector<std::pair<double, double>> points_;
  Curve curve_;
};

class Section {
 public:
  // Takes ownership of the sequencer (moved in). bars <= 0 makes an
  // empty section (frames() == 0, renders silence).
  Section(std::string name, int bars, Sequencer sequencer);

  // Render this section into the span. Renders up to frames()
  // samples (or the span size, whichever is smaller); a longer
  // span's tail is silenced. Resets the sequencer first, so
  // re-rendering a section reproduces it bit-identically.
  // Never allocates.
  void render(std::span<float> out);

  // Automation bindings. Returns false when the target cannot be
  // bound: bad track index, an empty curve, or (for the cutoff) a
  // voice that is not filterable. Binding again replaces the
  // previous curve for that track/voice. The cutoff cast happens
  // ONCE, here at bind time — never in the render loop.
  bool bind_filter_cutoff(std::size_t track, Automation curve);
  bool bind_track_volume(std::size_t track, Automation curve);

  // Sidechain: duck the music bus on every kick — the pump. The
  // section renders twice (kick-only bus, then music-only bus,
  // each from a reset sequencer) and mixes kick + ducked music.
  // The gain curve comes from the kick PATTERN's note-on times:
  // gain = 1 - depth * exp(-t/release) after each kick.
  //
  // Deliberate difference from the spike: the ducking is UNIFORM
  // across the whole music bus. The spike ducks the lead less than
  // the bass; the library has one bus and one curve — simpler, and
  // per-track depth belongs to a future version if anyone asks.
  //
  // depth is clamped to [0, 1], release to >= 1 ms. A kick track
  // with no notes (or a bad index) means no ducking — the mix is
  // still correct. The kick times are snapshotted here: the
  // section owns its sequencer, so no pattern can change after
  // this call, and render stays allocation-free.
  void enable_sidechain(std::size_t kick_track, double depth = 0.7,
                        double release_seconds = 0.1);

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] int bars() const noexcept { return bars_; }
  [[nodiscard]] std::size_t frames() const noexcept {
    return sequencer_.frames_for_bars(bars_);
  }

 private:
  struct CutoffBinding {
    FilterableVoice* voice;  // owned by the sequencer; tied to this section
    Automation curve;
  };
  struct VolumeBinding {
    std::size_t track;
    Automation curve;
  };

  // The blocked render loop (automations per block). Resets the
  // sequencer first; does NOT touch mutes (the sidechain path sets
  // them before calling this).
  void render_blocked(std::span<float> out);
  // The two-pass sidechain render: kick bus, music bus, then mix.
  void render_with_sidechain(std::span<float> out);
  // Kick note-on times in frames, from the kick pattern (loops
  // over the section when the pattern is shorter than it).
  [[nodiscard]] std::vector<std::size_t> kick_times(std::size_t kick_track) const;

  std::string name_;
  int bars_;
  Sequencer sequencer_;
  std::vector<CutoffBinding> cutoff_bindings_;
  std::vector<VolumeBinding> volume_bindings_;
  // Sidechain state. The buses are sized on the first sidechain
  // render and reused after that; the kick times are snapshotted
  // by enable_sidechain. Sections without sidechain carry neither.
  bool sidechain_enabled_ = false;
  std::size_t sidechain_kick_track_ = 0;
  double sidechain_depth_ = 0.7;
  double sidechain_release_seconds_ = 0.1;
  std::vector<std::size_t> kick_times_;
  std::vector<float> kick_bus_;
  std::vector<float> music_bus_;
};

} // namespace edm
