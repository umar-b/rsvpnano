#include <unity.h>

#include "settings/PreferenceSpec.h"

using settings::clampToRange;
using settings::inRange;
using settings::IntRange;

void test_in_range_inclusive_of_both_ends() {
  TEST_ASSERT_TRUE(inRange(10, settings::kWpmRange));
  TEST_ASSERT_TRUE(inRange(1000, settings::kWpmRange));
  TEST_ASSERT_TRUE(inRange(300, settings::kWpmRange));
}

void test_in_range_rejects_outside() {
  TEST_ASSERT_FALSE(inRange(9, settings::kWpmRange));
  TEST_ASSERT_FALSE(inRange(1001, settings::kWpmRange));
}

void test_clamp_pulls_into_domain() {
  TEST_ASSERT_EQUAL_INT(10, clampToRange(0, settings::kWpmRange));
  TEST_ASSERT_EQUAL_INT(1000, clampToRange(99999, settings::kWpmRange));
  TEST_ASSERT_EQUAL_INT(300, clampToRange(300, settings::kWpmRange));
}

void test_clamp_and_in_range_agree_on_the_same_domain() {
  // The whole point of the shared spec: a value the device would clamp is
  // exactly a value the companion would reject, because both read one range.
  const IntRange r = settings::kTypographyAnchorRange;
  for (long v = r.min - 5; v <= r.max + 5; ++v) {
    const bool accepted = inRange(v, r);
    const bool unchangedByClamp = clampToRange(v, r) == v;
    TEST_ASSERT_EQUAL(accepted, unchangedByClamp);
  }
}

void test_negative_domain_clamps_signed() {
  // tracking spans negatives; clamp must not assume unsigned.
  TEST_ASSERT_EQUAL_INT(-2, clampToRange(-10, settings::kTypographyTrackingRange));
  TEST_ASSERT_EQUAL_INT(3, clampToRange(50, settings::kTypographyTrackingRange));
  TEST_ASSERT_TRUE(inRange(-2, settings::kTypographyTrackingRange));
  TEST_ASSERT_FALSE(inRange(-3, settings::kTypographyTrackingRange));
}

void test_typography_anchor_domain_is_single_value() {
  // Guards the divergence this module exists to kill: stored anchor is 30..40.
  TEST_ASSERT_EQUAL_INT(30, settings::kTypographyAnchorRange.min);
  TEST_ASSERT_EQUAL_INT(40, settings::kTypographyAnchorRange.max);
}

void test_index_domains_start_at_zero() {
  // Array-backed index domains: 0..count-1, exclusive of count.
  TEST_ASSERT_EQUAL_INT(0, settings::kBrightnessIndexRange.min);
  TEST_ASSERT_EQUAL_INT(4, settings::kBrightnessIndexRange.max);
  TEST_ASSERT_TRUE(inRange(0, settings::kBrightnessIndexRange));
  TEST_ASSERT_FALSE(inRange(5, settings::kBrightnessIndexRange));
  TEST_ASSERT_EQUAL_INT(2, clampToRange(9, settings::kReaderFontSizeRange));
  TEST_ASSERT_FALSE(inRange(3, settings::kReaderFontSizeRange));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_index_domains_start_at_zero);
  RUN_TEST(test_in_range_inclusive_of_both_ends);
  RUN_TEST(test_in_range_rejects_outside);
  RUN_TEST(test_clamp_pulls_into_domain);
  RUN_TEST(test_clamp_and_in_range_agree_on_the_same_domain);
  RUN_TEST(test_negative_domain_clamps_signed);
  RUN_TEST(test_typography_anchor_domain_is_single_value);
  return UNITY_END();
}
