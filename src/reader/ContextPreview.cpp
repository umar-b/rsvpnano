#include "reader/ContextPreview.h"

#include <algorithm>

namespace contextpreview {

bool isParagraphStart(const std::vector<size_t> &paragraphStarts, size_t wordIndex) {
  if (wordIndex == 0) {
    return true;
  }

  return std::binary_search(paragraphStarts.begin(), paragraphStarts.end(), wordIndex);
}

size_t paragraphStartAtOrBefore(const std::vector<size_t> &paragraphStarts, size_t wordIndex) {
  if (wordIndex == 0 || paragraphStarts.empty()) {
    return 0;
  }

  const auto it = std::upper_bound(paragraphStarts.begin(), paragraphStarts.end(), wordIndex);
  if (it == paragraphStarts.begin()) {
    return 0;
  }

  return *std::prev(it);
}

size_t anchorIndex(size_t currentIndex, const std::vector<size_t> &paragraphStarts,
                   const Config &config) {
  if (currentIndex <= config.anchorLeadWords) {
    return 0;
  }

  const size_t anchorTarget = currentIndex - config.anchorLeadWords;
  const size_t paragraphStart = paragraphStartAtOrBefore(paragraphStarts, anchorTarget);
  if (anchorTarget - paragraphStart <= config.maxParagraphSnapWords) {
    return paragraphStart;
  }

  return anchorTarget;
}

String collectBefore(size_t currentIndex, size_t charTarget, const WordAt &wordAt) {
  if (currentIndex == 0 || charTarget == 0) {
    return "";
  }

  size_t startIndex = currentIndex;
  size_t totalChars = 0;
  while (startIndex > 0 && totalChars < charTarget) {
    --startIndex;
    const String word = wordAt(startIndex);
    totalChars += word.length();
    if (startIndex + 1 < currentIndex) {
      ++totalChars;
    }
  }

  String text;
  for (size_t index = startIndex; index < currentIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += wordAt(index);
  }
  return text;
}

String collectAfter(size_t currentIndex, size_t wordCount, size_t charTarget,
                    const WordAt &wordAt) {
  if (wordCount == 0 || currentIndex + 1 >= wordCount || charTarget == 0) {
    return "";
  }

  size_t endIndex = currentIndex + 1;
  size_t totalChars = 0;
  while (endIndex < wordCount && totalChars < charTarget) {
    const String word = wordAt(endIndex);
    totalChars += word.length();
    if (endIndex > currentIndex + 1) {
      ++totalChars;
    }
    ++endIndex;
  }

  String text;
  for (size_t index = currentIndex + 1; index < endIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += wordAt(index);
  }
  return text;
}

void Window::invalidate() {
  valid_ = false;
  words_.clear();
  currentLocalIndex_ = static_cast<size_t>(-1);
}

void Window::update(size_t currentIndex, size_t wordCount,
                    const std::vector<size_t> &paragraphStarts, const WordAt &wordAt,
                    const Config &config) {
  if (wordCount == 0) {
    invalidate();
    return;
  }

  size_t startIndex = startIndex_;
  size_t endIndex = 0;
  bool rebuildWindow = !valid_ || words_.empty();
  if (!rebuildWindow) {
    endIndex = std::min(wordCount, startIndex + config.windowWords);
    rebuildWindow = currentIndex < startIndex || currentIndex >= endIndex ||
                    (currentIndex + 1 >= endIndex && endIndex < wordCount);
  }

  if (rebuildWindow) {
    startIndex = anchorIndex(currentIndex, paragraphStarts, config);
    endIndex = std::min(wordCount, startIndex + config.windowWords);
    startIndex_ = startIndex;
    valid_ = true;
    words_.clear();
    words_.reserve(endIndex - startIndex);
    for (size_t index = startIndex; index < endIndex; ++index) {
      Word word;
      word.text = wordAt(index);
      word.paragraphStart = isParagraphStart(paragraphStarts, index);
      word.current = index == currentIndex;
      words_.push_back(word);
    }
    currentLocalIndex_ =
        currentIndex >= startIndex ? currentIndex - startIndex : static_cast<size_t>(-1);
    return;
  }

  const size_t nextLocalIndex = currentIndex - startIndex;
  if (currentLocalIndex_ < words_.size()) {
    words_[currentLocalIndex_].current = false;
  }
  if (nextLocalIndex < words_.size()) {
    words_[nextLocalIndex].current = true;
    currentLocalIndex_ = nextLocalIndex;
  } else {
    currentLocalIndex_ = static_cast<size_t>(-1);
  }
}

}  // namespace contextpreview
