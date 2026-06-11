#include <unity.h>

#include "display/BurnInJitter.h"

// Burn-in jitter schedule: deterministic, +/-1 px, steps every ~3 min, and the
// long-run average is centred (component sums over one cycle are zero). Pure
// helper, fake elapsed-ms.

namespace {

using burnin::JitterOffset;
constexpr uint32_t kStep = burnin::kJitterStepMs;

}  // namespace

void test_offsets_are_single_pixel() {
  for (uint32_t e = 0; e < kStep * 20; e += kStep / 4 + 1) {
    const JitterOffset o = burnin::jitterOffset(e);
    TEST_ASSERT_TRUE(o.dx == 1 || o.dx == -1);
    TEST_ASSERT_TRUE(o.dy == 1 || o.dy == -1);
  }
}

void test_deterministic_for_same_elapsed() {
  const JitterOffset a = burnin::jitterOffset(7 * kStep + 123);
  const JitterOffset b = burnin::jitterOffset(7 * kStep + 123);
  TEST_ASSERT_EQUAL_INT(a.dx, b.dx);
  TEST_ASSERT_EQUAL_INT(a.dy, b.dy);
}

void test_offset_holds_within_a_step() {
  const JitterOffset start = burnin::jitterOffset(3 * kStep);
  const JitterOffset mid = burnin::jitterOffset(3 * kStep + kStep / 2);
  const JitterOffset endish = burnin::jitterOffset(3 * kStep + kStep - 1);
  TEST_ASSERT_EQUAL_INT(start.dx, mid.dx);
  TEST_ASSERT_EQUAL_INT(start.dy, mid.dy);
  TEST_ASSERT_EQUAL_INT(start.dx, endish.dx);
  TEST_ASSERT_EQUAL_INT(start.dy, endish.dy);
}

void test_steps_at_boundary() {
  // Consecutive steps differ in at least one axis (the layout actually moves).
  bool sawChange = false;
  for (uint32_t step = 0; step < 8; ++step) {
    const JitterOffset a = burnin::jitterOffset(step * kStep);
    const JitterOffset b = burnin::jitterOffset((step + 1) * kStep);
    if (a.dx != b.dx || a.dy != b.dy) {
      sawChange = true;
    }
  }
  TEST_ASSERT_TRUE(sawChange);
}

void test_average_is_centred_over_a_cycle() {
  // Over one full pattern cycle the component sums must be zero so the panel
  // wears evenly and the motion is invisible on average.
  int sumX = 0;
  int sumY = 0;
  const int cycleSteps = 4;  // pattern length
  for (int step = 0; step < cycleSteps; ++step) {
    const JitterOffset o = burnin::jitterOffset(static_cast<uint32_t>(step) * kStep);
    sumX += o.dx;
    sumY += o.dy;
  }
  TEST_ASSERT_EQUAL_INT(0, sumX);
  TEST_ASSERT_EQUAL_INT(0, sumY);
}

void test_wraps_without_drift() {
  // The schedule runs forever: an elapsed time one full cycle later maps to the
  // same offset.
  const int cycleSteps = 4;
  const JitterOffset a = burnin::jitterOffset(2 * kStep);
  const JitterOffset b =
      burnin::jitterOffset((2 + cycleSteps) * kStep);
  TEST_ASSERT_EQUAL_INT(a.dx, b.dx);
  TEST_ASSERT_EQUAL_INT(a.dy, b.dy);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_offsets_are_single_pixel);
  RUN_TEST(test_deterministic_for_same_elapsed);
  RUN_TEST(test_offset_holds_within_a_step);
  RUN_TEST(test_steps_at_boundary);
  RUN_TEST(test_average_is_centred_over_a_cycle);
  RUN_TEST(test_wraps_without_drift);
  return UNITY_END();
}
