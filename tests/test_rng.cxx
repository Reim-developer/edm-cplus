// Rng determinism: two RNGs with the same seed produce identical streams.

#include "check.hxx"
#include "edm/rng.hxx"

int main() {
  test::Context ctx;

  // Same seed -> identical streams (raw values and uniforms).
  {
    edm::Rng a(42);
    edm::Rng b(42);
    bool same = true;
    for (int i = 0; i < 1000; ++i) {
      if (a.next() != b.next()) same = false;
    }
    ctx.check(same, "same seed -> identical raw stream (1000 values)");
  }
  {
    edm::Rng a(1234);
    edm::Rng b(1234);
    bool same = true;
    for (int i = 0; i < 1000; ++i) {
      if (a.uniform() != b.uniform()) same = false;
    }
    ctx.check(same, "same seed -> identical uniform stream (1000 values)");
  }

  // Different seeds -> different streams.
  {
    edm::Rng a(1);
    edm::Rng b(2);
    bool differ = false;
    for (int i = 0; i < 100; ++i) {
      if (a.next() != b.next()) differ = true;
    }
    ctx.check(differ, "different seeds -> different streams");
  }

  // uniform() stays in [0, 1); bipolar() stays in [-1, 1).
  {
    edm::Rng rng(7);
    bool in_range = true;
    for (int i = 0; i < 10000; ++i) {
      const double u = rng.uniform();
      if (u < 0.0 || u >= 1.0) in_range = false;
    }
    ctx.check(in_range, "uniform() in [0, 1) over 10000 draws");
  }
  {
    edm::Rng rng(7);
    bool in_range = true;
    for (int i = 0; i < 10000; ++i) {
      const float v = rng.bipolar();
      if (v < -1.0f || v >= 1.0f) in_range = false;
    }
    ctx.check(in_range, "bipolar() in [-1, 1) over 10000 draws");
  }

  // Seed 0 must not lock the generator up (xorshift with state 0 is stuck).
  {
    edm::Rng rng(0);
    bool varies = false;
    const std::uint32_t first = rng.next();
    for (int i = 0; i < 100; ++i) {
      if (rng.next() != first) varies = true;
    }
    ctx.check(varies, "seed 0 produces a varying stream");
  }

  return ctx.finish("rng");
}
