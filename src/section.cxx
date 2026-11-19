// Automation + Section implementation. See section.hxx.

#include "edm/section.hxx"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace edm {
namespace {

// Block size for the section render loop: automations are evaluated
// once per block. 64 frames is ~1.45 ms at 44.1 kHz — cutoff steps
// at that rate are inaudible, and one biquad recompute per block
// per automated voice is cheap.
constexpr std::size_t kAutomationBlockFrames = 64;

} // namespace

// Section moves its sequencer in — the move must exist.
static_assert(std::is_move_constructible_v<Sequencer>,
              "Section takes its sequencer by move");

Automation::Automation(std::vector<std::pair<double, double>> points, Curve curve)
    : points_(std::move(points)), curve_(curve) {
  std::sort(points_.begin(), points_.end(),
            [](const std::pair<double, double>& a, const std::pair<double, double>& b) {
              return a.first < b.first;
            });
}

double Automation::at(double bar) const noexcept {
  if (points_.empty()) return 0.0;
  if (points_.size() == 1 || bar <= points_.front().first) return points_.front().second;
  if (bar >= points_.back().first) return points_.back().second;
  // Find the segment [i, i+1] containing bar. The scan uses <= so
  // that at an exact point position the segment STARTING there wins
  // (for duplicate positions, the right point wins: a step change
  // at bar 2 means "from bar 2 on, the new value"). Point counts
  // are single digits in practice; a linear scan is the clear choice.
  std::size_t i = 0;
  while (i + 1 < points_.size() && points_[i + 1].first <= bar) ++i;
  const double x0 = points_[i].first;
  const double x1 = points_[i + 1].first;
  const double a = points_[i].second;
  const double b = points_[i + 1].second;
  if (x1 <= x0) return b;  // duplicate positions: the right point wins
  const double t = (bar - x0) / (x1 - x0);
  if (curve_ == Curve::Exponential && a > 0.0 && b > 0.0) {
    return a * std::pow(b / a, t);
  }
  return a + (b - a) * t;  // Linear, or the exponential fallback
}

Section::Section(std::string name, int bars, Sequencer sequencer)
    : name_(std::move(name)),
      bars_(bars > 0 ? bars : 0),
      sequencer_(std::move(sequencer)) {}

bool Section::bind_filter_cutoff(std::size_t track, Automation curve) {
  if (curve.empty()) return false;
  Voice* voice = sequencer_.track_voice(track);
  if (voice == nullptr) return false;
  FilterableVoice* filterable = dynamic_cast<FilterableVoice*>(voice);
  if (filterable == nullptr) return false;  // not a filterable voice
  // Rebinding replaces the previous curve for this voice.
  std::erase_if(cutoff_bindings_,
                [filterable](const CutoffBinding& b) { return b.voice == filterable; });
  cutoff_bindings_.push_back(CutoffBinding{filterable, std::move(curve)});
  return true;
}

bool Section::bind_track_volume(std::size_t track, Automation curve) {
  if (curve.empty()) return false;
  if (track >= sequencer_.track_count()) return false;
  std::erase_if(volume_bindings_,
                [track](const VolumeBinding& b) { return b.track == track; });
  volume_bindings_.push_back(VolumeBinding{track, std::move(curve)});
  return true;
}

void Section::enable_sidechain(std::size_t kick_track, double depth,
                               double release_seconds) {
  sidechain_enabled_ = true;
  sidechain_kick_track_ = kick_track;
  sidechain_depth_ = std::clamp(depth, 0.0, 1.0);
  sidechain_release_seconds_ =
      release_seconds > 0.001 ? release_seconds : 0.001;
  kick_times_ = kick_times(kick_track);  // snapshot; patterns can't change now
}

