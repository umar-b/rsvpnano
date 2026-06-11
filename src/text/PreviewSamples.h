#pragma once

#include <Arduino.h>

#include <vector>

// Typography-preview sample extraction. The typography tuning screen previews
// the reader's look on real text; when a Book is loaded we want those samples
// to be sentences from the book near the reader's position instead of canned
// strings. This module is the pure half: given a window of words (with their
// punctuation, exactly as the reader stores them) it stitches them back into
// whole sentences, bounded in length, using the same sentence-boundary
// convention as the reader's sentence rewind (wordpacing::wordEndsSentence).
// No SD, no reader, no hardware -- host-testable with the Arduino String shim.
namespace previewsamples {

struct Config {
  // Stop collecting once this many sentences are gathered.
  size_t maxSentences = 5;
  // A sentence longer than this many words is skipped (too long to preview
  // cleanly); collection continues with the next one.
  size_t maxWordsPerSentence = 14;
  // A sentence shorter than this many words is skipped (a stray fragment, an
  // abbreviation mis-split); avoids one-word "samples".
  size_t minWordsPerSentence = 2;
};

// Split a window of reader words into whole sentences. Each returned string is
// the sentence's words rejoined with single spaces, trimmed. Sentences outside
// [minWordsPerSentence, maxWordsPerSentence] are dropped. Returns at most
// Config::maxSentences strings; empty when nothing usable was found (caller
// falls back to the canned samples).
std::vector<String> extractSentences(const std::vector<String> &words, const Config &config);

}  // namespace previewsamples
