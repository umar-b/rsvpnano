#include <unity.h>

#include "reader/WordPacing.h"

namespace {

wordpacing::PacingConfig defaultConfig() { return wordpacing::PacingConfig(); }

}  // namespace

void test_duration_is_zero_when_base_interval_is_zero() {
  TEST_ASSERT_EQUAL_UINT32(0, wordpacing::durationForWord("anything", false, 0, defaultConfig()));
}

void test_plain_short_word_gets_no_bonus() {
  const wordpacing::PacingConfig cfg = defaultConfig();
  TEST_ASSERT_EQUAL_UINT32(0, wordpacing::bonusMsForWord("the", false, cfg));
  TEST_ASSERT_EQUAL_UINT32(200, wordpacing::durationForWord("the", false, 200, cfg));
}

void test_long_word_costs_more_than_short_word() {
  const wordpacing::PacingConfig cfg = defaultConfig();
  const uint32_t shortBonus = wordpacing::bonusMsForWord("cat", false, cfg);
  const uint32_t longBonus = wordpacing::bonusMsForWord("extraordinarily", false, cfg);
  TEST_ASSERT_GREATER_THAN_UINT32(shortBonus, longBonus);
}

void test_complex_token_costs_more_than_plain_letters() {
  const wordpacing::PacingConfig cfg = defaultConfig();
  // Mixed alphanumeric is treated as more complex than a plain 4-letter word.
  const uint32_t plain = wordpacing::bonusMsForWord("word", false, cfg);
  const uint32_t mixed = wordpacing::bonusMsForWord("h2o4x", false, cfg);
  TEST_ASSERT_GREATER_THAN_UINT32(plain, mixed);
}

void test_sentence_end_pauses_longer_than_comma() {
  const wordpacing::PacingConfig cfg = defaultConfig();
  // Same stem, differing only in trailing punctuation -> isolates the pause.
  const uint32_t comma = wordpacing::bonusMsForWord("done,", false, cfg);
  const uint32_t period = wordpacing::bonusMsForWord("done.", false, cfg);
  const uint32_t bang = wordpacing::bonusMsForWord("done!", false, cfg);
  TEST_ASSERT_GREATER_THAN_UINT32(comma, period);
  TEST_ASSERT_GREATER_THAN_UINT32(period, bang);
}

void test_abbreviation_does_not_get_a_sentence_pause() {
  const wordpacing::PacingConfig cfg = defaultConfig();
  // A short token ending in '.' before a lowercase word reads as an
  // abbreviation -> no full stop.
  const uint32_t asAbbrev = wordpacing::bonusMsForWord("fits.", true, cfg);
  // The same token before a capitalised word reads as a sentence end.
  const uint32_t asSentence = wordpacing::bonusMsForWord("fits.", false, cfg);
  TEST_ASSERT_GREATER_THAN_UINT32(asAbbrev, asSentence);
}

void test_punctuation_scale_zero_floor_still_applies_some_pause() {
  // Scaling clamps at 25% on the low end, so a comma never fully disappears.
  wordpacing::PacingConfig cfg = defaultConfig();
  cfg.punctuationScalePercent = 0;
  TEST_ASSERT_GREATER_THAN_UINT32(0, wordpacing::bonusMsForWord("wait,", false, cfg));
}

void test_word_ends_sentence_classification() {
  TEST_ASSERT_TRUE(wordpacing::wordEndsSentence("end.", false));
  TEST_ASSERT_TRUE(wordpacing::wordEndsSentence("wow!", false));
  TEST_ASSERT_TRUE(wordpacing::wordEndsSentence("really?", false));
  TEST_ASSERT_FALSE(wordpacing::wordEndsSentence("middle", false));
  TEST_ASSERT_FALSE(wordpacing::wordEndsSentence("Dr.", true));
  TEST_ASSERT_TRUE(wordpacing::wordEndsSentence("fits.", false));
}

void test_starts_with_lowercase_letter() {
  TEST_ASSERT_TRUE(wordpacing::startsWithLowercaseLetter("hello"));
  TEST_ASSERT_FALSE(wordpacing::startsWithLowercaseLetter("Hello"));
  TEST_ASSERT_TRUE(wordpacing::startsWithLowercaseLetter("123abc"));
  TEST_ASSERT_FALSE(wordpacing::startsWithLowercaseLetter("123"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_duration_is_zero_when_base_interval_is_zero);
  RUN_TEST(test_plain_short_word_gets_no_bonus);
  RUN_TEST(test_long_word_costs_more_than_short_word);
  RUN_TEST(test_complex_token_costs_more_than_plain_letters);
  RUN_TEST(test_sentence_end_pauses_longer_than_comma);
  RUN_TEST(test_abbreviation_does_not_get_a_sentence_pause);
  RUN_TEST(test_punctuation_scale_zero_floor_still_applies_some_pause);
  RUN_TEST(test_word_ends_sentence_classification);
  RUN_TEST(test_starts_with_lowercase_letter);
  return UNITY_END();
}
