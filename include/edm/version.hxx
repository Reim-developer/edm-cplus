#pragma once

// edm-cplus version. v0.0.1: the synth core.

namespace edm {

struct Version {
  int major;
  int minor;
  int patch;
};

inline constexpr Version kVersion{0, 0, 1};
inline constexpr const char* kVersionString = "0.0.1";

} // namespace edm
