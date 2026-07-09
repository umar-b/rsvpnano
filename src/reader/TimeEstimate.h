#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <functional>
#include <vector>

// Pure reading-time-estimate arithmetic for the RSVP reader. The estimate is
// the base WPM time for the remaining words plus the summed word-pacing
// bonuses; the bonus sum is served from a block prefix-sum cache built
// incrementally in the background. Extracted from App, where the cache, its
// build cursor, and the partial-block arithmetic were smeared across ~10
// members. No IO, no hardware -- safe to unit test on the host with the
// Arduino String shim. Side effects (progress rendering, logging, deciding
// when to step the build) stay with the caller.
namespace timeestimate {

// Words per prefix-sum block. Cache memory is one uint32 per block.
constexpr size_t kBlockWords = 256;

// Per-word pacing bonus provider, index -> extra ms beyond the base interval.
using BonusAt = std::function<uint32_t(size_t)>;

// Base reading time for [startIndex, endIndex) at wpm, ignoring pacing.
// Indices are clamped to wordCount; 0 when the range or wpm is empty.
uint32_t baseMs(size_t startIndex, size_t endIndex, size_t wordCount, uint32_t wpm);

// "0m", "12m", "3h", "3h5m", "2d", "2d4h" -- sub-minute rounds down to "0m".
String formatRemaining(uint32_t remainingMs);

// 24h wall-clock label for minutes-since-local-midnight: "21:42", "9:05".
// Minutes past a day wrap.
String formatClock(uint16_t minutesOfDay);

// Prefix-sum cache over word-pacing bonuses. Built in blocks so the caller
// can spread the work across update ticks; once valid, bonusMs() answers
// range queries touching at most 2*kBlockWords words directly.
class PrefixCache {
 public:
  // Drops the cache and any build in progress.
  void invalidate();

  // Starts an incremental build over wordCount words. Returns the block
  // count (0 = nothing to build).
  size_t beginBuild(size_t wordCount);

  // Processes up to maxBlocks blocks, pulling bonuses from bonusAt.
  // Returns true when the build completed and the cache became valid.
  bool stepBuild(size_t maxBlocks, const BonusAt &bonusAt);

  // Sum of pacing bonuses in [startIndex, endIndex), clamped to wordCount.
  // Full blocks come from the prefix sums; the partial edges from bonusAt.
  // 0 while the cache is not valid.
  uint32_t bonusMs(size_t startIndex, size_t endIndex, size_t wordCount,
                   const BonusAt &bonusAt) const;

  bool valid() const { return valid_; }
  bool buildInProgress() const { return buildInProgress_; }
  size_t buildWordCount() const { return wordCount_; }
  size_t buildBlockCount() const { return blockCount_; }
  size_t buildNextBlock() const { return nextBlock_; }
  int buildProgressPercent() const;
  // Whole-cache bonus total; meaningful once valid.
  uint32_t totalBonusMs() const { return runningMs_; }

 private:
  std::vector<uint32_t> prefixMs_;
  size_t wordCount_ = 0;
  size_t blockCount_ = 0;
  size_t nextBlock_ = 0;
  uint32_t runningMs_ = 0;
  bool valid_ = false;
  bool buildInProgress_ = false;
};

}  // namespace timeestimate
