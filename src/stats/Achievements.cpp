#include "stats/Achievements.h"

namespace stats {

bool isUnlocked(uint32_t mask, Achievement a) {
  return (mask & achievementBit(a)) != 0;
}

uint32_t qualifyingMask(const AchievementInputs &inputs) {
  uint32_t mask = 0;

  if (inputs.finishedBooks >= 1) {
    mask |= achievementBit(Achievement::FirstBookFinished);
  }
  if (inputs.finishedBooks >= 5) {
    mask |= achievementBit(Achievement::FiveBooksFinished);
  }
  if (inputs.finishedBooks >= 10) {
    mask |= achievementBit(Achievement::TenBooksFinished);
  }

  if (inputs.lifetimeWords >= 10000ULL) {
    mask |= achievementBit(Achievement::TenThousandWords);
  }
  if (inputs.lifetimeWords >= 100000ULL) {
    mask |= achievementBit(Achievement::HundredThousandWords);
  }
  if (inputs.lifetimeWords >= 1000000ULL) {
    mask |= achievementBit(Achievement::MillionWords);
  }

  // Streaks only carry calendar meaning with a valid clock; without one the
  // day keys are per-boot and not adjacent, so the streak count is not earned.
  if (inputs.clockValid) {
    if (inputs.currentStreak >= kSevenDayStreakDays) {
      mask |= achievementBit(Achievement::SevenDayStreak);
    }
    if (inputs.currentStreak >= kThirtyDayStreakDays) {
      mask |= achievementBit(Achievement::ThirtyDayStreak);
    }
  }

  // Sustained fast read: a real session (past the word floor) whose average
  // speed met the threshold.
  if (inputs.lastSessionWords >= kSpeedReaderMinWords &&
      inputs.lastSessionAvgWpm >= kSpeedReaderMinWpm) {
    mask |= achievementBit(Achievement::SpeedReader);
  }

  return mask;
}

uint32_t applyUnlocks(uint32_t &mask, const AchievementInputs &inputs) {
  const uint32_t qualifying = qualifyingMask(inputs) & kAllAchievementsMask;
  const uint32_t newly = qualifying & ~mask;
  mask |= newly;
  return newly;
}

uint8_t unlockedCount(uint32_t mask) {
  mask &= kAllAchievementsMask;
  uint8_t count = 0;
  while (mask) {
    count = static_cast<uint8_t>(count + (mask & 1U));
    mask >>= 1;
  }
  return count;
}

const char *achievementName(Achievement a) {
  switch (a) {
    case Achievement::FirstBookFinished:
      return "First book finished";
    case Achievement::FiveBooksFinished:
      return "5 books finished";
    case Achievement::TenBooksFinished:
      return "10 books finished";
    case Achievement::TenThousandWords:
      return "10k words read";
    case Achievement::HundredThousandWords:
      return "100k words read";
    case Achievement::MillionWords:
      return "1M words read";
    case Achievement::SevenDayStreak:
      return "7 day streak";
    case Achievement::ThirtyDayStreak:
      return "30 day streak";
    case Achievement::SpeedReader:
      return "Read at 600 WPM";
  }
  return "";
}

}  // namespace stats
