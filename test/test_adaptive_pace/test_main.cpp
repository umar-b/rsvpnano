#include <unity.h>

#include "reader/AdaptivePace.h"

using adaptivepace::Config;
using adaptivepace::Decider;

namespace {
Config testConfig() {
  Config cfg;
  cfg.windowMs = 60000;
  cfg.easeStepPermille = 100;
  cfg.maxSteps = 3;
  cfg.recoverAfterMs = 180000;
  return cfg;
}
}  // namespace

void test_single_rewind_does_nothing(void) {
  Decider d(testConfig());
  TEST_ASSERT_FALSE(d.noteRewind(1000));
  TEST_ASSERT_EQUAL_UINT16(1000, d.scalePermille());
}

void test_burst_of_two_eases_one_step(void) {
  Decider d(testConfig());
  TEST_ASSERT_FALSE(d.noteRewind(1000));
  TEST_ASSERT_TRUE(d.noteRewind(30000));
  TEST_ASSERT_EQUAL_UINT8(1, d.steps());
  TEST_ASSERT_EQUAL_UINT16(1100, d.scalePermille());
}

void test_spaced_rewinds_never_trigger(void) {
  Decider d(testConfig());
  TEST_ASSERT_FALSE(d.noteRewind(0));
  TEST_ASSERT_FALSE(d.noteRewind(61000));   // outside the window
  TEST_ASSERT_FALSE(d.noteRewind(130000));  // outside again
  TEST_ASSERT_EQUAL_UINT8(0, d.steps());
}

void test_each_step_needs_a_fresh_pair(void) {
  Decider d(testConfig());
  d.noteRewind(0);
  TEST_ASSERT_TRUE(d.noteRewind(1000));  // step 1
  // The very next rewind starts a new pair rather than stepping immediately.
  TEST_ASSERT_FALSE(d.noteRewind(2000));
  TEST_ASSERT_TRUE(d.noteRewind(3000));  // step 2
  TEST_ASSERT_EQUAL_UINT8(2, d.steps());
}

void test_steps_cap_at_max(void) {
  Decider d(testConfig());
  uint32_t t = 0;
  for (int pair = 0; pair < 6; ++pair) {
    d.noteRewind(t);
    d.noteRewind(t + 1000);
    t += 2000;
  }
  TEST_ASSERT_EQUAL_UINT8(3, d.steps());
  TEST_ASSERT_EQUAL_UINT16(1300, d.scalePermille());
}

void test_quiet_period_recovers_stepwise(void) {
  Decider d(testConfig());
  d.noteRewind(0);
  d.noteRewind(1000);  // step 1
  d.noteRewind(2000);
  d.noteRewind(3000);  // step 2
  TEST_ASSERT_EQUAL_UINT8(2, d.steps());

  TEST_ASSERT_FALSE(d.update(3000 + 179999));  // not quiet long enough
  TEST_ASSERT_TRUE(d.update(3000 + 180000));   // one step back
  TEST_ASSERT_EQUAL_UINT8(1, d.steps());
  TEST_ASSERT_FALSE(d.update(3000 + 180000 + 1000));  // next recovery needs its own quiet spell
  TEST_ASSERT_TRUE(d.update(3000 + 2 * 180000));
  TEST_ASSERT_EQUAL_UINT8(0, d.steps());
  TEST_ASSERT_FALSE(d.update(3000 + 3 * 180000));  // already at base
}

void test_rewind_during_recovery_restarts_quiet_clock(void) {
  Decider d(testConfig());
  d.noteRewind(0);
  d.noteRewind(1000);  // step 1
  d.noteRewind(100000);  // lone rewind, no step, but resets quiet
  TEST_ASSERT_FALSE(d.update(1000 + 180000));  // quiet clock restarted at 100000
  TEST_ASSERT_TRUE(d.update(100000 + 180000));
}

void test_reset_clears_everything(void) {
  Decider d(testConfig());
  d.noteRewind(0);
  d.noteRewind(1000);
  d.reset();
  TEST_ASSERT_EQUAL_UINT8(0, d.steps());
  TEST_ASSERT_EQUAL_UINT16(1000, d.scalePermille());
  TEST_ASSERT_FALSE(d.noteRewind(2000));  // pending pair also cleared
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_single_rewind_does_nothing);
  RUN_TEST(test_burst_of_two_eases_one_step);
  RUN_TEST(test_spaced_rewinds_never_trigger);
  RUN_TEST(test_each_step_needs_a_fresh_pair);
  RUN_TEST(test_steps_cap_at_max);
  RUN_TEST(test_quiet_period_recovers_stepwise);
  RUN_TEST(test_rewind_during_recovery_restarts_quiet_clock);
  RUN_TEST(test_reset_clears_everything);
  return UNITY_END();
}
