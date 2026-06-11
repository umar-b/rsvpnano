#include <unity.h>

#include "time/DeviceClock.h"

using devclock::DeviceClock;

// 2024-06-01T12:00:00Z = 1717243200. A round, post-2020 reference.
static constexpr int64_t kRefEpoch = 1717243200;

void test_epoch_now_advances_with_millis() {
  // 90s of elapsed millis -> 90s of epoch.
  TEST_ASSERT_EQUAL_INT64(kRefEpoch + 90,
                          devclock::epochNowSec(kRefEpoch, 1000, 91000));
}

void test_epoch_now_handles_now_before_ref() {
  // now behind ref (just-set clock) holds at the reference rather than rewinding.
  TEST_ASSERT_EQUAL_INT64(kRefEpoch, devclock::epochNowSec(kRefEpoch, 5000, 4000));
}

void test_epoch_now_tolerates_millis_wraparound() {
  // refMillis near uint32 max, nowMillis wrapped past 0 -> ~2s elapsed.
  const uint32_t refMs = 0xFFFFFFFFu - 1000u;  // 1000 ms before wrap
  const uint32_t nowMs = 1000u;                // 1000 ms after wrap
  TEST_ASSERT_EQUAL_INT64(kRefEpoch + 2, devclock::epochNowSec(kRefEpoch, refMs, nowMs));
}

void test_local_day_key_utc() {
  // 1717243200 = 2024-06-01 -> day 19875 since epoch.
  TEST_ASSERT_EQUAL_UINT32(19875, devclock::localDayKey(kRefEpoch, 0));
}

void test_local_day_key_tz_pushes_across_midnight() {
  // 2024-06-01T23:30:00Z. +60 min tz -> 2024-06-02 local -> day rolls forward.
  const int64_t late = 1717284600;  // 23:30Z
  const uint32_t utcDay = devclock::localDayKey(late, 0);
  const uint32_t cetDay = devclock::localDayKey(late, 60);
  TEST_ASSERT_EQUAL_UINT32(utcDay + 1, cetDay);
}

void test_local_day_key_negative_tz_pulls_back() {
  // 2024-06-01T00:30:00Z. -60 min tz -> 2024-05-31 local -> day rolls back.
  const int64_t early = 1717201800;  // 00:30Z
  const uint32_t utcDay = devclock::localDayKey(early, 0);
  const uint32_t westDay = devclock::localDayKey(early, -60);
  TEST_ASSERT_EQUAL_UINT32(utcDay - 1, westDay);
}

void test_epoch_validity_floor() {
  TEST_ASSERT_FALSE(devclock::epochLooksValid(0));
  TEST_ASSERT_FALSE(devclock::epochLooksValid(12345));  // seconds-since-boot
  TEST_ASSERT_TRUE(devclock::epochLooksValid(kRefEpoch));
}

void test_clock_invalid_until_set() {
  DeviceClock clock;
  TEST_ASSERT_FALSE(clock.valid());
  TEST_ASSERT_EQUAL_UINT32(0, clock.localDayKeyNow(1000));
  TEST_ASSERT_EQUAL_INT64(0, clock.epochNowSec(1000));
}

void test_set_reference_rejects_bogus_epoch() {
  DeviceClock clock;
  TEST_ASSERT_FALSE(clock.setReference(500, 1000));  // pre-2020 -> rejected
  TEST_ASSERT_FALSE(clock.valid());
  TEST_ASSERT_TRUE(clock.setReference(kRefEpoch, 1000));
  TEST_ASSERT_TRUE(clock.valid());
  TEST_ASSERT_FALSE(clock.stale());
}

void test_set_reference_then_day_key_now() {
  DeviceClock clock;
  clock.setTimezoneOffsetMinutes(60);
  clock.setReference(kRefEpoch, 1000);  // 2024-06-01 12:00Z
  // 6 hours later (21600s): 18:00Z -> 19:00 CET, still 2024-06-01 local.
  const uint32_t dayNow = clock.localDayKeyNow(1000 + 21600u * 1000u);
  TEST_ASSERT_EQUAL_UINT32(devclock::localDayKey(kRefEpoch, 60), dayNow);
}

void test_restore_snapshot_is_valid_but_stale() {
  DeviceClock clock;
  clock.restoreSnapshot(kRefEpoch, 2000);
  TEST_ASSERT_TRUE(clock.valid());
  TEST_ASSERT_TRUE(clock.stale());
  // A fresh set clears staleness.
  clock.setReference(kRefEpoch + 100, 3000);
  TEST_ASSERT_FALSE(clock.stale());
}

void test_restore_snapshot_rejects_bogus_epoch() {
  DeviceClock clock;
  clock.restoreSnapshot(42, 1000);
  TEST_ASSERT_FALSE(clock.valid());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_epoch_now_advances_with_millis);
  RUN_TEST(test_epoch_now_handles_now_before_ref);
  RUN_TEST(test_epoch_now_tolerates_millis_wraparound);
  RUN_TEST(test_local_day_key_utc);
  RUN_TEST(test_local_day_key_tz_pushes_across_midnight);
  RUN_TEST(test_local_day_key_negative_tz_pulls_back);
  RUN_TEST(test_epoch_validity_floor);
  RUN_TEST(test_clock_invalid_until_set);
  RUN_TEST(test_set_reference_rejects_bogus_epoch);
  RUN_TEST(test_set_reference_then_day_key_now);
  RUN_TEST(test_restore_snapshot_is_valid_but_stale);
  RUN_TEST(test_restore_snapshot_rejects_bogus_epoch);
  return UNITY_END();
}
