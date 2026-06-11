#include "standby/WordRain.h"

#include <algorithm>

#include "standby/LifeGrid.h"  // advanceRng

namespace standby {
namespace {

// A calm, reader-flavoured fallback when no book is loaded. Lower-case so the
// 5x7 tiny font (which the renderer uses) stays legible.
const char *const kFallbackWords[] = {
    "read", "word", "page", "book", "story", "focus", "flow",  "rsvp",
    "time", "think", "dream", "quiet", "ink",  "line", "phrase", "calm",
};
constexpr size_t kFallbackWordCount = sizeof(kFallbackWords) / sizeof(kFallbackWords[0]);

// How far apart (in cell columns) successive rain columns sit. Words are several
// cells wide; spacing them keeps trails from overlapping horizontally.
constexpr int kColumnPitch = 42;

}  // namespace

WordRain::WordRain(uint16_t columns, uint16_t rows) : columns_(columns), rows_(rows) {}

void WordRain::setWords(const std::vector<std::string> &words) {
  pool_.clear();
  for (const std::string &w : words) {
    if (!w.empty()) {
      pool_.push_back(w);
    }
  }
}

const std::string &WordRain::poolWord(uint8_t index) const {
  if (!pool_.empty()) {
    return pool_[index % pool_.size()];
  }
  static std::string fallback;
  fallback = kFallbackWords[index % kFallbackWordCount];
  return fallback;
}

uint8_t WordRain::pickWordIndex() {
  const size_t span = pool_.empty() ? kFallbackWordCount : pool_.size();
  return static_cast<uint8_t>((advanceRng(rng_) >> 16) % std::max<size_t>(1, span));
}

void WordRain::respawn(Column &column, bool atTop) {
  // Speed: between 0.5 and ~2.0 cell-rows per step (8..32 in 1/16 units).
  column.speed16 = static_cast<int16_t>(8 + ((advanceRng(rng_) >> 20) % 25));
  column.delay = atTop ? static_cast<int16_t>((advanceRng(rng_) >> 18) % 40) : 0;
  // Start a little above the top so the head slides in rather than popping.
  const int startRows = -((advanceRng(rng_) >> 22) % 6);
  column.headY16 = startRows * 16;
  column.wordIndex = pickWordIndex();
  for (int i = 0; i < kTrailLength; ++i) {
    column.trailWords[i] = pickWordIndex();
  }
}

void WordRain::seed(uint32_t rngSeed) {
  rng_ = rngSeed;
  generation_ = 0;
  activeColumns_.clear();

  const int usableColumns = std::max(1, static_cast<int>(columns_) - 4);
  const int count = std::max(1, usableColumns / kColumnPitch + 1);
  activeColumns_.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    Column column;
    // Spread columns across the width with a small per-column jitter.
    const int base = 2 + i * kColumnPitch;
    const int jitter = static_cast<int>((advanceRng(rng_) >> 24) % 9);
    column.x = static_cast<int16_t>(std::min(static_cast<int>(columns_) - 1, base + jitter));
    respawn(column, true);
    // Spread initial heads across the visible height so the first frame already
    // looks like rain rather than an empty grid filling from the top.
    column.delay = 0;
    column.headY16 =
        static_cast<int32_t>((advanceRng(rng_) >> 8) % std::max(1, static_cast<int>(rows_))) * 16;
    activeColumns_.push_back(column);
  }
  render();
}

void WordRain::step() {
  if (activeColumns_.empty()) {
    seed(advanceRng(rng_));
    return;
  }

  // The trail occupies kTrailLength words above the head; recycle once the
  // whole trail has cleared the bottom edge.
  const int bottomLimit = (static_cast<int>(rows_) + kTrailLength * kRowPitch) * 16;
  for (Column &column : activeColumns_) {
    if (column.delay > 0) {
      --column.delay;
      continue;
    }
    column.headY16 += column.speed16;
    if (column.headY16 > bottomLimit) {
      respawn(column, false);
    }
  }

  ++generation_;
  render();
}

void WordRain::render() {
  rendered_.clear();
  for (const Column &column : activeColumns_) {
    if (column.delay > 0) {
      continue;
    }
    const int headRow = column.headY16 / 16;
    for (int t = 0; t < kTrailLength; ++t) {
      const int row = headRow - t * kRowPitch;
      if (row < 0 || row >= static_cast<int>(rows_)) {
        continue;
      }
      // Head brightest (dim 0), trail fades out in even steps.
      const uint8_t dim = static_cast<uint8_t>(std::min(255, t * (220 / kTrailLength)));
      const uint8_t wordIdx = (t == 0) ? column.wordIndex : column.trailWords[t];
      RainWord rw;
      rw.text = poolWord(wordIdx);
      rw.x = column.x;
      rw.y = static_cast<int16_t>(row);
      rw.dim = dim;
      rendered_.push_back(rw);
    }
  }
}

}  // namespace standby
