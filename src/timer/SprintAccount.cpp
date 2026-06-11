#include "timer/SprintAccount.h"

namespace sprint {

uint32_t netForwardWords(size_t startIndex, size_t endIndex) {
  return endIndex > startIndex ? static_cast<uint32_t>(endIndex - startIndex) : 0U;
}

void SprintAccount::beginBlock(size_t wordIndex, bool playing) {
  wordsRead_ = 0;
  segmentOpen_ = false;
  segmentStartIndex_ = wordIndex;
  blockActive_ = true;
  if (playing) {
    enterPlaying(wordIndex);
  }
}

void SprintAccount::enterPlaying(size_t wordIndex) {
  if (!blockActive_ || segmentOpen_) {
    return;
  }
  segmentStartIndex_ = wordIndex;
  segmentOpen_ = true;
}

void SprintAccount::leavePlaying(size_t wordIndex) {
  if (!segmentOpen_) {
    return;
  }
  wordsRead_ += netForwardWords(segmentStartIndex_, wordIndex);
  segmentOpen_ = false;
}

uint32_t SprintAccount::finishBlock(size_t wordIndex) {
  if (segmentOpen_) {
    leavePlaying(wordIndex);
  }
  blockActive_ = false;
  return wordsRead_;
}

}  // namespace sprint
