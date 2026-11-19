#pragma once

// edm-cplus version. v0.0.5: edm::plugins namespace + CRTP base.

namespace edm {

struct Version {
  int major;
  int minor;
  int patch;
};

inline constexpr Version kVersion{0, 0, 5};
inline constexpr const char* kVersionString = "0.0.5";

} // namespace edm
