#include "app/TouchGesture.h"

#include <algorithm>
#include <stdlib.h>

namespace touchgesture {
namespace {

constexpr uint16_t kSwipeThresholdPx = 40;
constexpr uint16_t kAxisBiasPx = 12;
constexpr uint16_t kTapSlopPx = 26;
constexpr uint32_t kTouchPlayHoldMs = 420;
constexpr uint32_t kPreviewBrowseHoldMs = 240;
constexpr uint16_t kScrubStepPx = 22;
constexpr int kMaxScrubStepsPerGesture = 96;
constexpr uint16_t kBrowseNeutralZonePx = 14;
constexpr uint32_t kBrowseMinWordsPerSecondPermille = 4000;
constexpr uint32_t kBrowseMaxWordsPerSecondPermille = 72000;

// Reader tap-zone sizes (logical pixels).
constexpr int kFooterMetricTapWidthPx = 220;
constexpr int kFooterMetricTapHeightPx = 32;
constexpr int kBatteryBadgeTapWidthPx = 160;
constexpr int kBatteryBadgeTapHeightPx = 40;
constexpr int kPreviousSentenceTapWidthPx = 96;
constexpr int kPreviousSentenceTapHeightPx = 60;

}  // namespace

bool isTap(int absDeltaX, int absDeltaY) {
  return absDeltaX <= static_cast<int>(kTapSlopPx) && absDeltaY <= static_cast<int>(kTapSlopPx);
}

bool isHorizontalSwipe(int absDeltaX, int absDeltaY) {
  return absDeltaX >= static_cast<int>(kSwipeThresholdPx) &&
         absDeltaX > absDeltaY + static_cast<int>(kAxisBiasPx);
}

bool isVerticalSwipe(int absDeltaX, int absDeltaY) {
  return absDeltaY >= static_cast<int>(kSwipeThresholdPx) &&
         absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx);
}

bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended) {
  return !previewBrowseMode && !ended && pressDurationMs >= kTouchPlayHoldMs && tapLike;
}

ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended) {
  if (isHorizontalSwipe(absDeltaX, absDeltaY)) {
    return ReaderIntent::Scrub;
  }

  if (previewBrowseMode && !ended && pressDurationMs >= kPreviewBrowseHoldMs &&
      absDeltaY > absDeltaX + static_cast<int>(kAxisBiasPx)) {
    return ReaderIntent::BrowseScroll;
  }

  if (!previewBrowseMode && isVerticalSwipe(absDeltaX, absDeltaY)) {
    return ReaderIntent::Wpm;
  }

  return ReaderIntent::None;
}

int scrubStepsForDrag(int deltaX) {
  const int absDeltaX = abs(deltaX);
  if (absDeltaX < static_cast<int>(kSwipeThresholdPx)) {
    return 0;
  }

  int steps = 1 + ((absDeltaX - static_cast<int>(kSwipeThresholdPx)) / static_cast<int>(kScrubStepPx));
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
