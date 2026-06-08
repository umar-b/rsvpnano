#pragma once

#include <Arduino.h>
#include <stdint.h>

// Pure reading-layout geometry for RSVP word presentation: the ORP (optimal
// recognition point) letter decision and the horizontal start-x formula that
// pins the ORP letter to the screen anchor. No glyph tables, no framebuffer --
// DisplayManager measures glyph advances and draws pixels; this module decides
// which letter is the ORP and where the word's draw origin goes. Safe to unit
// test on the host with the Arduino String shim.
namespace readinglayout {

// ORP ordinal: the 0-based position, counting only letters, of the focus letter
// in a word of the given letter count. Longer words fixate further in.
int orpOrdinal(int wordCharacterCount);

// Index into `word` of the ORP letter, skipping non-letter characters. Returns
// -1 for an empty word, 0 for a word with no letters but some characters.
int focusLetterIndex(const String &word);

// Pixel bounds of a laid-out word relative to its draw origin. DisplayManager
// fills this from glyph advances; the start-x formula consumes it.
struct WordMetrics {
  int minX = 0;
  int maxX = 0;
  int focusCenterX = 0;
  bool hasPixels = false;

  WordMetrics() = default;
  WordMetrics(int minXValue, int maxXValue, int focusCenterXValue, bool hasPixelsValue)
      : minX(minXValue),
        maxX(maxXValue),
        focusCenterX(focusCenterXValue),
        hasPixels(hasPixelsValue) {}
};

// Rendered width of the word; 0 when it has no pixels.
int layoutWidth(const WordMetrics &metrics);

// Horizontal draw origin so the word's focus-letter centre lands on the anchor
// (anchorPercent of virtualWidth), clamped to leave sideMargin on both edges.
// focusIndex < 0 centres the whole word instead. clampToMargins=false returns
// the unclamped anchored position. When the margins leave no room, the
// unclamped position is returned so the focus letter stays put.
int startX(const WordMetrics &metrics, int virtualWidth, int focusIndex,
           int anchorPercent, int sideMargin, bool clampToMargins = true);

}  // namespace readinglayout
