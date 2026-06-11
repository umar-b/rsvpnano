#include <unity.h>

#include "library/MenuGesture.h"

using library::MenuHoldConfig;
using library::MenuHoldSample;

void test_hold_fires_when_still_and_long() {
  MenuHoldConfig cfg;  // defaults: maxDriftPx 26, holdMs 550
  MenuHoldSample sample;
  sample.deltaX = 4;
  sample.deltaY = -6;
  sample.elapsedMs = 600;
  TEST_ASSERT_TRUE(library::isHold(sample, cfg));
}

void test_hold_not_fired_before_threshold() {
  MenuHoldConfig cfg;
  MenuHoldSample sample;
  sample.deltaX = 0;
  sample.deltaY = 0;
  sample.elapsedMs = 549;  // one ms short
  TEST_ASSERT_FALSE(library::isHold(sample, cfg));
}

void test_hold_fired_exactly_at_threshold() {
  MenuHoldConfig cfg;
  MenuHoldSample sample;
  sample.elapsedMs = 550;  // exactly the threshold counts
  TEST_ASSERT_TRUE(library::isHold(sample, cfg));
}

void test_hold_cancelled_by_drift() {
  MenuHoldConfig cfg;
  MenuHoldSample sample;
  sample.elapsedMs = 1000;  // plenty of time
  sample.deltaX = 27;       // but drifted past the box on X
  sample.deltaY = 0;
  TEST_ASSERT_FALSE(library::isHold(sample, cfg));

  sample.deltaX = 0;
  sample.deltaY = -40;  // drifted on Y (a scroll swipe, not a hold)
  TEST_ASSERT_FALSE(library::isHold(sample, cfg));
}

void test_hold_drift_at_box_edge_still_holds() {
  MenuHoldConfig cfg;
  MenuHoldSample sample;
  sample.elapsedMs = 800;
  sample.deltaX = 26;   // exactly maxDriftPx is allowed
  sample.deltaY = -26;
  TEST_ASSERT_TRUE(library::isHold(sample, cfg));
}

void test_hold_respects_custom_config() {
  MenuHoldConfig cfg;
  cfg.maxDriftPx = 10;
  cfg.holdMs = 300;
  MenuHoldSample sample;
  sample.elapsedMs = 320;
  sample.deltaX = 8;
  sample.deltaY = 8;
  TEST_ASSERT_TRUE(library::isHold(sample, cfg));

  sample.deltaX = 11;  // past the tighter box
  TEST_ASSERT_FALSE(library::isHold(sample, cfg));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hold_fires_when_still_and_long);
  RUN_TEST(test_hold_not_fired_before_threshold);
  RUN_TEST(test_hold_fired_exactly_at_threshold);
  RUN_TEST(test_hold_cancelled_by_drift);
  RUN_TEST(test_hold_drift_at_box_edge_still_holds);
  RUN_TEST(test_hold_respects_custom_config);
  return UNITY_END();
}
