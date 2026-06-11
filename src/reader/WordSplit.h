#pragma once

#include <Arduino.h>
#include <stdint.h>

// Long-word splitting for the RSVP reader. A word too long to sit comfortably
// inside the focus guide is shown as two sequential flashes -- a "sub-frame"
// pair -- instead of one cramped flash: the first part carries a trailing
// hyphen ("hyphen-"), the second the remainder ("ation"). This is display
// sequencing only: the word index, reading progress, scrub, and statistics all
// still treat it as one word. The two parts merely share the word's on-screen
// time slice.
//
// Pure string + timing geometry, no glyph metrics and no hardware, so it is
// safe to unit test on the host with the Arduino String shim. The reader (or
// DisplayManager) supplies the char threshold; this module decides whether to
// split, where, and how to apportion the word's duration between the parts.
namespace wordsplit {

// Default readable-character threshold at or above which a word is split. 14
// mirrors WordPacing's "ultra-long" tier (kUltraLongWordAfterChars) and the
// deepest ORP tier in ReadingLayout (orpOrdinal saturates past 13 characters),
// so a word that already strains the focus guide is exactly the one we break.
constexpr uint8_t kDefaultSplitThresholdChars = 14;

// The two flashes a split word resolves to. When `shouldSplit` is false the
// word is shown as-is and `first`/`second` are empty.
struct SplitResult {
  bool shouldSplit = false;
  String first;   // leading part with the trailing hyphen, e.g. "compli-"
  String second;  // remainder, e.g. "cation."
};

// Number of readable (letter/digit) characters, ignoring punctuation and
// quotes. The split decision keys off readable length, not raw String length,
// so a quoted or parenthesised long word still splits on its letters.
int readableCharacterCount(const String &word);

// True if `word` has at least `thresholdChars` readable characters and a usable
// interior split point exists.
bool shouldSplit(const String &word, uint8_t thresholdChars = kDefaultSplitThresholdChars);

// Split `word` into two display parts. The split lands near the middle of the
// readable span, nudged to a syllable-friendly boundary: it avoids cutting in
// the middle of a vowel run (it breaks after a vowel-to-consonant transition
// where possible) so each part reads naturally. The trailing punctuation of the
// original word stays attached to the second part; the first part gains a
// trailing '-'. When the word should not split, returns shouldSplit=false with
// the original word untouched by the caller.
SplitResult split(const String &word, uint8_t thresholdChars = kDefaultSplitThresholdChars);

// Milliseconds the FIRST flash should hold, given the word's full on-screen
// duration. The split is apportioned by the readable-character share of each
// part (so a "compli-/cation" split that is 6/5 in letters splits the time
// 6/11), clamped so neither part flashes for an imperceptible blink. The second
// part simply takes the remainder (totalDurationMs - firstPartDurationMs).
uint32_t firstPartDurationMs(const SplitResult &result, uint32_t totalDurationMs);

}  // namespace wordsplit
