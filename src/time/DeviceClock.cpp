#include "time/DeviceClock.h"

namespace devclock {

namespace {
// 2020-01-01T00:00:00Z. Anything earlier is assumed to be "seconds since boot"
// from an unsynced SNTP stack rather than a real wall clock.
constexpr int64_t kMinValidEpochSec = 1577836800;
}  // namespace

int64_t epochNowSec(int64_t refEpochSec, uint32_t refMillis, uint32_t nowMillis) {
  // Unsigned subtraction folds a single millis() wraparound naturally: across a
  // wrap, (nowMillis - refMillis) is the true small elapsed span even though
  // nowMillis < refMillis numerically. The ambiguous case is "clock set just
  // ahead of the sample" (also a small backwards numeric delta) -- there the
  // unsigned span is instead near UINT32_MAX. Disambiguate by magnitude: a span
  // larger than ~24.8 days (half the uint32 ms range) is taken as "now is
  // effectively at the reference" rather than a 49-day jump, holding the clock
  // steady instead of leaping. A genuine >24-day uptime between samples does not
  // occur on this device's save cadence.
  const uint32_t elapsedMs = nowMillis - refMillis;
  if (elapsedMs > 0x80000000U) {
    return refEpochSec;
  }
  return refEpochSec + static_cast<int64_t>(elapsedMs / 1000U);
}

uint32_t localDayKey(int64_t epochSec, int32_t tzOffsetMinutes) {
  const int64_t localSec = epochSec + static_cast<int64_t>(tzOffsetMinutes) * 60;
  if (localSec < 0) {
    return 0;
  }
  return static_cast<uint32_t>(localSec / kSecondsPerDay);
}

bool epochLooksValid(int64_t epochSec) { return epochSec >= kMinValidEpochSec; }

bool DeviceClock::setReference(int64_t epochSec, uint32_t refMillis) {
  if (!epochLooksValid(epochSec)) {
    return false;
  }
  referenceEpochSec_ = epochSec;
  referenceMillis_ = refMillis;
  valid_ = true;
  stale_ = false;
  return true;
}

void DeviceClock::restoreSnapshot(int64_t epochSec, uint32_t refMillis) {
  if (!epochLooksValid(epochSec)) {
    return;
  }
  referenceEpochSec_ = epochSec;
  referenceMillis_ = refMillis;
  valid_ = true;
  stale_ = true;
}

int64_t DeviceClock::epochNowSec(uint32_t nowMillis) const {
  if (!valid_) {
    return 0;
  }
  return devclock::epochNowSec(referenceEpochSec_, referenceMillis_, nowMillis);
}

uint32_t DeviceClock::localDayKeyNow(uint32_t nowMillis) const {
  if (!valid_) {
    return 0;
  }
  return localDayKey(epochNowSec(nowMillis), tzOffsetMinutes_);
}

}  // namespace devclock
