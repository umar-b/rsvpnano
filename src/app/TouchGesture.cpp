#include "app/TouchGesture.h"

#include <algorithm>
#include <stdlib.h>

namespace touchgesture {
namespace {

// Fixed geometry constants. The four user-tunable knobs (swipe threshold, tap
// slop, scrub step, play-hold) now live in GestureConfig; these remain baked in
// (decision 2: curate to four knobs; browse permille + axis bias stay fixed).
constexpr uint16_t kAxisBiasPx = 12;
constexpr uint32_t kPreviewBrowseHoldMs = 240;
constexpr int kMaxScrubStepsPerGesture = 96;
constexpr uint16_t kBrowseNeutralZonePx = 14;
constexpr uint32_t kBrowseMinWordsPerSecondPermille = 4000;
constexpr uint32_t kBrowseMaxWordsPerSecondPermille = 72000;

// Clamp bounds for the tunable knobs. Minimums keep every gesture usable; the
// scrub step floor of 1 avoids a divide-by-zero in scrubStepsForDrag.
constexpr uint16_t kSwipeThresholdMinPx = 12;
constexpr uint16_t kSwipeThresholdMaxPx = 120;
constexpr uint16_t kTapSlopMinPx = 8;
constexpr uint16_t kTapSlopMaxPx = 80;
constexpr uint16_t kScrubStepMinPx = 1;
constexpr uint16_t kScrubStepMaxPx = 120;
constexpr uint32_t kPlayHoldMinMs = 120;
constexpr uint32_t kPlayHoldMaxMs = 1500;

uint16_t clampU16(uint16_t value, uint16_t lo, uint16_t hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

uint32_t clampU32(uint32_t value, uint32_t lo, uint32_t hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

// Reader tap-zone sizes (logical pixels).
constexpr int kFooterMetricTapWidthPx = 220;
constexpr int kFooterMetricTapHeightPx = 32;
constexpr int kBatteryBadgeTapWidthPx = 160;
constexpr int kBatteryBadgeTapHeightPx = 40;
constexpr int kPreviousSentenceTapWidthPx = 96;
constexpr int kPreviousSentenceTapHeightPx = 60;

}  // namespace

GestureConfig clampGestureConfig(const GestureConfig &config) {
  GestureConfig clamped;
  clamped.swipeThresholdPx =
      clampU16(config.swipeThresholdPx, kSwipeThresholdMinPx, kSwipeThresholdMaxPx);
  clamped.tapSlopPx = clampU16(config.tapSlopPx, kTapSlopMinPx, kTapSlopMaxPx);
  clamped.scrubStepPx = clampU16(config.scrubStepPx, kScrubStepMinPx, kScrubStepMaxPx);
  clamped.playHoldMs = clampU32(config.playHoldMs, kPlayHoldMinMs, kPlayHoldMaxMs);
  return clamped;
}

bool isTap(int absDeltaX, int absDeltaY) { return isTap(absDeltaX, absDeltaY, GestureConfig()); }

bool isTap(int absDeltaX, int absDeltaY, const GestureConfig &config) {
  return absDeltaX <= static_cast<int>(config.tapSlopPx) &&
         absDeltaY <= static_cast<int>(config.tapSlopPx);
}

bool isHorizontalSwipe(int absDeltaX, int absDeltaY) {
  return isHorizontalSwipe(absDeltaX, absDeltaY, GestureConfig());
}

bool isHorizontalSwipe(int absDeltaX, int absDeltaY, const GestureConfig &config) {
  return absDeltaX >= static_cast<int>(config.swipeThresholdPx) &&
         absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx);
}

bool isVerticalSwipe(int absDeltaX, int absDeltaY) {
  return isVerticalSwipe(absDeltaX, absDeltaY, GestureConfig());
}

bool isVerticalSwipe(int absDeltaX, int absDeltaY, const GestureConfig &config) {
  return absDeltaY >= static_cast<int>(config.swipeThresholdPx) &&
         absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx);
}

bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended) {
  return shouldEngagePlayHold(pressDurationMs, tapLike, previewBrowseMode, ended, GestureConfig());
}

bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended, const GestureConfig &config) {
  return !previewBrowseMode && !ended && pressDurationMs >= config.playHoldMs && tapLike;
}

ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended) {
  return classifyReaderDrag(absDeltaX, absDeltaY, pressDurationMs, previewBrowseMode, ended,
                            GestureConfig());
}

ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended, const GestureConfig &config) {
  if (isHorizontalSwipe(absDeltaX, absDeltaY, config)) {
    return ReaderIntent::Scrub;
  }

  if (previewBrowseMode && !ended && pressDurationMs >= kPreviewBrowseHoldMs &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
    return ReaderIntent::BrowseScroll;
  }

  if (!previewBrowseMode && isVerticalSwipe(absDeltaX, absDeltaY, config)) {
    return ReaderIntent::Wpm;
  }

  return ReaderIntent::None;
}

int scrubStepsForDrag(int deltaX) { return scrubStepsForDrag(deltaX, GestureConfig()); }

int scrubStepsForDrag(int deltaX, const GestureConfig &config) {
  const int absDeltaX = abs(deltaX);
  if (absDeltaX < static_cast<int>(config.swipeThresholdPx)) {
    return 0;
  }

  int steps = 1 + ((absDeltaX - static_cast<int>(config.swipeThresholdPx)) /
                   static_cast<int>(config.scrubStepPx));
  steps = std::min(steps, kMaxScrubStepsPerGesture);

  return (deltaX > 0) ? steps : -steps;
}

int browseScrollRatePermille(int y, int displayHeight) {
  const int centerY = displayHeight / 2;
  const int signedDistance = y - centerY;
  const int absDistance = abs(signedDistance);
  if (absDistance <= static_cast<int>(kBrowseNeutralZonePx)) {
    return 0;
  }

  const int activeRange = std::max(1, centerY - static_cast<int>(kBrowseNeutralZonePx));
  const int activeDistance =
      std::min(activeRange, absDistance - static_cast<int>(kBrowseNeutralZonePx));
  const uint32_t speedPermille =
      kBrowseMinWordsPerSecondPermille +
      ((kBrowseMaxWordsPerSecondPermille - kBrowseMinWordsPerSecondPermille) *
       static_cast<uint32_t>(activeDistance)) /
          static_cast<uint32_t>(activeRange);

  return signedDistance < 0 ? -static_cast<int>(speedPermille) : static_cast<int>(speedPermille);
}

int wpmDeltaForDrag(int deltaY) { return (deltaY < 0) ? 1 : -1; }

bool isFooterMetricTap(int x, int y, int displayWidth, int displayHeight) {
  return x >= displayWidth - kFooterMetricTapWidthPx &&
         y >= displayHeight - kFooterMetricTapHeightPx;
}

bool isBatteryBadgeTap(int x, int y, int displayWidth) {
  return x >= displayWidth - kBatteryBadgeTapWidthPx && y <= kBatteryBadgeTapHeightPx;
}

bool isPreviousSentenceTap(int x, int y) {
  return x < kPreviousSentenceTapWidthPx && y < kPreviousSentenceTapHeightPx;
}

}  // namespace touchgesture
