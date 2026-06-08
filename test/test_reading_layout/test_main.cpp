#include <unity.h>

#include "display/ReadingLayout.h"

using readinglayout::WordMetrics;

void test_orp_ordinal_grows_with_length() {
  TEST_ASSERT_EQUAL_INT(0, readinglayout::orpOrdinal(1));
  TEST_ASSERT_EQUAL_INT(1, readinglayout::orpOrdinal(5));
  TEST_ASSERT_EQUAL_INT(2, readinglayout::orpOrdinal(9));
  TEST_ASSERT_EQUAL_INT(3, readinglayout::orpOrdinal(13));
  TEST_ASSERT_EQUAL_INT(4, readinglayout::orpOrdinal(40));
}

void test_focus_index_picks_a_letter_inside_the_word() {
  // "reading" has 7 letters -> ordinal 2 -> third letter 'a' at index 2.
  TEST_ASSERT_EQUAL_INT(2, readinglayout::focusLetterIndex("reading"));
  // Single letter -> ordinal 0.
  TEST_ASSERT_EQUAL_INT(0, readinglayout::focusLetterIndex("a"));
}

void test_focus_index_skips_leading_punctuation() {
  // Leading quote is not a word character; ordinal counts letters only, so the
  // returned index lands on a letter past the punctuation.
  const int idx = readinglayout::focusLetterIndex("\"hello\"");
  TEST_ASSERT_TRUE(idx >= 1);
  TEST_ASSERT_EQUAL_INT('e', static_cast<char>(String("\"hello\"")[idx]));
}

void test_focus_index_empty_word_is_negative() {
  TEST_ASSERT_EQUAL_INT(-1, readinglayout::focusLetterIndex(""));
}

void test_layout_width_zero_without_pixels() {
  WordMetrics m;
  TEST_ASSERT_EQUAL_INT(0, readinglayout::layoutWidth(m));
  m.hasPixels = true;
  m.minX = 4;
  m.maxX = 24;
  TEST_ASSERT_EQUAL_INT(20, readinglayout::layoutWidth(m));
}

void test_start_x_anchors_focus_centre() {
  // focus centre at 50px, anchor 35% of 200 = 70 -> origin 70 - 50 = 20.
  WordMetrics m;
  m.hasPixels = true;
  m.minX = 0;
  m.maxX = 100;
  m.focusCenterX = 50;
  const int x = readinglayout::startX(m, 200, /*focusIndex=*/3, /*anchorPercent=*/35,
                                      /*sideMargin=*/12, /*clampToMargins=*/false);
  TEST_ASSERT_EQUAL_INT(20, x);
}

void test_start_x_centres_when_focus_index_negative() {
  // No focus letter -> centre the word: (200 - 100)/2 - minX(0) = 50.
  WordMetrics m;
  m.hasPixels = true;
  m.minX = 0;
  m.maxX = 100;
  m.focusCenterX = 50;
  const int x = readinglayout::startX(m, 200, /*focusIndex=*/-1, 35, 12, true);
  TEST_ASSERT_EQUAL_INT(50, x);
}

void test_start_x_clamps_into_side_margins() {
  // A high anchor would push the word's right edge past the margin; clamp pulls
  // the origin back to virtualWidth - sideMargin - maxX.
  WordMetrics m;
  m.hasPixels = true;
  m.minX = 0;
  m.maxX = 40;
  m.focusCenterX = 0;  // anchor 90% of 200 = 180 unclamped origin
  const int x = readinglayout::startX(m, 200, /*focusIndex=*/0, /*anchorPercent=*/90,
                                      /*sideMargin=*/12, /*clampToMargins=*/true);
  TEST_ASSERT_EQUAL_INT(200 - 12 - 40, x);  // = 148
}

void test_start_x_returns_unclamped_when_no_room() {
  // Word wider than the margins allow: clamp limits invert, so the anchored
  // position is returned unchanged rather than snapping nonsensically.
  WordMetrics m;
  m.hasPixels = true;
  m.minX = 0;
  m.maxX = 190;
  m.focusCenterX = 5;
  const int x = readinglayout::startX(m, 200, 0, 35, 12, true);
  TEST_ASSERT_EQUAL_INT((200 * 35) / 100 - 5, x);  // unclamped anchor: 65
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_orp_ordinal_grows_with_length);
  RUN_TEST(test_focus_index_picks_a_letter_inside_the_word);
  RUN_TEST(test_focus_index_skips_leading_punctuation);
  RUN_TEST(test_focus_index_empty_word_is_negative);
  RUN_TEST(test_layout_width_zero_without_pixels);
  RUN_TEST(test_start_x_anchors_focus_centre);
  RUN_TEST(test_start_x_centres_when_focus_index_negative);
  RUN_TEST(test_start_x_clamps_into_side_margins);
  RUN_TEST(test_start_x_returns_unclamped_when_no_room);
  return UNITY_END();
}
