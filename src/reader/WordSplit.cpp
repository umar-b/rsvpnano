#include "reader/WordSplit.h"

#include <algorithm>

#include "text/LatinText.h"

namespace wordsplit {
namespace {

bool isLetter(char c) { return LatinText::isLetter(LatinText::byteValue(c)); }
bool isWordChar(char c) { return LatinText::isWordCharacter(LatinText::byteValue(c)); }
bool isVowel(char c) {
  return LatinText::isVowel(LatinText::toLowercaseByte(LatinText::byteValue(c)));
}

// Index just past the last readable character, so [0, end) covers the word body
// and [end, length) is trailing punctuation/quotes carried by the second part.
int lastReadableIndexExclusive(const String &word) {
  int end = static_cast<int>(word.length());
  while (end > 0 && !isWordChar(word[static_cast<size_t>(end - 1)])) {
    --end;
  }
  return end;
}

// Index of the first readable character (leading quotes/brackets precede it).
int firstReadableIndex(const String &word) {
  int start = 0;
  const int length = static_cast<int>(word.length());
  while (start < length && !isWordChar(word[static_cast<size_t>(start)])) {
    ++start;
  }
  return start;
}

// Choose the character index at which the first part ends (the cut is BEFORE
// this index). Aims for the readable midpoint, then nudges to a boundary where
// a vowel is followed by a consonant so we do not slice a vowel cluster. Keeps
// at least two readable characters on each side. Returns -1 when no interior
// cut is possible.
int chooseSplitIndex(const String &word, int readableStart, int readableEnd) {
  const int readableLen = readableEnd - readableStart;
  if (readableLen < 4) {
    return -1;
  }

  const int midpoint = readableStart + readableLen / 2;
  const int lowerBound = readableStart + 2;        // keep >=2 chars before the cut
  const int upperBound = readableEnd - 2;           // keep >=2 chars after the cut
  if (lowerBound > upperBound) {
    return -1;
  }

  int best = std::min(std::max(midpoint, lowerBound), upperBound);
  int bestDistance = std::abs(best - midpoint) + 1000;  // boundary search wins ties

  // Prefer cutting right after a vowel that is followed by a consonant, scanning
  // outward from the midpoint. The cut index sits between word[cut-1] (vowel)
  // and word[cut] (consonant).
  for (int cut = lowerBound; cut <= upperBound; ++cut) {
    const char prev = word[static_cast<size_t>(cut - 1)];
    const char here = word[static_cast<size_t>(cut)];
    if (!isLetter(prev) || !isLetter(here)) {
      continue;
    }
    if (isVowel(prev) && !isVowel(here)) {
      const int distance = std::abs(cut - midpoint);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = cut;
      }
    }
  }

  return best;
}

}  // namespace

int readableCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordChar(word[i])) {
      ++count;
    }
  }
  return count;
}

bool shouldSplit(const String &word, uint8_t thresholdChars) {
  if (readableCharacterCount(word) < static_cast<int>(thresholdChars)) {
    return false;
  }
  const int readableStart = firstReadableIndex(word);
  const int readableEnd = lastReadableIndexExclusive(word);
  return chooseSplitIndex(word, readableStart, readableEnd) >= 0;
}

SplitResult split(const String &word, uint8_t thresholdChars) {
  SplitResult result;
  if (readableCharacterCount(word) < static_cast<int>(thresholdChars)) {
    return result;
  }

  const int readableStart = firstReadableIndex(word);
  const int readableEnd = lastReadableIndexExclusive(word);
  const int cut = chooseSplitIndex(word, readableStart, readableEnd);
  if (cut < 0) {
    return result;
  }

  result.shouldSplit = true;
  result.first = word.substring(0, cut) + "-";
  result.second = word.substring(cut);
  return result;
}

uint32_t firstPartDurationMs(const SplitResult &result, uint32_t totalDurationMs) {
  if (!result.shouldSplit || totalDurationMs == 0) {
    return totalDurationMs;
  }

  const int firstChars = readableCharacterCount(result.first);    // excludes the '-'
  const int secondChars = readableCharacterCount(result.second);
  const int totalChars = firstChars + secondChars;
  if (totalChars <= 0) {
    return totalDurationMs / 2;
  }

  uint32_t firstMs = static_cast<uint32_t>(
      (static_cast<uint64_t>(totalDurationMs) * static_cast<uint64_t>(firstChars)) /
      static_cast<uint64_t>(totalChars));

  // Never let either flash drop below a perceptible minimum, while keeping the
  // sum exactly equal to the word's total duration (second part = remainder).
  constexpr uint32_t kMinFlashMs = 40;
  if (totalDurationMs > 2 * kMinFlashMs) {
    firstMs = std::max(firstMs, kMinFlashMs);
    firstMs = std::min(firstMs, totalDurationMs - kMinFlashMs);
  }
  return firstMs;
}

}  // namespace wordsplit
