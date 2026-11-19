#pragma once

// Sequencer: schedules patterns at a tempo and renders them through
// voices. The composition layer — this is what turns the library
// from "makes sounds" into "makes music".
//
// Time model: 16th-note steps, 4 steps per beat, 16 steps per bar
// (4/4). Step duration = (60 / bpm / 4) * sample_rate samples,
// rounded to the nearest sample (fractional tempos round; the drift
// is sub-sample per step). Patterns loop; tracks may have different
// pattern lengths, so polyrhythm falls out for free.
//
// Scheduling is sample-accurate: the playhead tracks the current
// step + the sample offset within it. At each step boundary, for
// each track, a note on that step fires note_on, and a sounding
// note whose gate (length_steps) has elapsed fires note_off.
// Render is per-sample through the voices (correctness first;
// block rendering is a later performance version).
//
// A track added before the first render starts at step 0. A track
// added mid-render starts immediately, on the current step (its
// note fires now, mid-step, and releases on schedule). reset()
// rewinds to step 0 and fires its notes.
//
// Deterministic: two fresh sequencers with the same setup render
// identical output (noise voices consume their own seeded Rng).

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "edm/constants.hxx"
#include "edm/pattern.hxx"
#include "edm/voice.hxx"

namespace edm {

struct Track {
  Pattern pattern;
  std::unique_ptr<Voice> voice;
  float volume = 1.0f;
};

class Sequencer {
 public:
  // bpm <= 0 falls back to 120, sample_rate <= 0 to 44100.
  Sequencer(double bpm, double sample_rate);

  // Takes ownership of the voice. Null voices are ignored.
  void add_track(std::unique_ptr<Voice> voice, Pattern pattern, float volume = 1.0f);

  // Mute a track: it neither fires notes nor renders, so its voice
  // stays idle (a muted noise voice consumes no randomness). Bad
  // indices are no-ops. Mutes survive reset().
  void set_track_muted(std::size_t index, bool muted) noexcept;
  [[nodiscard]] bool is_track_muted(std::size_t index) const noexcept;

  // Set a track's volume (1.0 = as added). Bad indices are no-ops.
  // This is how automation moves a track's volume at render time.
  void set_track_volume(std::size_t index, float volume) noexcept;

  // Non-owning access to a track's voice (for bind-time casts) and
  // pattern (for sidechain kick times). Bad indices return nullptr
  // / an empty pattern — never a crash.
  [[nodiscard]] Voice* track_voice(std::size_t index) noexcept;
  [[nodiscard]] const Pattern& track_pattern(std::size_t index) const noexcept;
  [[nodiscard]] std::size_t track_count() const noexcept { return tracks_.size(); }

  // The sample rate this sequencer was built with (the fallback
  // default when the caller passed a non-positive rate).
  [[nodiscard]] double sample_rate() const noexcept { return sample_rate_; }

  // Fill the span from the current playhead, advancing it.
  // Never allocates. An empty span is a no-op; with no tracks the
  // output is silence.
  void render(std::span<float> out) noexcept;

  // Buffer-sizing helpers (integer frames; see the time model).
  [[nodiscard]] std::size_t frames_per_step() const noexcept { return frames_per_step_; }
  [[nodiscard]] std::size_t frames_per_bar() const noexcept { return frames_per_step_ * kStepsPerBar; }
  [[nodiscard]] std::size_t frames_for_bars(int bars) const noexcept {
    return frames_per_bar() * static_cast<std::size_t>(bars > 0 ? bars : 0);
  }

  // Rewind to step 0, silence all voices, fire step 0's notes.
  void reset() noexcept;

 private:
  struct TrackState {
    std::optional<int> active_midi;  // the note currently sounding
    std::size_t start_step = 0;      // global step the note started on
    int length_steps = 0;            // its gate length
  };

  struct TrackEntry {
    Track track;
    TrackState state;
    bool muted = false;
  };

  // Fire one step's events for every track: note_off for gates
  // that elapsed, note_on for notes on this step.
  void fire_step(std::size_t step) noexcept;
  // Fire one track's events for one step.
  void fire_track_step(TrackEntry& entry, std::size_t step) noexcept;

  std::size_t frames_per_step_ = 0;
  double sample_rate_ = kDefaultSampleRate;
  std::vector<TrackEntry> tracks_;
  std::size_t step_ = 0;    // current step (global, unbounded)
  std::size_t offset_ = 0;  // samples into the current step
};

} // namespace edm
