#include <unity.h>

#include "reader/TimeEstimate.h"

namespace {

// Synthetic pacing: word i costs i ms of bonus. Makes range sums easy to
// verify against the closed form and exercises non-uniform blocks.
uint32_t bonusAt(size_t i) { return static_cast<uint32_t>(i); }

uint32_t linearBonusSum(size_t start, size_t end) {
  uint32_t sum = 0;
  for (size_t i = start; i < end; ++i) {
    sum += bonusAt(i);
  }
  return sum;
}

timeestimate::PrefixCache builtCache(size_t wordCount) {
  timeestimate::PrefixCache cache;
  cache.beginBuild(wordCount);
  while (!cache.stepBuild(1, bonusAt)) {
  }
  return cache;
}

}  // namespace

void test_base_ms_zero_cases() {
  TEST_ASSERT_EQUAL_UINT32(0, timeestimate::baseMs(0, 100, 0, 300));
  TEST_ASSERT_EQUAL_UINT32(0, timeestimate::baseMs(0, 100, 100, 0));
  TEST_ASSERT_EQUAL_UINT32(0, timeestimate::baseMs(50, 50, 100, 300));
  TEST_ASSERT_EQUAL_UINT32(0, timeestimate::baseMs(80, 20, 100, 300));
}

void test_base_ms_clamps_indices_to_word_count() {
  // 100 words at 60 WPM = 1s per word; end index past the book clamps.
  TEST_ASSERT_EQUAL_UINT32(100000, timeestimate::baseMs(0, 500, 100, 60));
}

void test_base_ms_scales_with_wpm() {
  TEST_ASSERT_EQUAL_UINT32(60000, timeestimate::baseMs(0, 300, 300, 300));
  TEST_ASSERT_EQUAL_UINT32(30000, timeestimate::baseMs(0, 300, 300, 600));
}

void test_format_sub_minute_rounds_to_zero() {
  TEST_ASSERT_EQUAL_STRING("0m", timeestimate::formatRemaining(0).c_str());
  TEST_ASSERT_EQUAL_STRING("0m", timeestimate::formatRemaining(59999).c_str());
}

void test_format_minutes_hours_days() {
  TEST_ASSERT_EQUAL_STRING("12m", timeestimate::formatRemaining(12UL * 60000UL).c_str());
  TEST_ASSERT_EQUAL_STRING("1h", timeestimate::formatRemaining(60UL * 60000UL).c_str());
  TEST_ASSERT_EQUAL_STRING("1h30m", timeestimate::formatRemaining(90UL * 60000UL).c_str());
  TEST_ASSERT_EQUAL_STRING("2d", timeestimate::formatRemaining(48UL * 3600000UL).c_str());
  TEST_ASSERT_EQUAL_STRING("2d4h", timeestimate::formatRemaining(52UL * 3600000UL).c_str());
}

void test_format_clock_24h_zero_padded_minutes() {
  TEST_ASSERT_EQUAL_STRING("21:42", timeestimate::formatClock(21 * 60 + 42).c_str());
  TEST_ASSERT_EQUAL_STRING("9:05", timeestimate::formatClock(9 * 60 + 5).c_str());
  TEST_ASSERT_EQUAL_STRING("0:00", timeestimate::formatClock(0).c_str());
  // Past-midnight wrap: 24:30 -> 0:30.
  TEST_ASSERT_EQUAL_STRING("0:30", timeestimate::formatClock(24 * 60 + 30).c_str());
}

void test_build_steps_toward_valid() {
  timeestimate::PrefixCache cache;
  // 600 words -> 3 blocks of 256/256/88.
  const size_t blocks = cache.beginBuild(600);
  TEST_ASSERT_EQUAL_UINT32(3, blocks);
  TEST_ASSERT_TRUE(cache.buildInProgress());
  TEST_ASSERT_FALSE(cache.valid());

  TEST_ASSERT_FALSE(cache.stepBuild(1, bonusAt));
  TEST_ASSERT_EQUAL_INT(33, cache.buildProgressPercent());
  TEST_ASSERT_FALSE(cache.stepBuild(1, bonusAt));
  TEST_ASSERT_TRUE(cache.stepBuild(1, bonusAt));

  TEST_ASSERT_TRUE(cache.valid());
  TEST_ASSERT_FALSE(cache.buildInProgress());
  TEST_ASSERT_EQUAL_UINT32(linearBonusSum(0, 600), cache.totalBonusMs());
}

void test_bonus_is_zero_before_build_completes() {
  timeestimate::PrefixCache cache;
  cache.beginBuild(600);
  cache.stepBuild(1, bonusAt);
  TEST_ASSERT_EQUAL_UINT32(0, cache.bonusMs(0, 600, 600, bonusAt));
}

void test_bonus_full_range_matches_linear_sum() {
  const timeestimate::PrefixCache cache = builtCache(600);
  TEST_ASSERT_EQUAL_UINT32(linearBonusSum(0, 600), cache.bonusMs(0, 600, 600, bonusAt));
}

void test_bonus_range_spanning_blocks_matches_linear_sum() {
  const timeestimate::PrefixCache cache = builtCache(600);
  // Partial head, one full block, partial tail.
  TEST_ASSERT_EQUAL_UINT32(linearBonusSum(100, 550), cache.bonusMs(100, 550, 600, bonusAt));
}

void test_bonus_range_within_one_block_matches_linear_sum() {
  const timeestimate::PrefixCache cache = builtCache(600);
  TEST_ASSERT_EQUAL_UINT32(linearBonusSum(10, 200), cache.bonusMs(10, 200, 600, bonusAt));
}

void test_bonus_clamps_to_word_count() {
  const timeestimate::PrefixCache cache = builtCache(600);
  TEST_ASSERT_EQUAL_UINT32(linearBonusSum(590, 600), cache.bonusMs(590, 9999, 600, bonusAt));
  TEST_ASSERT_EQUAL_UINT32(0, cache.bonusMs(700, 800, 600, bonusAt));
}

void test_invalidate_drops_cache() {
  timeestimate::PrefixCache cache = builtCache(600);
  cache.invalidate();
  TEST_ASSERT_FALSE(cache.valid());
  TEST_ASSERT_FALSE(cache.buildInProgress());
  TEST_ASSERT_EQUAL_UINT32(0, cache.bonusMs(0, 600, 600, bonusAt));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_base_ms_zero_cases);
  RUN_TEST(test_base_ms_clamps_indices_to_word_count);
  RUN_TEST(test_base_ms_scales_with_wpm);
  RUN_TEST(test_format_sub_minute_rounds_to_zero);
  RUN_TEST(test_format_minutes_hours_days);
  RUN_TEST(test_format_clock_24h_zero_padded_minutes);
  RUN_TEST(test_build_steps_toward_valid);
  RUN_TEST(test_bonus_is_zero_before_build_completes);
  RUN_TEST(test_bonus_full_range_matches_linear_sum);
  RUN_TEST(test_bonus_range_spanning_blocks_matches_linear_sum);
  RUN_TEST(test_bonus_range_within_one_block_matches_linear_sum);
  RUN_TEST(test_bonus_clamps_to_word_count);
  RUN_TEST(test_invalidate_drops_cache);
  return UNITY_END();
}
