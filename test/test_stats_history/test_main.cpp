#include <unity.h>

#include "stats/StatsHistory.h"

using stats::DayBucket;
using stats::StatsHistory;

void test_empty_history() {
  StatsHistory h;
  TEST_ASSERT_EQUAL_UINT8(0, h.dayCount());
  TEST_ASSERT_EQUAL_UINT32(0, h.wordsForDay(100));
  TEST_ASSERT_EQUAL_UINT16(0, h.currentStreak(100));
  TEST_ASSERT_EQUAL_UINT16(0, h.bestStreak());
}

void test_record_same_day_accumulates() {
  StatsHistory h;
  h.recordSession(100, 500, 60000);
  h.recordSession(100, 300, 30000);
  TEST_ASSERT_EQUAL_UINT8(1, h.dayCount());
  TEST_ASSERT_EQUAL_UINT32(800, h.wordsForDay(100));
}

void test_record_new_day_opens_bucket() {
  StatsHistory h;
  h.recordSession(100, 500, 60000);
  h.recordSession(101, 200, 20000);
  TEST_ASSERT_EQUAL_UINT8(2, h.dayCount());
  TEST_ASSERT_EQUAL_UINT32(500, h.wordsForDay(100));
  TEST_ASSERT_EQUAL_UINT32(200, h.wordsForDay(101));
}

void test_zero_session_and_zero_daykey_are_noops() {
  StatsHistory h;
  h.recordSession(100, 0, 0);
  h.recordSession(0, 500, 60000);  // clock-invalid day key ignored
  TEST_ASSERT_EQUAL_UINT8(0, h.dayCount());
}

void test_ring_buffer_evicts_oldest_past_30() {
  StatsHistory h;
  // 32 distinct days: 1..32. Oldest two (1,2) should be evicted.
  for (uint32_t d = 1; d <= 32; ++d) {
    h.recordSession(d, d * 10, 1000);
  }
  TEST_ASSERT_EQUAL_UINT8(30, h.dayCount());
  TEST_ASSERT_EQUAL_UINT32(0, h.wordsForDay(1));
  TEST_ASSERT_EQUAL_UINT32(0, h.wordsForDay(2));
  TEST_ASSERT_EQUAL_UINT32(30, h.wordsForDay(3));
  TEST_ASSERT_EQUAL_UINT32(320, h.wordsForDay(32));
}

void test_streak_counts_consecutive_ending_today() {
  StatsHistory h;
  h.recordSession(100, 10, 1000);
  h.recordSession(101, 10, 1000);
  h.recordSession(102, 10, 1000);
  TEST_ASSERT_EQUAL_UINT16(3, h.currentStreak(102));
}

void test_streak_alive_when_today_not_read_yet() {
  StatsHistory h;
  h.recordSession(100, 10, 1000);
  h.recordSession(101, 10, 1000);
  // todayKey 102 has no words yet; streak ending yesterday stays alive.
  TEST_ASSERT_EQUAL_UINT16(2, h.currentStreak(102));
}

void test_streak_broken_by_gap() {
  StatsHistory h;
  h.recordSession(100, 10, 1000);
  // gap at 101
  h.recordSession(102, 10, 1000);
  h.recordSession(103, 10, 1000);
  TEST_ASSERT_EQUAL_UINT16(2, h.currentStreak(103));
}

void test_streak_zero_when_today_and_yesterday_empty() {
  StatsHistory h;
  h.recordSession(100, 10, 1000);
  // two-day gap means streak ended long ago.
  TEST_ASSERT_EQUAL_UINT16(0, h.currentStreak(110));
}

void test_streak_zero_when_clock_invalid() {
  StatsHistory h;
  h.recordSession(100, 10, 1000);
  TEST_ASSERT_EQUAL_UINT16(0, h.currentStreak(0));
}

void test_best_streak_finds_longest_run() {
  StatsHistory h;
  // run of 2 (10,11), gap, run of 3 (20,21,22).
  h.recordSession(10, 5, 100);
  h.recordSession(11, 5, 100);
  h.recordSession(20, 5, 100);
  h.recordSession(21, 5, 100);
  h.recordSession(22, 5, 100);
  TEST_ASSERT_EQUAL_UINT16(3, h.bestStreak());
}

