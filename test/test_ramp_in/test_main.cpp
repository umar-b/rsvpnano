#include <unity.h>

#include "reader/RampIn.h"

void test_disabled_is_always_full_speed() {
  for (uint32_t w = 0; w < 30; ++w) {
    TEST_ASSERT_EQUAL_UINT16(100, rampin::intervalScalePercent(w, false));
  }
}

void test_first_word_is_slowest() {
  // 60% speed -> interval stretched to ~167%.
  const uint16_t firstScale = rampin::intervalScalePercent(0, true);
  TEST_ASSERT_GREATER_THAN_UINT16(150, firstScale);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(170, firstScale);
}

void test_ramp_is_monotonically_decreasing_toward_full_speed() {
  uint16_t previous = rampin::intervalScalePercent(0, true);
  for (uint32_t w = 1; w < rampin::kRampWords; ++w) {
    const uint16_t scale = rampin::intervalScalePercent(w, true);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(previous, scale);
    previous = scale;
  }
}

void test_ramp_completes_at_full_speed() {
  TEST_ASSERT_EQUAL_UINT16(100, rampin::intervalScalePercent(rampin::kRampWords, true));
  TEST_ASSERT_EQUAL_UINT16(100, rampin::intervalScalePercent(rampin::kRampWords + 5, true));
}

void test_scale_never_below_full_speed() {
  // The ramp only ever slows the reader, never speeds it past 100%.
  for (uint32_t w = 0; w < rampin::kRampWords + 5; ++w) {
    TEST_ASSERT_GREATER_OR_EQUAL_UINT16(100, rampin::intervalScalePercent(w, true));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_is_always_full_speed);
  RUN_TEST(test_first_word_is_slowest);
  RUN_TEST(test_ramp_is_monotonically_decreasing_toward_full_speed);
  RUN_TEST(test_ramp_completes_at_full_speed);
  RUN_TEST(test_scale_never_below_full_speed);
  return UNITY_END();
}
