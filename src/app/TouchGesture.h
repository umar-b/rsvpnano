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
bool isHorizontalSwipe(int absDeltaX, int absDeltaY);
bool isVerticalSwipe(int absDeltaX, int absDeltaY);

// True once a still, long-enough press should start playback (press-and-hold),
// but only outside the context-preview browse mode and before the touch ends.
bool shouldEngagePlayHold(uint32_t pressDurationMs, bool tapLike, bool previewBrowseMode,
                          bool ended);

// Resolves a paused-reader drag to an intent. Precedence: horizontal scrub
// first, then a held vertical browse-scroll while the preview is up, then a
// vertical WPM swipe otherwise. None until a threshold is cleared.
ReaderIntent classifyReaderDrag(int absDeltaX, int absDeltaY, uint32_t pressDurationMs,
                                bool previewBrowseMode, bool ended);

// Words to seek for a horizontal scrub drag of deltaX pixels (signed: positive
// drags forward). Zero below the swipe threshold; clamped per gesture.
int scrubStepsForDrag(int deltaX);

// Browse-scroll speed in words/second * 1000 from a touch at y on a panel of
// the given height. Zero inside the neutral band around centre; signed by
// direction (negative above centre).
int browseScrollRatePermille(int y, int displayHeight);

// WPM step for a vertical drag: up (negative deltaY) raises, down lowers.
int wpmDeltaForDrag(int deltaY);

}  // namespace touchgesture
