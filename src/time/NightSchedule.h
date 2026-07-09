#pragma once

#include <stdint.h>

// Auto night mode's schedule arithmetic: is a given local time inside the
// night window? Pure -- App feeds it the device clock's epoch + timezone and
// flips the display theme on the day/night edges; the window itself is fixed
// (21:00-07:00) until someone asks for a configurable one.
namespace nightschedule {

constexpr uint16_t kDefaultStartMinutes = 21 * 60;  // 21:00
constexpr uint16_t kDefaultEndMinutes = 7 * 60;     // 07:00

// Minutes since local midnight (0..1439) for a UTC epoch and a timezone
// offset in minutes east of UTC. Handles pre-1970 / negative-offset wrap.
uint16_t localMinutesOfDay(int64_t epochSec, int32_t tzOffsetMinutes);

// True when minutesOfDay falls inside [start, end). A start after the end
// wraps past midnight (the 21:00-07:00 case). start == end is never night.
bool isNight(uint16_t minutesOfDay, uint16_t startMinutes = kDefaultStartMinutes,
             uint16_t endMinutes = kDefaultEndMinutes);

}  // namespace nightschedule
