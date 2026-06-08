#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

#include "reader/BookContent.h"

// Pure book-text parsing shared across the storage paths (plain-text / .rsvp
// parsing, indexed-book building, metadata reading, EPUB labels). Turns raw
// file lines into reader-ready words plus chapter/paragraph markers, including
// UTF-8 normalization, tokenization, and chapter/directive detection. No SD and
// no heap policy of its own -- the one device concern, "is memory low," is
// injected -- so the whole parser is host-testable like LatinText.
namespace booktext {

struct ParseStats {
  size_t malformedUtf8 = 0;
  size_t nonAsciiCodepoints = 0;
  size_t longLineSplits = 0;
  bool memoryLow = false;
};

// Predicate the caller supplies so word pushing can bail out under memory
// pressure on the device; defaults to "never low" (e.g. on the host).
using MemoryLowFn = std::function<bool()>;

// --- Normalization -------------------------------------------------------
// Decode UTF-8 and fold each codepoint to a renderable device byte, then
// collapse whitespace. When stats is non-null, counts malformed/non-ASCII.
String normalizeDisplayText(const String &text, ParseStats *stats = nullptr);

// --- Word-count limit (RSVP_MAX_BOOK_WORDS; 0 = unlimited) ----------------
bool hasBookWordLimit();
bool reachedBookWordLimit(size_t wordCount);

// --- Character / token classifiers --------------------------------------
void trimAsciiWhitespace(String &text);
bool isWordBoundary(char c);
bool isReadableTokenChar(char c);
bool isInlineWordHyphen(const String &text, size_t index);
bool tokenHasReadableCharacter(const String &token);
bool isHyphenToken(const String &token);
bool isEllipsisToken(const String &token);
bool isStandaloneRhythmToken(const String &token);
bool prefixHasBoundary(const String &lowered, const char *prefix);

// --- Line helpers --------------------------------------------------------
String stripBom(String text);
bool chapterTitleFromLine(const String &line, String &title);
String directiveValue(const String &line, const char *directive);
void addChapterMarker(BookContent &book, const String &title);
void addParagraphMarker(BookContent &book);

// Append a normalized word to the list unless it is empty / unreadable.
// Returns false (stop) only when memoryLow() reports pressure mid-book.
bool pushCleanWord(String token, std::vector<String> &words, ParseStats *stats,
                   const MemoryLowFn &memoryLow);

// Tokenize one line into the in-memory word list.
bool appendLineWords(const String &line, std::vector<String> &words, ParseStats *stats,
                     const MemoryLowFn &memoryLow);

// Process a single book/.rsvp line, updating book words + chapter/paragraph
// markers. paragraphPending carries the blank-line state across lines.
bool processBookLine(const String &line, BookContent &book, bool &paragraphPending,
                     ParseStats *stats, const MemoryLowFn &memoryLow);
bool processRsvpLine(const String &line, BookContent &book, bool &paragraphPending,
                     ParseStats *stats, const MemoryLowFn &memoryLow);

// Split a line into tokens via pushToken (returns false to stop). Generic over
// the push target so both the in-memory and indexed-build paths share it.
template <typename PushToken, typename WordCount>
bool appendTokenizedLineWords(const String &line, PushToken pushToken, WordCount wordCount,
                              ParseStats *stats) {
  const String normalizedLine = normalizeDisplayText(line, stats);
  String currentWord;
  String pendingToken;
  currentWord.reserve(32);
  pendingToken.reserve(32);

  auto flushPending = [&]() -> bool {
    if (pendingToken.isEmpty()) {
      return true;
    }
    if (!pushToken(pendingToken)) {
      return false;
    }
    pendingToken = "";
    return !reachedBookWordLimit(wordCount());
  };

  auto finishToken = [&](String token) -> bool {
    trimAsciiWhitespace(token);
    if (token.isEmpty()) {
      return true;
    }

    if (isEllipsisToken(token)) {
      if (!pendingToken.isEmpty()) {
        pendingToken += "...";
      }
      return true;
    }

    if (isHyphenToken(token)) {
      if (!flushPending()) {
        return false;
      }
      if (!pushToken("-")) {
        return false;
      }
      return !reachedBookWordLimit(wordCount());
    }

    if (!flushPending()) {
      return false;
    }
    pendingToken = token;
    return true;
  };

  auto flushCurrent = [&]() -> bool {
    if (currentWord.isEmpty()) {
      return true;
    }
    const bool ok = finishToken(currentWord);
    currentWord = "";
    return ok;
  };

  for (size_t i = 0; i < normalizedLine.length(); ++i) {
    const char c = normalizedLine[i];
    if (isWordBoundary(c)) {
      if (!flushCurrent()) {
        return false;
      }
      continue;
    }

    if (c == '-') {
      if (isInlineWordHyphen(normalizedLine, i)) {
        currentWord += c;
        continue;
      }
      if (!flushCurrent() || !finishToken("-")) {
        return false;
      }
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '-') {
        ++i;
      }
      continue;
    }

    if (c == '.' && i + 2 < normalizedLine.length() && normalizedLine[i + 1] == '.' &&
        normalizedLine[i + 2] == '.') {
      currentWord += "...";
      i += 2;
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '.') {
        ++i;
      }
      if (!flushCurrent()) {
        return false;
      }
      continue;
    }

    currentWord += c;
  }

  if (!flushCurrent()) {
    return false;
  }

  return flushPending();
}

}  // namespace booktext