void test_goal_progress_permille() {
  StatsHistory h;
  h.recordSession(100, 2500, 60000);
  TEST_ASSERT_EQUAL_UINT16(500, h.goalProgressPermille(100, 5000));
  TEST_ASSERT_FALSE(h.goalReached(100, 5000));
}

void test_goal_progress_clamps_at_full() {
  StatsHistory h;
  h.recordSession(100, 8000, 60000);
  TEST_ASSERT_EQUAL_UINT16(1000, h.goalProgressPermille(100, 5000));
  TEST_ASSERT_TRUE(h.goalReached(100, 5000));
}

void test_goal_zero_is_trivially_met() {
  StatsHistory h;
  TEST_ASSERT_EQUAL_UINT16(1000, h.goalProgressPermille(100, 0));
}

void test_sparkline_calendar_layout_with_gaps() {
  StatsHistory h;
  // today = 50. Read days 48 and 50; day 49 unread.
  h.recordSession(48, 100, 1000);
  h.recordSession(50, 300, 1000);
  uint32_t spark[stats::kMaxDays] = {0};
  const uint32_t maxW = h.sparkline(50, spark, stats::kMaxDays);
  TEST_ASSERT_EQUAL_UINT32(300, maxW);
  // last slot = today (50) = 300, slot-1 = day 49 = 0, slot-2 = day 48 = 100.
  TEST_ASSERT_EQUAL_UINT32(300, spark[stats::kMaxDays - 1]);
  TEST_ASSERT_EQUAL_UINT32(0, spark[stats::kMaxDays - 2]);
  TEST_ASSERT_EQUAL_UINT32(100, spark[stats::kMaxDays - 3]);
}

void test_sparkline_packs_when_clock_invalid() {
  StatsHistory h;
  h.recordSession(7, 100, 1000);
  h.recordSession(8, 200, 1000);
  uint32_t spark[stats::kMaxDays] = {0};
  const uint32_t maxW = h.sparkline(0, spark, stats::kMaxDays);
  TEST_ASSERT_EQUAL_UINT32(200, maxW);
  TEST_ASSERT_EQUAL_UINT32(200, spark[stats::kMaxDays - 1]);
  TEST_ASSERT_EQUAL_UINT32(100, spark[stats::kMaxDays - 2]);
  TEST_ASSERT_EQUAL_UINT32(0, spark[stats::kMaxDays - 3]);
}

void test_restore_bucket_roundtrip() {
  StatsHistory h;
  h.restoreBucket(100, 500, 60000);
  h.restoreBucket(101, 200, 20000);
  TEST_ASSERT_EQUAL_UINT8(2, h.dayCount());
  TEST_ASSERT_EQUAL_UINT32(500, h.wordsForDay(100));
  const DayBucket &b = h.bucketAt(0);
  TEST_ASSERT_EQUAL_UINT32(100, b.dayKey);
  TEST_ASSERT_EQUAL_UINT32(500, b.words);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_history);
  RUN_TEST(test_record_same_day_accumulates);
  RUN_TEST(test_record_new_day_opens_bucket);
  RUN_TEST(test_zero_session_and_zero_daykey_are_noops);
  RUN_TEST(test_ring_buffer_evicts_oldest_past_30);
  RUN_TEST(test_streak_counts_consecutive_ending_today);
  RUN_TEST(test_streak_alive_when_today_not_read_yet);
  RUN_TEST(test_streak_broken_by_gap);
  RUN_TEST(test_streak_zero_when_today_and_yesterday_empty);
  RUN_TEST(test_streak_zero_when_clock_invalid);
  RUN_TEST(test_best_streak_finds_longest_run);
  RUN_TEST(test_goal_progress_permille);
  RUN_TEST(test_goal_progress_clamps_at_full);
  RUN_TEST(test_goal_zero_is_trivially_met);
  RUN_TEST(test_sparkline_calendar_layout_with_gaps);
  RUN_TEST(test_sparkline_packs_when_clock_invalid);
  RUN_TEST(test_restore_bucket_roundtrip);
  return UNITY_END();
}
