#include <unity.h>

#include "motion/TiltScrub.h"

// Tilt-to-scrub: dead zone, rate curve, direction, debounce/reset. Pure module,
// synthetic accel samples and fake dt.

namespace {

using motion::TiltScrub;

constexpr float kLevel = 0.0f;      // level: no roll
constexpr float kInside = 0.10f;    // inside the 0.18 g dead zone
constexpr float kEdgeOut = 0.30f;   // just past the dead zone
constexpr float kFull = 0.70f;      // beyond full scale (0.65 g)
constexpr uint32_t kDt = 50;        // ms per fed sample

}  // namespace

void test_level_never_scrubs() {
  TiltScrub t;
  for (int i = 0; i < 100; ++i) {
    TEST_ASSERT_EQUAL_INT(0, t.updateWithSample(kLevel, 0.0f, 1.0f, kDt, true));
  }
  TEST_ASSERT_FALSE(t.active());
}

void test_inside_dead_zone_is_idle() {
  TiltScrub t;
  for (int i = 0; i < 100; ++i) {
    TEST_ASSERT_EQUAL_INT(0, t.updateWithSample(kInside, 0.0f, 1.0f, kDt, true));
  }
  TEST_ASSERT_FALSE(t.active());
}

void test_forward_roll_accumulates_forward() {
  TiltScrub t;
  int steps = 0;
  for (int i = 0; i < 100; ++i) {
    steps = t.updateWithSample(kFull, 0.0f, 1.0f, kDt, true);
  }
  // Full-scale (12 wps) over 100 * 50 ms = 5 s -> ~60 words, forward (+).
  TEST_ASSERT_TRUE(t.active());
  TEST_ASSERT_TRUE(steps > 0);
  TEST_ASSERT_INT_WITHIN(3, 60, steps);
}

void test_backward_roll_accumulates_backward() {
  TiltScrub t;
  int steps = 0;
  for (int i = 0; i < 100; ++i) {
    steps = t.updateWithSample(-kFull, 0.0f, 1.0f, kDt, true);
  }
  TEST_ASSERT_TRUE(steps < 0);
  TEST_ASSERT_INT_WITHIN(3, -60, steps);
}

void test_rate_grows_with_tilt() {
  TiltScrub slow;
  TiltScrub fast;
  int slowSteps = 0;
  int fastSteps = 0;
  for (int i = 0; i < 100; ++i) {
    slowSteps = slow.updateWithSample(kEdgeOut, 0.0f, 1.0f, kDt, true);
    fastSteps = fast.updateWithSample(kFull, 0.0f, 1.0f, kDt, true);
  }
  // A steeper roll scrubs more words in the same time.
  TEST_ASSERT_TRUE(fastSteps > slowSteps);
}

void test_returning_to_level_ends_gesture_but_keeps_steps() {
  TiltScrub t;
  int steps = 0;
  for (int i = 0; i < 40; ++i) {
    steps = t.updateWithSample(kFull, 0.0f, 1.0f, kDt, true);
  }
  TEST_ASSERT_TRUE(t.active());
  TEST_ASSERT_TRUE(steps > 0);

  // Roll back to level: gesture ends, but the accumulated steps remain for the
  // caller to act on (like a released touch finishing on the last target).
  const int afterRelease = t.updateWithSample(kLevel, 0.0f, 1.0f, kDt, true);
  TEST_ASSERT_FALSE(t.active());
  TEST_ASSERT_EQUAL_INT(steps, afterRelease);
}

void test_disabled_resets_accumulator() {
  TiltScrub t;
  for (int i = 0; i < 40; ++i) {
    t.updateWithSample(kFull, 0.0f, 1.0f, kDt, true);
  }
  TEST_ASSERT_TRUE(t.steps() != 0);
  // Disabling (touch down, menu, focus timer, flick) zeroes the gesture...
  TEST_ASSERT_EQUAL_INT(0, t.updateWithSample(kFull, 0.0f, 1.0f, kDt, false));
  TEST_ASSERT_FALSE(t.active());
  TEST_ASSERT_EQUAL_INT(0, t.steps());
  // ...and the next enabled tick starts a fresh gesture from zero.
  TEST_ASSERT_EQUAL_INT(0, t.updateWithSample(kInside, 0.0f, 1.0f, kDt, true));
}

void test_huge_dt_is_clamped() {
  TiltScrub t;
  // A 10 s dt (loop hitch) must not lurch the position: clamped to maxStepDtMs.
  const int steps = t.updateWithSample(kFull, 0.0f, 1.0f, 10000, true);
  // 200 ms cap * 12 wps = ~2.4 words, not ~120.
  TEST_ASSERT_TRUE(steps <= 3);
}

void test_reset_clears_everything() {
  TiltScrub t;
  for (int i = 0; i < 40; ++i) {
    t.updateWithSample(kFull, 0.0f, 1.0f, kDt, true);
  }
  t.reset();
  TEST_ASSERT_FALSE(t.active());
  TEST_ASSERT_EQUAL_INT(0, t.steps());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_level_never_scrubs);
  RUN_TEST(test_inside_dead_zone_is_idle);
  RUN_TEST(test_forward_roll_accumulates_forward);
  RUN_TEST(test_backward_roll_accumulates_backward);
  RUN_TEST(test_rate_grows_with_tilt);
  RUN_TEST(test_returning_to_level_ends_gesture_but_keeps_steps);
  RUN_TEST(test_disabled_resets_accumulator);
  RUN_TEST(test_huge_dt_is_clamped);
  RUN_TEST(test_reset_clears_everything);
  return UNITY_END();
}
