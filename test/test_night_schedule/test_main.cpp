#include <unity.h>

#include "time/NightSchedule.h"

using nightschedule::isNight;
using nightschedule::localMinutesOfDay;

void test_local_minutes_utc(void) {
  // 2026-07-09 14:30:00 UTC = epoch 1783002600
  TEST_ASSERT_EQUAL_UINT16(14 * 60 + 30, localMinutesOfDay(1783002600LL, 0));
}

void test_local_minutes_positive_offset_wraps_forward(void) {
  // 23:30 UTC + 120 min = 01:30 next day local.
  const int64_t epoch = 1783002600LL - (14 * 60 + 30) * 60 + (23 * 60 + 30) * 60;
  TEST_ASSERT_EQUAL_UINT16(1 * 60 + 30, localMinutesOfDay(epoch, 120));
}

void test_local_minutes_negative_offset_wraps_backward(void) {
  // 00:30 UTC - 300 min (EST) = 19:30 previous day local.
  const int64_t epoch = 1783002600LL - (14 * 60 + 30) * 60 + 30 * 60;
  TEST_ASSERT_EQUAL_UINT16(19 * 60 + 30, localMinutesOfDay(epoch, -300));
}

void test_night_window_wraps_midnight(void) {
  // Default 21:00-07:00.
  TEST_ASSERT_TRUE(isNight(21 * 60));      // 21:00 inclusive start
  TEST_ASSERT_TRUE(isNight(23 * 60 + 59));
  TEST_ASSERT_TRUE(isNight(0));
  TEST_ASSERT_TRUE(isNight(6 * 60 + 59));
  TEST_ASSERT_FALSE(isNight(7 * 60));      // 07:00 exclusive end
  TEST_ASSERT_FALSE(isNight(12 * 60));
  TEST_ASSERT_FALSE(isNight(20 * 60 + 59));
}

void test_night_window_same_day(void) {
  TEST_ASSERT_TRUE(isNight(2 * 60, 1 * 60, 5 * 60));
  TEST_ASSERT_FALSE(isNight(5 * 60, 1 * 60, 5 * 60));
  TEST_ASSERT_FALSE(isNight(0, 1 * 60, 5 * 60));
}

void test_degenerate_window_is_never_night(void) {
  TEST_ASSERT_FALSE(isNight(0, 300, 300));
  TEST_ASSERT_FALSE(isNight(300, 300, 300));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_local_minutes_utc);
  RUN_TEST(test_local_minutes_positive_offset_wraps_forward);
  RUN_TEST(test_local_minutes_negative_offset_wraps_backward);
  RUN_TEST(test_night_window_wraps_midnight);
  RUN_TEST(test_night_window_same_day);
  RUN_TEST(test_degenerate_window_is_never_night);
  return UNITY_END();
}
