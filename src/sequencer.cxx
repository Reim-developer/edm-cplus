// Sequencer implementation. See sequencer.hxx.

#include "edm/sequencer.hxx"

#include <cmath>
#include <utility>

#include "edm/constants.hxx"

namespace edm {
namespace {

// Sanity cap so absurd tempos cannot overflow the integer.
constexpr std::size_t kMaxFramesPerStep = 1000000000;

// Frames per 16th-note step, rounded to the nearest sample.
// Step duration is (60 / bpm / 4) seconds: seconds-per-minute /
// steps-per-beat — exactly 15.0, written as the ratio so the
// time model reads, instead of a magic literal.
std::size_t compute_frames_per_step(double bpm, double sample_rate) noexcept {
  const double frames = (kSecondsPerMinute / kStepsPerBeat) * sample_rate / bpm;
  if (!(frames >= 1.0)) return 1;
  if (frames > static_cast<double>(kMaxFramesPerStep)) return kMaxFramesPerStep;
  return static_cast<std::size_t>(std::llround(frames));
}

} // namespace

Sequencer::Sequencer(double bpm, double sample_rate) {
  const double safe_bpm = bpm > 0.0 ? bpm : kDefaultBpm;
  const double safe_rate = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
  frames_per_step_ = compute_frames_per_step(safe_bpm, safe_rate);
}

void Sequencer::add_track(std::unique_ptr<Voice> voice, Pattern pattern, float volume) {
  if (voice == nullptr) return;  // null voices are ignored
  // Aggregate-initialize: Pattern has no default constructor, so
  // the entry cannot be default-constructed and assigned.
  TrackEntry entry{Track{std::move(pattern), std::move(voice), volume}, TrackState{}};
  tracks_.push_back(std::move(entry));
  fire_track_step(tracks_.back(), step_);  // start now, on the current step
}

void Sequencer::render(std::span<float> out) noexcept {
  for (float& s : out) {
    float mix = 0.0f;
    for (auto& entry : tracks_) {
      mix += entry.track.voice->next() * entry.track.volume;
    }
    s = mix;
    ++offset_;
    if (offset_ >= frames_per_step_) {
      offset_ = 0;
      ++step_;
      fire_step(step_);
    }
  }
}

void Sequencer::reset() noexcept {
  step_ = 0;
  offset_ = 0;
  for (auto& entry : tracks_) {
    entry.state = TrackState{};
    entry.track.voice->reset();
  }
  fire_step(0);
}

void Sequencer::fire_step(std::size_t step) noexcept {
  for (auto& entry : tracks_) fire_track_step(entry, step);
}

void Sequencer::fire_track_step(TrackEntry& entry, std::size_t step) noexcept {
  // Gate elapsed? Release the sounding note.
  if (entry.state.active_midi.has_value() &&
      step >= entry.state.start_step + static_cast<std::size_t>(entry.state.length_steps)) {
    entry.track.voice->note_off(entry.state.active_midi.value());
    entry.state.active_midi.reset();
  }
  // Note on this step? Fire it (retrigger if one is sounding).
  const int size = entry.track.pattern.size();
  if (size <= 0) return;
  const Step& s =
      entry.track.pattern.at(static_cast<int>(step % static_cast<std::size_t>(size)));
  if (s.note.has_value()) {
    entry.track.voice->note_on(s.note.value(), s.velocity);
    entry.state.active_midi = s.note;
    entry.state.start_step = step;
    entry.state.length_steps = s.length_steps;
  }
}

} // namespace edm
