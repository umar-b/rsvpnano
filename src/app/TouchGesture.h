#pragma once

#include <stdint.h>

// Pure touch-gesture geometry for the reader, menu, and focus-timer handlers.
// All the swipe/tap thresholds and the reader drag-intent decisions live here,
// keyed only on pixel deltas and timing -- no App state, no panel, no reader.
// The handlers in App keep the per-touch tracking state and the side effects;
// they ask this module "what does this drag mean, and how much".
//
// Extracted from App, where the same threshold comparisons were copied across
// three handlers and the intent latch was tangled with the dispatch.
namespace touchgesture {

// User-tunable gesture sensitivity. NSDMI defaults reproduce the historical
// baked-in constants exactly, so a default-constructed config (and every
// no-config overload below) behaves identically to before this struct existed.
// Mirrors the wordpacing::PacingConfig / clampConfig pattern: App stores the
// values (presets on-device, raw values over the companion), clamps them, and
// threads the config through the pure functions. Only the four curated knobs
// are exposed; the browse permille, axis bias, and tap-zone sizes stay fixed.
struct GestureConfig {
  uint16_t swipeThresholdPx = 40;
  uint16_t tapSlopPx = 26;
  uint16_t scrubStepPx = 22;
  uint32_t playHoldMs = 420;
};

// Clamps each field to the range the geometry honours (sane minimums so a
// stored config can't disable a gesture entirely), so a stored config matches
// what the handlers will actually apply.
GestureConfig clampGestureConfig(const GestureConfig &config);

// What a paused-reader drag resolves to, once it clears the thresholds.
enum class ReaderIntent : uint8_t {
  None = 0,
  Scrub,        // horizontal drag -> seek by words
  BrowseScroll,  // held vertical drag in the context preview -> scroll
  Wpm,          // vertical drag -> adjust words-per-minute
};

// Shared geometry primitives. A tap stays within the slop box; a swipe clears
// the swipe threshold on its dominant axis and beats the other axis by the
// axis bias (so near-diagonal drags do not register on either axis).
bool isTap(int absDeltaX, int absDeltaY);
bool isTap(int absDeltaX, int absDeltaY, const GestureConfig &config);
bool isHorizontalSwipe(int absDeltaX, int absDeltaY);
bool isHorizontalSwipe(int absDeltaX, int absDeltaY, const GestureConfig &config);
bool isVerticalSwipe(int absDeltaX, int absDeltaY);
bool isVerticalSwipe(int absDeltaX, int absDeltaY, const GestureConfig &config);

// True once a still, long-enough press should start playback (press-and-hold),
// but only outside the context-preview browse mode and before the touch ends.
bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended);
bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended, const GestureConfig &config);

// Resolves a paused-reader drag to an intent. Precedence: horizontal scrub
// first, then a held vertical browse-scroll while the preview is up, then a
// vertical WPM swipe otherwise. None until a threshold is cleared.
ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended);
ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended, const GestureConfig &config);

// Words to seek for a horizontal scrub drag of deltaX pixels (signed: positive
// drags forward). Zero below the swipe threshold; clamped per gesture.
int scrubStepsForDrag(int deltaX);
int scrubStepsForDrag(int deltaX, const GestureConfig &config);

// Browse-scroll speed in words/second * 1000 from a touch at y on a panel of
// the given height. Zero inside the neutral band around centre; signed by
// direction (negative above centre).
int browseScrollRatePermille(int y, int displayHeight);

// WPM step for a vertical drag: up (negative deltaY) raises, down lowers.
int wpmDeltaForDrag(int deltaY);

// Reader tap zones -- pure hit-testing for the three tappable regions of the
// reading screen, keyed only on the tap coordinates and the panel size (logical
// pixels, post-rotation; App supplies BoardConfig::DISPLAY_WIDTH/HEIGHT). The
// zone sizes live inside the module. App keeps the non-geometry guards (am I
// playing, is the footer visible, is there a battery label) and the dispatch
// order; these answer only "is the point inside the zone".
bool isFooterMetricTap(int x, int y, int displayWidth, int displayHeight);  // bottom-right
bool isBatteryBadgeTap(int x, int y, int displayWidth);                     // top-right
bool isPreviousSentenceTap(int x, int y);                                   // top-left corner
// Star-the-sentence zone: a top-edge band the same width/height as the battery
// badge, but shifted inboard so it sits to the RIGHT of the previous-sentence
// corner instead of on top of it. Handedness is a full 180deg display+touch
// rotation, so these logical coords are stable in both hands; the only top-left
// affordance to avoid is the previous-sentence corner, which this clears.
bool isStarSentenceTap(int x, int y);                                       // top-left band

}  // namespace touchgesture
