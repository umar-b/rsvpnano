#include <unity.h>

#include "stats/ReadingStats.h"

using stats::ReadingStats;
using stats::Snapshot;

void test_empty_stats_are_zero() {
  ReadingStats s;
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot().totalMs);
  TEST_ASSERT_EQUAL_UINT32(0, s.averageWpm());
}

void test_record_accumulates_total_and_day() {
  ReadingStats s;
  s.recordSession(7, 100, 60000);  // 100 words in 60s -> 100 wpm
  TEST_ASSERT_EQUAL_UINT32(100, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(60000, s.snapshot().totalMs);
  TEST_ASSERT_EQUAL_UINT32(7, s.snapshot().dayKey);
  TEST_ASSERT_EQUAL_UINT32(100, s.snapshot().dayWords);
  TEST_ASSERT_EQUAL_UINT32(60000, s.snapshot().dayMs);
  TEST_ASSERT_EQUAL_UINT32(100, s.averageWpm());
}

void test_same_day_sessions_add_to_day_bucket() {
  ReadingStats s;
  s.recordSession(3, 50, 30000);
  s.recordSession(3, 50, 30000);
  TEST_ASSERT_EQUAL_UINT32(100, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(100, s.snapshot().dayWords);
  TEST_ASSERT_EQUAL_UINT32(60000, s.snapshot().dayMs);
}

void test_day_boundary_rolls_day_bucket_but_keeps_totals() {
  ReadingStats s;
  s.recordSession(1, 200, 60000);
  s.recordSession(2, 80, 60000);  // new day key -> day bucket resets
  TEST_ASSERT_EQUAL_UINT32(280, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(120000, s.snapshot().totalMs);
  TEST_ASSERT_EQUAL_UINT32(2, s.snapshot().dayKey);
  TEST_ASSERT_EQUAL_UINT32(80, s.snapshot().dayWords);
  TEST_ASSERT_EQUAL_UINT32(60000, s.snapshot().dayMs);
}

void test_zero_session_is_noop() {
  ReadingStats s;
  s.recordSession(5, 0, 0);
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot().dayKey);
}

void test_average_wpm_ignores_tiny_durations() {
  ReadingStats s;
  s.recordSession(1, 10, 500);  // under the 1s floor
  TEST_ASSERT_EQUAL_UINT32(0, s.averageWpm());
}

void test_restores_from_snapshot() {
  Snapshot snap;
  snap.totalWords = 1000;
  snap.totalMs = 600000;  // -> 100 wpm
  snap.dayKey = 9;
  snap.dayWords = 200;
  snap.dayMs = 120000;
  ReadingStats s(snap);
  TEST_ASSERT_EQUAL_UINT32(100, s.averageWpm());
  // A new same-key session keeps the restored day bucket growing.
  s.recordSession(9, 100, 60000);
  TEST_ASSERT_EQUAL_UINT32(1100, s.snapshot().totalWords);
  TEST_ASSERT_EQUAL_UINT32(300, s.snapshot().dayWords);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_stats_are_zero);
  RUN_TEST(test_record_accumulates_total_and_day);
  RUN_TEST(test_same_day_sessions_add_to_day_bucket);
  RUN_TEST(test_day_boundary_rolls_day_bucket_but_keeps_totals);
  RUN_TEST(test_zero_session_is_noop);
  RUN_TEST(test_average_wpm_ignores_tiny_durations);
  RUN_TEST(test_restores_from_snapshot);
  return UNITY_END();
}
