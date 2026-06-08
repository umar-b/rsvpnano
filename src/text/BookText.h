#pragma once

#include <Arduino.h>

// Pure book-text normalization shared across the storage paths (plain-text /
// .rsvp parsing, indexed-book building, metadata reading, EPUB labels). Decodes
// UTF-8 and folds each codepoint to a byte the device font can render -- a
// custom glyph slot or an ASCII approximation -- then collapses whitespace. No
// SD, no heap policy: host-testable like LatinText.
namespace booktext {

struct ParseStats {
  size_t malformedUtf8 = 0;
  size_t nonAsciiCodepoints = 0;
  size_t longLineSplits = 0;
  bool memoryLow = false;
};

// Normalize display text. When stats is non-null, counts malformed UTF-8 and
// non-ASCII codepoints seen.
String normalizeDisplayText(const String &text, ParseStats *stats = nullptr);

}  // namespace booktext
