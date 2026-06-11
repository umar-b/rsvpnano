#include "stats/StatsHistory.h"

namespace stats {

int StatsHistory::indexOfDay(uint32_t dayKey) const {
  for (uint8_t i = 0; i < count_; ++i) {
    if (buckets_[i].dayKey == dayKey) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void StatsHistory::pushNewDay(uint32_t dayKey, uint32_t words, uint32_t ms) {
  if (count_ == kMaxDays) {
    // Evict the oldest (index 0) by shifting the window left.
    for (uint8_t i = 1; i < kMaxDays; ++i) {
      buckets_[i - 1] = buckets_[i];
    }
    --count_;
  }
  buckets_[count_].dayKey = dayKey;
  buckets_[count_].words = words;
  buckets_[count_].ms = ms;
  ++count_;
}

void StatsHistory::recordSession(uint32_t dayKey, uint32_t words, uint32_t ms) {
  if (dayKey == 0) {
    return;
  }
  if (words == 0 && ms == 0) {
    return;
  }
  const int existing = indexOfDay(dayKey);
  if (existing >= 0) {
    buckets_[existing].words += words;
    buckets_[existing].ms += ms;
    return;
  }
  pushNewDay(dayKey, words, ms);
}

void StatsHistory::restoreBucket(uint32_t dayKey, uint32_t words, uint32_t ms) {
  if (dayKey == 0) {
    return;
  }
  // Restore path mirrors record but keeps zero-word buckets out (they were never
  // written) and is tolerant of a repeated key by accumulating.
  const int existing = indexOfDay(dayKey);
  if (existing >= 0) {
    buckets_[existing].words += words;
    buckets_[existing].ms += ms;
    return;
  }
  if (words == 0 && ms == 0) {
    return;
  }
  pushNewDay(dayKey, words, ms);
}

uint32_t StatsHistory::wordsForDay(uint32_t dayKey) const {
  if (dayKey == 0) {
    return 0;
  }
  const int at = indexOfDay(dayKey);
  return at >= 0 ? buckets_[static_cast<uint8_t>(at)].words : 0;
}

uint16_t StatsHistory::currentStreak(uint32_t todayKey) const {
  if (todayKey == 0 || count_ == 0) {
    return 0;
  }
  // Anchor the streak at today if today has words, else yesterday (today not yet
  // read keeps an existing streak alive). If neither qualifies, streak is 0.
  uint32_t anchor = todayKey;
  if (wordsForDay(todayKey) == 0) {
    if (todayKey == 0 || wordsForDay(todayKey - 1) == 0) {
      return 0;
    }
    anchor = todayKey - 1;
  }

  uint16_t streak = 0;
  uint32_t day = anchor;
  while (true) {
    if (wordsForDay(day) == 0) {
      break;
    }
    ++streak;
    if (day == 0) {
      break;  // cannot step before day 0
    }
    --day;
  }
  return streak;
}

uint16_t StatsHistory::bestStreak() const {
  if (count_ == 0) {
    return 0;
  }
  // Buckets are ascending by dayKey. A run is consecutive calendar days with
  // words > 0 (dayKey increments by exactly 1 each step).
  uint16_t best = 0;
  uint16_t run = 0;
  uint32_t prevKey = 0;
  bool havePrev = false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (buckets_[i].words == 0) {
      run = 0;
      havePrev = false;
      continue;
    }
    if (havePrev && buckets_[i].dayKey == prevKey + 1) {
      ++run;
    } else {
      run = 1;
    }
    if (run > best) {
      best = run;
    }
    prevKey = buckets_[i].dayKey;
    havePrev = true;
  }
  return best;
}

uint16_t StatsHistory::goalProgressPermille(uint32_t todayKey, uint32_t goalWords) const {
  if (goalWords == 0) {
    return 1000;
  }
  const uint64_t words = wordsForDay(todayKey);
  const uint64_t permille = (words * 1000ULL) / goalWords;
  return permille > 1000ULL ? 1000 : static_cast<uint16_t>(permille);
}

bool StatsHistory::goalReached(uint32_t todayKey, uint32_t goalWords) const {
  return wordsForDay(todayKey) >= goalWords;
}

uint32_t StatsHistory::sparkline(uint32_t todayKey, uint32_t *out, uint8_t outLen) const {
  if (out == nullptr || outLen == 0) {
    return 0;
  }
  for (uint8_t i = 0; i < outLen; ++i) {
    out[i] = 0;
  }

  uint32_t maxWords = 0;
  if (todayKey != 0) {
    // Calendar layout: out[outLen-1] = today, walking back one day per slot, so
    // unread days render as 0-height bars.
    for (uint8_t i = 0; i < outLen; ++i) {
      // slot i counts back (outLen-1-i) days from today.
      const uint32_t back = static_cast<uint32_t>(outLen - 1 - i);
      if (back > todayKey) {
        continue;  // before the epoch / no such day
      }
      const uint32_t day = todayKey - back;
      const uint32_t words = wordsForDay(day);
      out[i] = words;
      if (words > maxWords) {
        maxWords = words;
      }
    }
    return maxWords;
  }

  // No calendar: pack the populated buckets newest-last with no gaps.
  const uint8_t take = count_ < outLen ? count_ : outLen;
  for (uint8_t i = 0; i < take; ++i) {
    // newest bucket is buckets_[count_-1]; place it last.
    const uint8_t src = static_cast<uint8_t>(count_ - take + i);
    const uint8_t dst = static_cast<uint8_t>(outLen - take + i);
    out[dst] = buckets_[src].words;
    if (buckets_[src].words > maxWords) {
      maxWords = buckets_[src].words;
    }
  }
  return maxWords;
}

const DayBucket &StatsHistory::bucketAt(uint8_t index) const {
  static const DayBucket empty;
  if (index >= count_) {
    return empty;
  }
  return buckets_[index];
}

}  // namespace stats
