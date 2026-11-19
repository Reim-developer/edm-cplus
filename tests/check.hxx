#pragma once

// Minimal test helper: no framework, no macros, no global state.
// Each test binary owns one Context and returns finish() as its exit code.

#include <cmath>
#include <cstdio>

namespace test {

struct Context {
  int checks = 0;
  int failures = 0;

  void check(bool ok, const char* name) {
    ++checks;
    if (!ok) {
      ++failures;
      std::printf("FAIL: %s\n", name);
    }
  }

  void check_near(double got, double expected, double eps, const char* name) {
    ++checks;
    if (std::abs(got - expected) > eps) {
      ++failures;
      std::printf("FAIL: %s (got %g, expected %g +/- %g)\n", name, got, expected, eps);
    }
  }

  int finish(const char* suite) const {
    std::printf("%s: %d/%d checks passed\n", suite, checks - failures, checks);
    return failures == 0 ? 0 : 1;
  }
};

} // namespace test
