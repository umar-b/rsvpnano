#pragma once

#include <cstdint>

// The device has no RTC or battery-backed wall clock. DeviceClock turns a single
// trusted "epoch reference" -- a known UTC epoch-seconds value captured at a
// known millis() tick -- into "epoch seconds now" by adding the elapsed millis,
// then folds in a timezone offset to produce a local calendar day key for the
// reading stats. A companion (POST /api/time with the browser's clock) or an
// opportunistic SNTP sync while on home Wi-Fi sets the reference; it is also
// snapshotted to NVS so a reboot restores an approximate-but-valid clock.
//
// The reference + math here are pure (no Arduino, no SNTP, no NVS): the caller
// supplies millis() and the trusted epoch, so day-key/tz arithmetic stays
// host-testable. The thin Arduino-facing SNTP/NVS plumbing lives in App.
//
// Staleness semantics: a restored-from-NVS reference is still treated as valid
// (so day keys survive a reboot) but is "stale" -- it was captured before the
// reboot and the device was powered off for an unknown span, so the restored
// epoch can only advance by the millis() since boot, not the real wall time off.
// Callers that care (e.g. before trusting a streak across the power-off gap)
// can inspect stale(); a fresh companion/SNTP sync clears it. Day bucketing
// itself tolerates the drift: at worst a stale clock mislabels the boot day,
// and the next sync corrects it.
namespace devclock {

// Seconds in a day, exposed for callers computing day boundaries.
constexpr int64_t kSecondsPerDay = 86400;

// "Epoch seconds now" from a reference epoch captured at refMillis, observed at
// nowMillis. Monotonic in nowMillis; tolerates millis() wraparound by using the
// unsigned difference. Returns refEpochSec when nowMillis < refMillis (clock set
// in the future of the sample, e.g. just after a sync) rather than going
// backwards.
int64_t epochNowSec(int64_t refEpochSec, uint32_t refMillis, uint32_t nowMillis);

// Local calendar day key: days since the Unix epoch in the given timezone.
// tzOffsetMinutes is minutes east of UTC (e.g. +60 for CET, -300 for EST). A
// stable, monotonic-per-calendar-day integer suitable as the ReadingStats day
// key and StatsHistory bucket id. Never returns 0 for any real post-epoch time
// (day 0 = 1970-01-01), so 0 stays reserved as "no day yet" by ReadingStats.
uint32_t localDayKey(int64_t epochSec, int32_t tzOffsetMinutes);

// True when an epoch looks like a real wall-clock time rather than the small
// "seconds since boot" SNTP returns before it has synced. Guards against
// adopting a bogus reference. 2020-01-01 UTC is the floor.
bool epochLooksValid(int64_t epochSec);

// Holds the trusted reference and timezone, and answers "now" / "today" against
// a supplied millis(). Pure: no clock source of its own.
class DeviceClock {
 public:
  DeviceClock() = default;

  // Adopt a new trusted reference: epochSec is UTC epoch seconds true as of
  // refMillis (the millis() tick when the value was observed). Rejected (no
  // change, returns false) if epochSec does not look like a real wall clock.
  // A successful set marks the clock valid and not stale.
  bool setReference(int64_t epochSec, uint32_t refMillis);

  // Restore a snapshot persisted to NVS across a reboot. Treated as valid so day
  // keys survive the reboot, but marked stale until a fresh sync. refMillis is
  // the current boot's millis() at restore time, so "now" advances only by the
  // millis() since restore (the power-off span is unknown and unrecoverable).
  void restoreSnapshot(int64_t epochSec, uint32_t refMillis);

  void setTimezoneOffsetMinutes(int32_t minutes) { tzOffsetMinutes_ = minutes; }
  int32_t timezoneOffsetMinutes() const { return tzOffsetMinutes_; }

  bool valid() const { return valid_; }
  bool stale() const { return stale_; }

  // Clear staleness, e.g. once a fresh sync confirms the reference. (setReference
  // already does this; exposed for callers that re-confirm without a new value.)
  void markFresh() { stale_ = false; }

  // Epoch seconds now, evaluated against the supplied millis(). 0 when invalid.
  int64_t epochNowSec(uint32_t nowMillis) const;

  // Local calendar day key now. Returns 0 when the clock is invalid, so callers
  // can fall back to the per-boot session key (ReadingStats treats 0 specially).
  uint32_t localDayKeyNow(uint32_t nowMillis) const;

  // The reference epoch as last set/restored (for snapshotting to NVS). The
  // companion stores epoch+tz; on the next boot restoreSnapshot replays it.
  int64_t referenceEpochSec() const { return referenceEpochSec_; }
  uint32_t referenceMillis() const { return referenceMillis_; }

 private:
  int64_t referenceEpochSec_ = 0;
  uint32_t referenceMillis_ = 0;
  int32_t tzOffsetMinutes_ = 0;
  bool valid_ = false;
  bool stale_ = false;
};

}  // namespace devclock
