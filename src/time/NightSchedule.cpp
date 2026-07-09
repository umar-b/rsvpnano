#include "time/NightSchedule.h"

namespace nightschedule {

namespace {
constexpr int64_t kSecondsPerDay = 86400;
}

uint16_t localMinutesOfDay(int64_t epochSec, int32_t tzOffsetMinutes) {
  int64_t localSec = epochSec + static_cast<int64_t>(tzOffsetMinutes) * 60;
  int64_t secondsOfDay = localSec % kSecondsPerDay;
  if (secondsOfDay < 0) {
    secondsOfDay += kSecondsPerDay;
  }
  return static_cast<uint16_t>(secondsOfDay / 60);
}

bool isNight(uint16_t minutesOfDay, uint16_t startMinutes, uint16_t endMinutes) {
  if (startMinutes == endMinutes) {
    return false;
  }
  if (startMinutes < endMinutes) {
    return minutesOfDay >= startMinutes && minutesOfDay < endMinutes;
  }
  return minutesOfDay >= startMinutes || minutesOfDay < endMinutes;
}

}  // namespace nightschedule
