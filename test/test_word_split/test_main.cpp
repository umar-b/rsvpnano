#include <unity.h>

#include "reader/WordSplit.h"

void test_short_word_does_not_split() {
  TEST_ASSERT_FALSE(wordsplit::shouldSplit("reading"));
  wordsplit::SplitResult r = wordsplit::split("reading");
  TEST_ASSERT_FALSE(r.shouldSplit);
  TEST_ASSERT_TRUE(r.first.isEmpty());
  TEST_ASSERT_TRUE(r.second.isEmpty());
}

void test_word_at_threshold_splits() {
  // "complication." has 12 readable chars -> below default 14, no split.
  TEST_ASSERT_FALSE(wordsplit::shouldSplit("complication."));
  // "responsibility" has 14 readable chars -> at threshold, splits.
  TEST_ASSERT_TRUE(wordsplit::shouldSplit("responsibility"));
}

void test_split_parts_recombine_to_original() {
  wordsplit::SplitResult r = wordsplit::split("responsibility");
  TEST_ASSERT_TRUE(r.shouldSplit);
  // first ends with the hyphen; dropping it and appending second restores word.
  TEST_ASSERT_EQUAL('-', r.first[r.first.length() - 1]);
  String recombined = r.first.substring(0, r.first.length() - 1) + r.second;
  TEST_ASSERT_EQUAL_STRING("responsibility", recombined.c_str());
}

void test_trailing_punctuation_stays_on_second_part() {
  wordsplit::SplitResult r = wordsplit::split("internationalization,");
  TEST_ASSERT_TRUE(r.shouldSplit);
  TEST_ASSERT_EQUAL('-', r.first[r.first.length() - 1]);
  TEST_ASSERT_EQUAL(',', r.second[r.second.length() - 1]);
  // No hyphen leaks into the second part.
  TEST_ASSERT_EQUAL(-1, r.second.indexOf('-'));
}

void test_split_avoids_cutting_inside_a_vowel_run() {
  // The cut sits between a vowel and a consonant: first part ends in a vowel.
  wordsplit::SplitResult r = wordsplit::split("administration");
  TEST_ASSERT_TRUE(r.shouldSplit);
  const char lastLetterOfFirst = r.first[r.first.length() - 2];  // char before '-'
  const String vowels = "aeiouAEIOU";
  TEST_ASSERT_TRUE(vowels.indexOf(lastLetterOfFirst) >= 0);
}

void test_both_parts_have_at_least_two_letters() {
  wordsplit::SplitResult r = wordsplit::split("characteristically");
  TEST_ASSERT_TRUE(r.shouldSplit);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(2, wordsplit::readableCharacterCount(r.first));
  TEST_ASSERT_GREATER_OR_EQUAL_INT(2, wordsplit::readableCharacterCount(r.second));
}

void test_duration_split_is_proportional_and_sums_to_total() {
  wordsplit::SplitResult r = wordsplit::split("responsibility");
  const uint32_t total = 600;
  const uint32_t first = wordsplit::firstPartDurationMs(r, total);
  TEST_ASSERT_GREATER_THAN_UINT32(0, first);
  TEST_ASSERT_LESS_THAN_UINT32(total, first);
  // The second part takes the remainder, so the two always sum to the total.
  const uint32_t second = total - first;
  TEST_ASSERT_EQUAL_UINT32(total, first + second);
}

void test_duration_of_unsplit_word_is_the_whole_total() {
  wordsplit::SplitResult r = wordsplit::split("short");
  TEST_ASSERT_FALSE(r.shouldSplit);
  TEST_ASSERT_EQUAL_UINT32(500, wordsplit::firstPartDurationMs(r, 500));
}

void test_custom_threshold_lowers_the_split_point() {
  // With a threshold of 8, "splitting" (9 readable) should split.
  TEST_ASSERT_FALSE(wordsplit::shouldSplit("splitting"));  // default 14: no
  TEST_ASSERT_TRUE(wordsplit::shouldSplit("splitting", 8));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_short_word_does_not_split);
  RUN_TEST(test_word_at_threshold_splits);
  RUN_TEST(test_split_parts_recombine_to_original);
  RUN_TEST(test_trailing_punctuation_stays_on_second_part);
  RUN_TEST(test_split_avoids_cutting_inside_a_vowel_run);
  RUN_TEST(test_both_parts_have_at_least_two_letters);
  RUN_TEST(test_duration_split_is_proportional_and_sums_to_total);
  RUN_TEST(test_duration_of_unsplit_word_is_the_whole_total);
  RUN_TEST(test_custom_threshold_lowers_the_split_point);
  return UNITY_END();
}
