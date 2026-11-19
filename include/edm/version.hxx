#pragma once

// edm-cplus version. v0.0.4: SFX plugin refactor + CHANGELOG.

namespace edm {

struct Version {
  int major;
  int minor;
  int patch;
};

inline constexpr Version kVersion{0, 0, 4};
inline constexpr const char* kVersionString = "0.0.4";

} // namespace edm