std::vector<std::size_t> Section::kick_times(std::size_t kick_track) const {
  const Pattern& pattern = sequencer_.track_pattern(kick_track);
  const int length = pattern.size();
  if (length <= 0) return {};
  const std::size_t frames_per_step = sequencer_.frames_per_step();
  const std::size_t total = frames();
  const std::size_t section_steps =
      static_cast<std::size_t>(bars_) * static_cast<std::size_t>(kStepsPerBar);
  std::vector<std::size_t> times;
  // The pattern loops over the section: a 16-step kick pattern in a
  // 4-bar section fires its notes 4 times.
  for (std::size_t base = 0; base < section_steps; base += static_cast<std::size_t>(length)) {
    for (int step = 0; step < length; ++step) {
      if (pattern.at(step).note.has_value()) {
        const std::size_t time = (base + static_cast<std::size_t>(step)) * frames_per_step;
        if (time < total) times.push_back(time);
      }
    }
  }
  return times;  // already sorted (base, then step, both ascending)
}

void Section::render(std::span<float> out) {
  for (std::size_t i = 0; i < sequencer_.track_count(); ++i) {
    sequencer_.set_track_muted(i, false);  // a plain render hears all tracks
  }
  if (sidechain_enabled_) {
    render_with_sidechain(out);
    return;
  }
  render_blocked(out);
}

void Section::render_with_sidechain(std::span<float> out) {
  const std::size_t total = frames();
  // Buses are sized once and reused; a section without sidechain
  // never allocates them.
  if (kick_bus_.size() != total) kick_bus_.resize(total);
  if (music_bus_.size() != total) music_bus_.resize(total);

  // Kick bus: mute everything except the kick track.
  for (std::size_t i = 0; i < sequencer_.track_count(); ++i) {
    sequencer_.set_track_muted(i, i != sidechain_kick_track_);
  }
  render_blocked(std::span<float>(kick_bus_));

  // Music bus: mute only the kick track. render_blocked resets the
  // sequencer, so the music voices start from the same state as in
  // the kick pass — the two buses are independent renders.
  for (std::size_t i = 0; i < sequencer_.track_count(); ++i) {
    sequencer_.set_track_muted(i, i == sidechain_kick_track_);
  }
  render_blocked(std::span<float>(music_bus_));

  // Mix: kick + ducked music. One pass over the samples, one pass
  // over the kick times (both monotonic).
  const double sample_rate = sequencer_.sample_rate();
  std::size_t kick = 0;  // index of the most recent kick at or before i
  const std::size_t to_render = out.size() < total ? out.size() : total;
  for (std::size_t i = 0; i < to_render; ++i) {
    while (kick + 1 < kick_times_.size() && kick_times_[kick + 1] <= i) ++kick;
    double gain = 1.0;
    if (!kick_times_.empty() && kick_times_[kick] <= i) {
      const double t =
          static_cast<double>(i - kick_times_[kick]) / sample_rate;
      gain = 1.0 - sidechain_depth_ *
                         std::exp(-t / sidechain_release_seconds_);
    }
    out[i] = kick_bus_[i] + music_bus_[i] * static_cast<float>(gain);
  }
  for (std::size_t i = to_render; i < out.size(); ++i) out[i] = 0.0f;
}

void Section::render_blocked(std::span<float> out) {
  sequencer_.reset();  // every render starts from the section start
  const std::size_t total = frames();
  const std::size_t to_render = out.size() < total ? out.size() : total;
  const std::size_t frames_per_bar = sequencer_.frames_per_bar();
  std::size_t done = 0;
  while (done < to_render) {
    // Evaluate automations at this block's bar position, apply them,
    // then render the block.
    const double bar =
        frames_per_bar > 0
            ? static_cast<double>(done) / static_cast<double>(frames_per_bar)
            : 0.0;
    for (const auto& binding : cutoff_bindings_) {
      binding.voice->set_filter_cutoff(binding.curve.at(bar));
    }
    for (const auto& binding : volume_bindings_) {
      sequencer_.set_track_volume(binding.track,
                                  static_cast<float>(binding.curve.at(bar)));
    }
    std::size_t block = kAutomationBlockFrames;
    if (done + block > to_render) block = to_render - done;
    sequencer_.render(out.subspan(done, block));
    done += block;
  }
  // A span longer than the section: silence the tail.
  for (std::size_t i = to_render; i < out.size(); ++i) out[i] = 0.0f;
}

} // namespace edm
