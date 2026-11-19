#pragma once

// edm-cplus version. v0.0.3: sequencer + patterns.

namespace edm {

struct Version {
  int major;
  int minor;
  int patch;
};

inline constexpr Version kVersion{0, 0, 3};
inline constexpr const char* kVersionString = "0.0.3";

} // namespace edm
