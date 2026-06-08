#include "display/ReadingLayout.h"

#include <algorithm>

#include "text/LatinText.h"

namespace readinglayout {
namespace {

bool isWordCharacter(char c) { return LatinText::isWordCharacter(LatinText::byteValue(c)); }

}  // namespace

int orpOrdinal(int wordCharacterCount) {
  if (wordCharacterCount <= 1) {
    return 0;
  }
  if (wordCharacterCount <= 5) {
    return 1;
  }
  if (wordCharacterCount <= 9) {
    return 2;
  }
  if (wordCharacterCount <= 13) {
    return 3;
  }
  return 4;
}

int focusLetterIndex(const String &word) {
  int wordCharacterCount = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordCharacter(word[i])) {
      ++wordCharacterCount;
    }
  }

  if (wordCharacterCount == 0) {
    return word.length() > 0 ? 0 : -1;
  }

  const int targetOrdinal = std::min(orpOrdinal(wordCharacterCount), wordCharacterCount - 1);
  int currentOrdinal = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (!isWordCharacter(word[i])) {
      continue;
    }
    if (currentOrdinal == targetOrdinal) {
      return static_cast<int>(i);
    }
    ++currentOrdinal;
  }

  return 0;
}

int layoutWidth(const WordMetrics &metrics) {
  if (!metrics.hasPixels) {
    return 0;
  }
  return std::max(0, metrics.maxX - metrics.minX);
}

int startX(const WordMetrics &metrics, int virtualWidth, int focusIndex, int anchorPercent,
           int sideMargin, bool clampToMargins) {
  const int wordWidth = layoutWidth(metrics);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - metrics.minX;
  }

  const int anchorX = (virtualWidth * anchorPercent) / 100;
  const int x = anchorX - metrics.focusCenterX;
  if (!clampToMargins) {
    return x;
  }

  const int minX = sideMargin - metrics.minX;
  const int maxX = virtualWidth - sideMargin - metrics.maxX;
  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

}  // namespace readinglayout
