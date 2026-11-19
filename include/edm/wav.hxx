#pragma once

// 16-bit mono WAV writer, so anything the library renders can be heard
// and verified. Float samples in [-1, 1] are clamped and quantized to
// int16. Returns false when the file cannot be opened or written.
// No exceptions, no allocation on the caller's side beyond the span.
//
// Ported from the game-audio spike (spike.cpp), which proved the header
// layout with Python's wave module.

#include <span>
#include <string>

namespace edm {

// sample_rate must be positive; an empty span writes a valid 0-frame WAV.
bool write_wav(const std::string& path, std::span<const float> samples, int sample_rate);

} // namespace edm
