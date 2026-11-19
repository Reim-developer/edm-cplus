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

void Section::render(std::span<float> out) {
  sequencer_.reset();  // every render starts from the section start
  for (std::size_t i = 0; i < sequencer_.track_count(); ++i) {
    sequencer_.set_track_muted(i, false);  // a plain render hears all tracks
  }
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
