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

namespace {
constexpr int kW = 640;  // device logical panel size
constexpr int kH = 172;
}  // namespace

void test_footer_metric_tap_zone() {
  // Bottom-right corner: x >= W-220 (=420) and y >= H-32 (=140).
  TEST_ASSERT_TRUE(touchgesture::isFooterMetricTap(420, 140, kW, kH));   // corner of zone
  TEST_ASSERT_TRUE(touchgesture::isFooterMetricTap(639, 171, kW, kH));   // far corner
  TEST_ASSERT_FALSE(touchgesture::isFooterMetricTap(419, 140, kW, kH));  // just left
  TEST_ASSERT_FALSE(touchgesture::isFooterMetricTap(420, 139, kW, kH));  // just above
}

void test_battery_badge_tap_zone() {
  // Top-right: x >= W-160 (=480) and y <= 40.
  TEST_ASSERT_TRUE(touchgesture::isBatteryBadgeTap(480, 40, kW));
  TEST_ASSERT_TRUE(touchgesture::isBatteryBadgeTap(639, 0, kW));
  TEST_ASSERT_FALSE(touchgesture::isBatteryBadgeTap(479, 40, kW));  // just left
  TEST_ASSERT_FALSE(touchgesture::isBatteryBadgeTap(480, 41, kW));  // just below
}

void test_previous_sentence_tap_zone() {
  // Top-left corner: x < 96 and y < 60.
  TEST_ASSERT_TRUE(touchgesture::isPreviousSentenceTap(0, 0));
  TEST_ASSERT_TRUE(touchgesture::isPreviousSentenceTap(95, 59));
  TEST_ASSERT_FALSE(touchgesture::isPreviousSentenceTap(96, 59));  // at right edge (exclusive)
  TEST_ASSERT_FALSE(touchgesture::isPreviousSentenceTap(95, 60));  // at bottom edge (exclusive)
}

void test_star_sentence_tap_zone() {
  // Top-left band starting just past the previous-sentence corner: x in
  // [96,256) and y <= 40. It must never overlap the previous-sentence corner.
  TEST_ASSERT_TRUE(touchgesture::isStarSentenceTap(96, 0));
  TEST_ASSERT_TRUE(touchgesture::isStarSentenceTap(255, 40));
  TEST_ASSERT_FALSE(touchgesture::isStarSentenceTap(95, 0));    // still in prev-sentence zone
  TEST_ASSERT_FALSE(touchgesture::isStarSentenceTap(256, 40));  // right edge (exclusive)
  TEST_ASSERT_FALSE(touchgesture::isStarSentenceTap(150, 41));  // below the band
  // The two top-left zones are mutually exclusive everywhere.
  for (int x = 0; x < 300; x += 5) {
    for (int y = 0; y < 80; y += 5) {
      TEST_ASSERT_FALSE(touchgesture::isStarSentenceTap(x, y) &&
                        touchgesture::isPreviousSentenceTap(x, y));
    }
  }
}

void test_config_defaults_match_baked_in_constants() {
  // A default-constructed config must reproduce the historical behaviour the
  // no-config overloads were written against.
  touchgesture::GestureConfig def;
  TEST_ASSERT_EQUAL_UINT16(40, def.swipeThresholdPx);
  TEST_ASSERT_EQUAL_UINT16(26, def.tapSlopPx);
  TEST_ASSERT_EQUAL_UINT16(22, def.scrubStepPx);
  TEST_ASSERT_EQUAL_UINT32(420, def.playHoldMs);

  TEST_ASSERT_EQUAL(touchgesture::isTap(26, 26), touchgesture::isTap(26, 26, def));
  TEST_ASSERT_EQUAL(touchgesture::isHorizontalSwipe(40, 0),
                    touchgesture::isHorizontalSwipe(40, 0, def));
  TEST_ASSERT_EQUAL(touchgesture::scrubStepsForDrag(40 + 22),
                    touchgesture::scrubStepsForDrag(40 + 22, def));
  TEST_ASSERT_EQUAL(touchgesture::shouldEngagePlayHold(420, true, false, false),
                    touchgesture::shouldEngagePlayHold(420, true, false, false, def));
}

void test_config_changes_thresholds() {
  touchgesture::GestureConfig cfg;
  cfg.tapSlopPx = 10;
  cfg.swipeThresholdPx = 60;
  cfg.scrubStepPx = 10;
  cfg.playHoldMs = 700;

  // Tighter tap slop: 26px drift is no longer a tap.
  TEST_ASSERT_FALSE(touchgesture::isTap(20, 0, cfg));
  TEST_ASSERT_TRUE(touchgesture::isTap(10, 10, cfg));

  // Higher swipe threshold: 40px no longer a horizontal swipe.
  TEST_ASSERT_FALSE(touchgesture::isHorizontalSwipe(40, 0, cfg));
  TEST_ASSERT_TRUE(touchgesture::isHorizontalSwipe(60, 0, cfg));

  // Scrub: nothing below the (raised) threshold; first step at threshold.
  TEST_ASSERT_EQUAL_INT(0, touchgesture::scrubStepsForDrag(59, cfg));
  TEST_ASSERT_EQUAL_INT(1, touchgesture::scrubStepsForDrag(60, cfg));
  TEST_ASSERT_EQUAL_INT(2, touchgesture::scrubStepsForDrag(60 + 10, cfg));

  // Longer play-hold.
  TEST_ASSERT_FALSE(touchgesture::shouldEngagePlayHold(699, true, false, false, cfg));
  TEST_ASSERT_TRUE(touchgesture::shouldEngagePlayHold(700, true, false, false, cfg));
}

void test_clamp_gesture_config() {
  touchgesture::GestureConfig wild;
  wild.swipeThresholdPx = 5;     // below min
  wild.tapSlopPx = 5;            // below min
  wild.scrubStepPx = 0;          // below min (must clamp to >=1, no div-by-zero)
  wild.playHoldMs = 50;          // below min
  touchgesture::GestureConfig lo = touchgesture::clampGestureConfig(wild);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16(12, lo.swipeThresholdPx);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16(8, lo.tapSlopPx);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16(1, lo.scrubStepPx);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(120, lo.playHoldMs);
  // Clamped scrub step must not divide by zero.
  TEST_ASSERT_EQUAL_INT(1, touchgesture::scrubStepsForDrag(lo.swipeThresholdPx, lo));

  touchgesture::GestureConfig big;
  big.swipeThresholdPx = 9999;
  big.tapSlopPx = 9999;
  big.scrubStepPx = 9999;
  big.playHoldMs = 99999;
  touchgesture::GestureConfig hi = touchgesture::clampGestureConfig(big);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(120, hi.swipeThresholdPx);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(80, hi.tapSlopPx);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(120, hi.scrubStepPx);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1500, hi.playHoldMs);
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
  RUN_TEST(test_footer_metric_tap_zone);
  RUN_TEST(test_battery_badge_tap_zone);
  RUN_TEST(test_previous_sentence_tap_zone);
  RUN_TEST(test_star_sentence_tap_zone);
  RUN_TEST(test_config_defaults_match_baked_in_constants);
  RUN_TEST(test_config_changes_thresholds);
  RUN_TEST(test_clamp_gesture_config);
  return UNITY_END();
}
