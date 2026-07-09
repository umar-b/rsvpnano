#include "reader/TimeEstimate.h"

#include <algorithm>
#include <cstdio>

namespace timeestimate {

uint32_t baseMs(size_t startIndex, size_t endIndex, size_t wordCount, uint32_t wpm) {
  if (wordCount == 0 || wpm == 0) {
    return 0;
  }

  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  return static_cast<uint32_t>((static_cast<uint64_t>(endIndex - startIndex) * 60000ULL) /
                               static_cast<uint64_t>(wpm));
}

String formatClock(uint16_t minutesOfDay) {
  const unsigned hours = (minutesOfDay / 60u) % 24u;
  const unsigned minutes = minutesOfDay % 60u;
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%u:%02u", hours, minutes);
  return String(buffer);
}

String formatRemaining(uint32_t remainingMs) {
  const uint32_t totalSeconds = remainingMs / 1000UL;
  if (totalSeconds < 60UL) {
    return "0m";
  }

  const uint32_t totalMinutes = totalSeconds / 60UL;
  if (totalMinutes < 60UL) {
    return String(totalMinutes) + "m";
  }

  const uint32_t totalHours = totalMinutes / 60UL;
  const uint32_t minutes = totalMinutes % 60UL;
  if (totalHours < 24UL) {
    if (minutes == 0) {
      return String(totalHours) + "h";
    }
    return String(totalHours) + "h" + String(minutes) + "m";
  }

  const uint32_t days = totalHours / 24UL;
  const uint32_t hours = totalHours % 24UL;
  if (hours == 0) {
    return String(days) + "d";
  }
  return String(days) + "d" + String(hours) + "h";
}

void PrefixCache::invalidate() {
  std::vector<uint32_t>().swap(prefixMs_);
  wordCount_ = 0;
  blockCount_ = 0;
  nextBlock_ = 0;
  runningMs_ = 0;
  valid_ = false;
  buildInProgress_ = false;
}

size_t PrefixCache::beginBuild(size_t wordCount) {
  invalidate();
  if (wordCount == 0) {
    return 0;
  }

  wordCount_ = wordCount;
  blockCount_ = (wordCount + kBlockWords - 1) / kBlockWords;
  prefixMs_.assign(blockCount_ + 1, 0);
  buildInProgress_ = true;
  return blockCount_;
}

bool PrefixCache::stepBuild(size_t maxBlocks, const BonusAt &bonusAt) {
  if (!buildInProgress_) {
    return false;
  }

  size_t processedBlocks = 0;
  while (nextBlock_ < blockCount_ && processedBlocks < maxBlocks) {
    prefixMs_[nextBlock_] = runningMs_;
    const size_t blockStart = nextBlock_ * kBlockWords;
    const size_t blockEnd = std::min(wordCount_, blockStart + kBlockWords);
    for (size_t i = blockStart; i < blockEnd; ++i) {
      runningMs_ += bonusAt(i);
    }
    ++nextBlock_;
    ++processedBlocks;
  }

  if (nextBlock_ >= blockCount_) {
    prefixMs_[blockCount_] = runningMs_;
    valid_ = true;
    buildInProgress_ = false;
    return true;
  }
  return false;
}

uint32_t PrefixCache::bonusMs(size_t startIndex, size_t endIndex, size_t wordCount,
                              const BonusAt &bonusAt) const {
  if (!valid_ || prefixMs_.empty() || endIndex <= startIndex) {
    return 0;
  }

  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  const size_t firstFullBlock = (startIndex + kBlockWords - 1) / kBlockWords;
  const size_t lastFullBlockEnd = endIndex / kBlockWords;
  uint32_t bonusMs = 0;

  if (firstFullBlock < lastFullBlockEnd && lastFullBlockEnd < prefixMs_.size()) {
    const size_t startPartialEnd = std::min(endIndex, firstFullBlock * kBlockWords);
    for (size_t i = startIndex; i < startPartialEnd; ++i) {
      bonusMs += bonusAt(i);
    }

    bonusMs += prefixMs_[lastFullBlockEnd] - prefixMs_[firstFullBlock];

    const size_t endPartialStart = lastFullBlockEnd * kBlockWords;
    for (size_t i = endPartialStart; i < endIndex; ++i) {
      bonusMs += bonusAt(i);
    }
    return bonusMs;
  }

  for (size_t i = startIndex; i < endIndex; ++i) {
    bonusMs += bonusAt(i);
  }
  return bonusMs;
}

int PrefixCache::buildProgressPercent() const {
  return static_cast<int>((nextBlock_ * 100UL) / std::max<size_t>(1, blockCount_));
}

}  // namespace timeestimate
