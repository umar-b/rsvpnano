#pragma once

// MenuGesture -- pure, host-testable detection of a menu touch-HOLD.
//
// App.cpp owns the raw TouchEvent plumbing, the existing tap/vertical-swipe
// dispatch (touchgesture::isTap / isVerticalSwipe), and rendering. This module
// adds only the one new decision the library features need: "has this press
// been held still long enough to be a hold?" -- so it can be unit-tested
// without Arduino. A hold is a long press with little drift; it deliberately
// out-ranks a tap (a still finger held past the threshold is a hold, not a
// tap), matching the focus-timer cancel-hold already in App.

#include <cstdint>

namespace library {

struct MenuHoldConfig {
  uint16_t maxDriftPx = 26;  // finger must stay within this box to count as held
  uint32_t holdMs = 550;     // min press duration (with little drift) for a hold
};

// Inputs describing the in-progress (or completed) press, measured from the
// touch Start: signed drift on each axis and elapsed time.
struct MenuHoldSample {
  int deltaX = 0;          // currentX - startX
  int deltaY = 0;          // currentY - startY
  uint32_t elapsedMs = 0;  // now - startMs
};

// True when the press has stayed within maxDriftPx on both axes for at least
// holdMs. Usable while the press is still active (fire the action menu the
// moment the threshold is crossed) and after it ends.
bool isHold(const MenuHoldSample &sample, const MenuHoldConfig &config);

}  // namespace library
