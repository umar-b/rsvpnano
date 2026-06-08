#include "stats/ReadingStats.h"

namespace stats {

void ReadingStats::recordSession(uint32_t dayKey, uint32_t words, uint32_t ms) {
  if (words == 0 && ms == 0) {
    return;
  }

  // Roll the day bucket over when the caller's day key changes. The first ever
  // session (stored dayKey == 0) just adopts the incoming key.
  if (snapshot_.dayKey != dayKey) {
    snapshot_.dayKey = dayKey;
    snapshot_.dayWords = 0;
    snapshot_.dayMs = 0;
  }

  snapshot_.totalWords += words;
  snapshot_.totalMs += ms;
  snapshot_.dayWords += words;
  snapshot_.dayMs += ms;
}

uint32_t ReadingStats::averageWpm() const {
  // Need a non-trivial amount of time to avoid wild divisions on a stray ms.
  if (snapshot_.totalMs < 1000) {
    return 0;
  }
  // words per minute = words / (ms / 60000).
  const uint64_t wpm = (snapshot_.totalWords * 60000ULL) / snapshot_.totalMs;
  return static_cast<uint32_t>(wpm);
}

}  // namespace stats
