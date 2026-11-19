#pragma once

// Shared constants — the single source for the library's chosen
// numbers. Values used across headers, or that ARE the library's
// behavior, live here as inline constexpr (one definition, usable
// from any translation unit). Implementation details stay
// file-local in their .cxx (see filter.cxx, wav.cxx, sfx.cxx).
//
// Two values that happen to be equal but mean different things
// get separate names — that is the point (kSquareWaveAmplitude
// vs kDefaultVelocity, both 0.8). Definitional math stays inline
// at its use site (0.0/1.0 levels, the 2*phase-1 saw formula,
// the 0.5 square duty cycle, 2*pi).

#include <array>
#include <cstddef>

namespace edm {

// The sample rate everything defaults to when the caller passes a
// non-positive rate (and the rate the examples render at).
inline constexpr double kDefaultSampleRate = 44100.0;

// The tempo the sequencer falls back to for a non-positive BPM.
inline constexpr double kDefaultBpm = 120.0;

// Concert pitch: A4 (MIDI 69) = 440 Hz. The default reference for
// midi_to_freq — see voice.hxx for alternate concert pitch.
inline constexpr double kA4Frequency = 440.0;
inline constexpr int kA4MidiNote = 69;
inline constexpr int kSemitonesPerOctave = 12;

// The sequencer's time model: 16th-note steps, 4 steps per beat,
// 16 steps per bar (4/4). Step duration in samples is
// (kSecondsPerMinute / bpm / kStepsPerBeat) * sample_rate.
inline constexpr double kSecondsPerMinute = 60.0;
inline constexpr int kStepsPerBeat = 4;
inline constexpr int kStepsPerBar = 16;

inline constexpr double kPi = 3.14159265358979323846;

// Square-wave peak amplitude (the spike's proven value): a square
// at full volume peaks at 0.8, not 1.0.
inline constexpr double kSquareWaveAmplitude = 0.8;

// The default seed for noise voices (SynthVoice) — the spike's
// seed, kept so the demos sound the same as v0.0.1.
inline constexpr int kDefaultSeed = 1234;

// The default note velocity in a Pattern step.
inline constexpr float kDefaultVelocity = 0.8f;

// The supersaw stack: 7 detuned saws (the classic trance lead).
// Per-saw gain 0.35/7, so the stack peaks around 0.35 when the
// saws are in phase (the spike's proven values).
inline constexpr std::size_t kSupersawVoices = 7;
inline constexpr std::array<double, kSupersawVoices> kSupersawDetuneCents = {
    -12.0, -8.0, -4.0, 0.0, 4.0, 8.0, 12.0};
inline constexpr double kSupersawGain = 0.35;

// The master peak the demo normalizes to (the spike's master
// chain: tanh soft clip, then normalize to 0.89).
inline constexpr float kMixNormalizePeak = 0.89f;

} // namespace edm
