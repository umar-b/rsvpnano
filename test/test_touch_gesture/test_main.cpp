#include <unity.h>

#include "app/TouchGesture.h"

namespace {

constexpr int kHeight = 320;  // test panel height; centre = 160
int as_int(touchgesture::ReaderIntent i) { return static_cast<int>(i); }

}  // namespace

void test_is_tap_within_slop() {
  TEST_ASSERT_TRUE(touchgesture::isTap(0, 0));
  TEST_ASSERT_TRUE(touchgesture::isTap(26, 26));  // slop box edge (<=)
  TEST_ASSERT_FALSE(touchgesture::isTap(27, 0));
  TEST_ASSERT_FALSE(touchgesture::isTap(0, 27));
}

void test_horizontal_swipe_needs_threshold_and_axis_bias() {
  TEST_ASSERT_TRUE(touchgesture::isHorizontalSwipe(40, 0));   // at threshold, clears bias
  TEST_ASSERT_FALSE(touchgesture::isHorizontalSwipe(39, 0));  // below threshold
  TEST_ASSERT_FALSE(touchgesture::isHorizontalSwipe(40, 30));  // 40 !> 30+12
  TEST_ASSERT_TRUE(touchgesture::isHorizontalSwipe(60, 30));   // 60 > 42
}

void test_vertical_swipe_is_symmetric() {
  TEST_ASSERT_TRUE(touchgesture::isVerticalSwipe(0, 40));
  TEST_ASSERT_FALSE(touchgesture::isVerticalSwipe(30, 40));
  TEST_ASSERT_TRUE(touchgesture::isVerticalSwipe(30, 50));
}

void test_play_hold_conditions() {
  TEST_ASSERT_TRUE(touchgesture::shouldEngagePlayHold(420, true, false, false));
  TEST_ASSERT_FALSE(touchgesture::shouldEngagePlayHold(419, true, false, false));  // too brief
  TEST_ASSERT_FALSE(touchgesture::shouldEngagePlayHold(500, false, false, false));  // moved
  TEST_ASSERT_FALSE(touchgesture::shouldEngagePlayHold(500, true, true, false));   // in preview
  TEST_ASSERT_FALSE(touchgesture::shouldEngagePlayHold(500, true, false, true));   // already ended
}

void test_classify_reader_drag_intents() {
  // Horizontal -> Scrub.
  TEST_ASSERT_EQUAL_INT(as_int(touchgesture::ReaderIntent::Scrub),
                        as_int(touchgesture::classifyReaderDrag(50, 0, 0, false, false)));
  // Vertical outside preview -> Wpm.
  TEST_ASSERT_EQUAL_INT(as_int(touchgesture::ReaderIntent::Wpm),
                        as_int(touchgesture::classifyReaderDrag(0, 50, 0, false, false)));
  // Held vertical in preview -> BrowseScroll.
  TEST_ASSERT_EQUAL_INT(as_int(touchgesture::ReaderIntent::BrowseScroll),
                        as_int(touchgesture::classifyReaderDrag(0, 50, 240, true, false)));
  // Vertical in preview but not yet held long enough -> None (and Wpm is suppressed in preview).
  TEST_ASSERT_EQUAL_INT(as_int(touchgesture::ReaderIntent::None),
                        as_int(touchgesture::classifyReaderDrag(0, 50, 239, true, false)));
  // Nothing clears the thresholds -> None.
  TEST_ASSERT_EQUAL_INT(as_int(touchgesture::ReaderIntent::None),
                        as_int(touchgesture::classifyReaderDrag(10, 10, 0, false, false)));
}

void test_scrub_steps_for_drag() {
  TEST_ASSERT_EQUAL_INT(0, touchgesture::scrubStepsForDrag(0));
  TEST_ASSERT_EQUAL_INT(0, touchgesture::scrubStepsForDrag(39));     // below threshold
  TEST_ASSERT_EQUAL_INT(1, touchgesture::scrubStepsForDrag(40));     // first step
  TEST_ASSERT_EQUAL_INT(-1, touchgesture::scrubStepsForDrag(-40));   // signed
  TEST_ASSERT_EQUAL_INT(2, touchgesture::scrubStepsForDrag(40 + 22));  // one step beyond
  TEST_ASSERT_EQUAL_INT(96, touchgesture::scrubStepsForDrag(40 + 22 * 500));   // clamped
  TEST_ASSERT_EQUAL_INT(-96, touchgesture::scrubStepsForDrag(-(40 + 22 * 500)));
}

void test_browse_scroll_rate() {
  TEST_ASSERT_EQUAL_INT(0, touchgesture::browseScrollRatePermille(160, kHeight));  // centre
  TEST_ASSERT_EQUAL_INT(0, touchgesture::browseScrollRatePermille(174, kHeight));  // neutral edge
  TEST_ASSERT_GREATER_THAN_INT(0, touchgesture::browseScrollRatePermille(175, kHeight));  // below centre
  TEST_ASSERT_LESS_THAN_INT(0, touchgesture::browseScrollRatePermille(140, kHeight));     // above centre
  // Top of panel = full upward speed.
  TEST_ASSERT_EQUAL_INT(-72000, touchgesture::browseScrollRatePermille(0, kHeight));
}

void test_wpm_delta_direction() {
  TEST_ASSERT_EQUAL_INT(1, touchgesture::wpmDeltaForDrag(-5));   // dragged up -> faster
  TEST_ASSERT_EQUAL_INT(-1, touchgesture::wpmDeltaForDrag(5));   // dragged down -> slower
  TEST_ASSERT_EQUAL_INT(-1, touchgesture::wpmDeltaForDrag(0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_is_tap_within_slop);
  RUN_TEST(test_horizontal_swipe_needs_threshold_and_axis_bias);
  RUN_TEST(test_vertical_swipe_is_symmetric);
  RUN_TEST(test_play_hold_conditions);
  RUN_TEST(test_classify_reader_drag_intents);
  RUN_TEST(test_scrub_steps_for_drag);
  RUN_TEST(test_browse_scroll_rate);
  RUN_TEST(test_wpm_delta_direction);
  return UNITY_END();
}
