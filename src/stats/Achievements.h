#pragma once

#include <cstdint>

// Pure achievement catalogue + unlock evaluation. No I/O, no clock, no Arduino:
// the App feeds in a snapshot of data it already has (lifetime words, finished
// books, streaks, the just-finished session's average WPM) and gets back which
// achievements are unlocked, expressed as a compact bitmask. Persistence (a
// single NVS u32, "ach_mask") lives in App; this module only owns the predicates
// and the stable id<->bit mapping.
//
// Stability contract: an achievement's bit position is permanent (it is what we
// persist). New achievements append at the high end; existing bits never move
// or change meaning, exactly like the FooterMetricMode / ScreensaverMode enums.
namespace stats {

// Stable bit positions. Never renumber or reuse -- the set is persisted in NVS
// as a bitmask. Append new entries before kAchievementCount.
enum class Achievement : uint8_t {
  FirstBookFinished = 0,   // finished >= 1 book
  FiveBooksFinished = 1,   // finished >= 5 books
  TenThousandWords = 2,    // >= 10k lifetime words
  HundredThousandWords = 3,  // >= 100k lifetime words
  MillionWords = 4,        // >= 1M lifetime words
  SevenDayStreak = 5,      // 7-day streak (clock-gated)
  ThirtyDayStreak = 6,     // 30-day streak (clock-gated)
  SpeedReader = 7,         // a completed session averaging >= 600 WPM
  TenBooksFinished = 8,    // finished >= 10 books
};

constexpr uint8_t kAchievementCount = 9;

// All-bits-set sentinel for "every achievement", for tests / sanity asserts.
constexpr uint32_t kAllAchievementsMask = (1UL << kAchievementCount) - 1UL;

// A snapshot of everything the predicates need, gathered by the caller at a
// natural evaluation seam (stats load, session end, book finished). All fields
// default to a clean slate so an unfilled input simply unlocks nothing.
struct AchievementInputs {
  uint64_t lifetimeWords = 0;   // all-time words read
  uint32_t finishedBooks = 0;   // count of books flagged finished
  uint16_t currentStreak = 0;   // consecutive-day streak (0 when clock invalid)
  bool clockValid = false;      // streaks only count with a real calendar clock
  // The session that just ended (0/0 when evaluating outside a session seam).
  // SpeedReader requires a *sustained* fast read: a real session (>= the word
  // floor) whose average speed reached the threshold, not a momentary WPM bump.
  uint32_t lastSessionWords = 0;
  uint32_t lastSessionAvgWpm = 0;
};

// Minimum words in a completed session for SpeedReader to count, so a two-word
// flick can't trip the "sustained 600 WPM" achievement.
constexpr uint32_t kSpeedReaderMinWords = 100;
constexpr uint32_t kSpeedReaderMinWpm = 600;

constexpr uint16_t kSevenDayStreakDays = 7;
constexpr uint16_t kThirtyDayStreakDays = 30;

// The bit for an achievement id.
constexpr uint32_t achievementBit(Achievement a) {
  return 1UL << static_cast<uint8_t>(a);
}

// True when the achievement is among the set bits of mask.
bool isUnlocked(uint32_t mask, Achievement a);

// The full set of achievements the inputs *qualify* for, ignoring history. This
// is monotonic in the data: more words / books / streak can only add bits.
uint32_t qualifyingMask(const AchievementInputs &inputs);

// Fold the qualifying set into an existing mask and report which bits are newly
// set by this evaluation (so the caller can fire one overlay + ping per unlock
// and never re-trigger an already-unlocked achievement). Returns the bits that
// transitioned 0 -> 1; `mask` is updated in place to the union.
uint32_t applyUnlocks(uint32_t &mask, const AchievementInputs &inputs);

// Number of set bits in mask (unlocked count), clamped to the catalogue size.
uint8_t unlockedCount(uint32_t mask);

// Human-readable name for the overlay / stats line. Stable, ASCII (the device
// font has no extended glyphs). Returns "" for an out-of-range id.
const char *achievementName(Achievement a);

}  // namespace stats
