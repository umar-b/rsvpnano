#include <unity.h>

#include "stats/Achievements.h"

using stats::Achievement;
using stats::AchievementInputs;

namespace {

AchievementInputs cleanSlate() { return AchievementInputs{}; }

}  // namespace

void test_empty_inputs_unlock_nothing() {
  AchievementInputs in = cleanSlate();
  TEST_ASSERT_EQUAL_UINT32(0, stats::qualifyingMask(in));
}

void test_first_book_finished() {
  AchievementInputs in = cleanSlate();
  in.finishedBooks = 1;
  const uint32_t mask = stats::qualifyingMask(in);
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::FirstBookFinished));
  TEST_ASSERT_FALSE(stats::isUnlocked(mask, Achievement::FiveBooksFinished));
}

void test_book_milestones_are_cumulative() {
  AchievementInputs in = cleanSlate();
  in.finishedBooks = 10;
  const uint32_t mask = stats::qualifyingMask(in);
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::FirstBookFinished));
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::FiveBooksFinished));
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::TenBooksFinished));
}

void test_word_milestones() {
  AchievementInputs in = cleanSlate();
  in.lifetimeWords = 100000;
  const uint32_t mask = stats::qualifyingMask(in);
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::TenThousandWords));
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::HundredThousandWords));
  TEST_ASSERT_FALSE(stats::isUnlocked(mask, Achievement::MillionWords));
}

void test_million_words() {
  AchievementInputs in = cleanSlate();
  in.lifetimeWords = 1000000;
  TEST_ASSERT_TRUE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::MillionWords));
}

void test_word_milestone_just_below_threshold() {
  AchievementInputs in = cleanSlate();
  in.lifetimeWords = 9999;
  TEST_ASSERT_FALSE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::TenThousandWords));
}

void test_streak_requires_valid_clock() {
  AchievementInputs in = cleanSlate();
  in.currentStreak = 30;
  in.clockValid = false;
  const uint32_t maskInvalid = stats::qualifyingMask(in);
  TEST_ASSERT_FALSE(stats::isUnlocked(maskInvalid, Achievement::SevenDayStreak));
  TEST_ASSERT_FALSE(stats::isUnlocked(maskInvalid, Achievement::ThirtyDayStreak));

  in.clockValid = true;
  const uint32_t maskValid = stats::qualifyingMask(in);
  TEST_ASSERT_TRUE(stats::isUnlocked(maskValid, Achievement::SevenDayStreak));
  TEST_ASSERT_TRUE(stats::isUnlocked(maskValid, Achievement::ThirtyDayStreak));
}

void test_seven_day_streak_not_thirty() {
  AchievementInputs in = cleanSlate();
  in.currentStreak = 7;
  in.clockValid = true;
  const uint32_t mask = stats::qualifyingMask(in);
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::SevenDayStreak));
  TEST_ASSERT_FALSE(stats::isUnlocked(mask, Achievement::ThirtyDayStreak));
}

void test_speed_reader_requires_word_floor() {
  AchievementInputs in = cleanSlate();
  in.lastSessionAvgWpm = 650;
  in.lastSessionWords = 50;  // below the floor
  TEST_ASSERT_FALSE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::SpeedReader));

  in.lastSessionWords = 100;  // at the floor
  TEST_ASSERT_TRUE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::SpeedReader));
}

void test_speed_reader_requires_wpm_threshold() {
  AchievementInputs in = cleanSlate();
  in.lastSessionWords = 500;
  in.lastSessionAvgWpm = 599;
  TEST_ASSERT_FALSE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::SpeedReader));
  in.lastSessionAvgWpm = 600;
  TEST_ASSERT_TRUE(stats::isUnlocked(stats::qualifyingMask(in), Achievement::SpeedReader));
}

void test_apply_unlocks_reports_only_new_bits() {
  AchievementInputs in = cleanSlate();
  in.finishedBooks = 1;
  uint32_t mask = 0;

  const uint32_t firstNew = stats::applyUnlocks(mask, in);
  TEST_ASSERT_EQUAL_UINT32(stats::achievementBit(Achievement::FirstBookFinished), firstNew);
  TEST_ASSERT_TRUE(stats::isUnlocked(mask, Achievement::FirstBookFinished));

  // Re-evaluating the same inputs unlocks nothing new (no re-trigger).
  const uint32_t secondNew = stats::applyUnlocks(mask, in);
  TEST_ASSERT_EQUAL_UINT32(0, secondNew);

  // Crossing a new threshold reports only the freshly crossed bit.
  in.finishedBooks = 5;
  const uint32_t thirdNew = stats::applyUnlocks(mask, in);
  TEST_ASSERT_EQUAL_UINT32(stats::achievementBit(Achievement::FiveBooksFinished), thirdNew);
}

void test_bitmask_roundtrip_and_count() {
  AchievementInputs in = cleanSlate();
  in.finishedBooks = 10;
  in.lifetimeWords = 1000000;
  in.currentStreak = 30;
  in.clockValid = true;
  in.lastSessionWords = 1000;
  in.lastSessionAvgWpm = 700;

  uint32_t mask = 0;
  stats::applyUnlocks(mask, in);
  // Every catalogue achievement should be unlocked by this maximal input.
  TEST_ASSERT_EQUAL_UINT32(stats::kAllAchievementsMask, mask & stats::kAllAchievementsMask);
  TEST_ASSERT_EQUAL_UINT8(stats::kAchievementCount, stats::unlockedCount(mask));

  // A round-trip through a u32 store preserves the set.
  const uint32_t stored = mask;
  uint32_t restored = stored;
  TEST_ASSERT_EQUAL_UINT8(stats::kAchievementCount, stats::unlockedCount(restored));
}

void test_unlocked_count_ignores_high_bits() {
  // Spurious high bits (e.g. a stale/corrupt NVS value) must not inflate count.
  const uint32_t mask = 0xFFFFFFFFUL;
  TEST_ASSERT_EQUAL_UINT8(stats::kAchievementCount, stats::unlockedCount(mask));
}

void test_names_present_for_all() {
  for (uint8_t i = 0; i < stats::kAchievementCount; ++i) {
    const char *name = stats::achievementName(static_cast<Achievement>(i));
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_TRUE(name[0] != '\0');
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_inputs_unlock_nothing);
  RUN_TEST(test_first_book_finished);
  RUN_TEST(test_book_milestones_are_cumulative);
  RUN_TEST(test_word_milestones);
  RUN_TEST(test_million_words);
  RUN_TEST(test_word_milestone_just_below_threshold);
  RUN_TEST(test_streak_requires_valid_clock);
  RUN_TEST(test_seven_day_streak_not_thirty);
  RUN_TEST(test_speed_reader_requires_word_floor);
  RUN_TEST(test_speed_reader_requires_wpm_threshold);
  RUN_TEST(test_apply_unlocks_reports_only_new_bits);
  RUN_TEST(test_bitmask_roundtrip_and_count);
  RUN_TEST(test_unlocked_count_ignores_high_bits);
  RUN_TEST(test_names_present_for_all);
  return UNITY_END();
}
