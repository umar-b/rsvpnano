#pragma once

#include <cstdint>

// A rolling 30-day-bucket history of reading actuals, a sibling to ReadingStats
// (which owns the all-time totals + the single live day bucket). StatsHistory
// remembers the last `kMaxDays` distinct day buckets {dayKey, words, ms} so the
// stats screen can draw a sparkline and compute a reading streak.
//
// Pure: no I/O, no clock, no Arduino. The caller supplies the day key (real
// local calendar day from DeviceClock when valid, else the per-boot session key)
// exactly as it does for ReadingStats, so the two stay in lockstep.
//
// Streak meaning depends on a real clock. A streak is "consecutive calendar days
// with words > 0, ending today or yesterday". With per-boot session keys (clock
// invalid) the keys are not calendar-adjacent, so the streak collapses to 0 or 1
// and is not meaningful -- callers should only surface the streak when the clock
// is valid. See `currentStreak`'s contract below.
namespace stats {

// One calendar day's reading actuals.
struct DayBucket {
  uint32_t dayKey = 0;  // 0 = empty slot
  uint32_t words = 0;
  uint32_t ms = 0;
};

constexpr uint8_t kMaxDays = 30;

// Sensible on-device daily word-goal options, cycled in the stats/settings flow.
constexpr uint32_t kDailyGoalOptions[] = {1000, 2000, 5000, 10000, 20000};
constexpr uint8_t kDailyGoalOptionCount =
    static_cast<uint8_t>(sizeof(kDailyGoalOptions) / sizeof(kDailyGoalOptions[0]));
constexpr uint32_t kDefaultDailyGoal = 5000;

class StatsHistory {
 public:
  StatsHistory() = default;

  // Fold a finished session into the bucket for `dayKey`. A new dayKey opens a
  // fresh bucket (evicting the oldest when full); a repeated dayKey accumulates.
  // dayKey 0 (clock-invalid "no day yet") is ignored. Zero words AND zero ms is
  // a no-op so empty sessions never create buckets.
  void recordSession(uint32_t dayKey, uint32_t words, uint32_t ms);

  // Number of populated buckets (0..kMaxDays).
  uint8_t dayCount() const { return count_; }

  // Words read today (the bucket whose key == todayKey), 0 if none. todayKey 0
  // returns 0.
  uint32_t wordsForDay(uint32_t dayKey) const;

  // Current streak: consecutive calendar days with words > 0 ending at todayKey
  // or todayKey-1 (a streak stays alive through "today, not read yet"). Returns
  // 0 when todayKey is 0 (clock invalid) or no qualifying recent day exists.
  // Only meaningful with a valid clock, where day keys are calendar-adjacent.
  uint16_t currentStreak(uint32_t todayKey) const;

  // Longest run of consecutive-calendar-day buckets with words > 0 anywhere in
  // the retained window. Clock-invalid session keys are not adjacent, so this is
  // 0 or 1 there -- surface only with a valid clock, like currentStreak.
  uint16_t bestStreak() const;

  // Goal progress as a permille (0..1000) of `goalWords` reached today. Clamped
  // to 1000. goalWords 0 returns 1000 (a zero goal is trivially met) to avoid a
  // divide-by-zero; callers clamp the goal to a sane minimum.
  uint16_t goalProgressPermille(uint32_t todayKey, uint32_t goalWords) const;

  // True once today's words reach the goal.
  bool goalReached(uint32_t todayKey, uint32_t goalWords) const;

  // Fill `out` (length kMaxDays) with the most recent `kMaxDays` day word-counts,
  // oldest first, newest last, for a sparkline. Days with no bucket are 0. When
  // todayKey is valid (non-zero) the series is laid out on the real calendar
  // ending at todayKey, so gaps (unread days) show as 0 bars; when todayKey is 0
  // the populated buckets are packed newest-last with no calendar gaps. Returns
  // the max word-count in the series (for bar scaling); 0 when empty.
  uint32_t sparkline(uint32_t todayKey, uint32_t *out, uint8_t outLen) const;

  // Snapshot the raw ring for persistence (SD JSON authoritative). Buckets are
  // returned newest-last in calendar order of insertion via index access.
  const DayBucket &bucketAt(uint8_t index) const;

  // Append a bucket while restoring from persistence. Buckets must arrive in
  // ascending dayKey order (as written). Ignores empties and overflow past
  // kMaxDays-newest. Use during load only.
  void restoreBucket(uint32_t dayKey, uint32_t words, uint32_t ms);

 private:
  // Newest bucket sits at the highest used index; index 0 is the oldest.
  // Buckets are kept sorted ascending by dayKey because real day keys only ever
  // increase, and that ordering is what streak/sparkline scans rely on.
  DayBucket buckets_[kMaxDays];
  uint8_t count_ = 0;

  int indexOfDay(uint32_t dayKey) const;
  void pushNewDay(uint32_t dayKey, uint32_t words, uint32_t ms);
};

}  // namespace stats
