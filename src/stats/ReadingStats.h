#pragma once

#include <cstdint>

// Pure reading-statistics roll-up: actual words read and time spent, with an
// all-time total and a single "current day" bucket. No I/O, no clock, no
// Arduino -- the caller supplies a monotonic-ish "day key" so the module can
// stay host-testable and clock-source-agnostic.
//
// On this device there is no RTC / NTP / wall clock (confirmed: nothing in the
// firmware sets one), so the App currently buckets by power-on session -- each
// boot is a new day key. The math here is identical if a real calendar day key
// is plugged in later (e.g. once a companion sets an epoch): a changed day key
// simply rolls the day bucket over while accumulating into the all-time totals.
//
// Distinct from the reading *time estimate* (a projection from word pacing);
// this is *actuals*. See CONTEXT.md "Progress" ambiguity note.
namespace stats {

// A serialisable snapshot of the accumulators. Persistence (SD JSON + an NVS
// mirror for instant boot display) lives outside this module; it reads/writes
// these fields. Plain struct, default-initialised to a clean slate.
struct Snapshot {
  uint64_t totalWords = 0;  // all-time words read in Playing state
  uint64_t totalMs = 0;     // all-time time spent reading (Playing)
  uint32_t dayKey = 0;      // identifies the current day bucket (0 = none yet)
  uint64_t dayWords = 0;    // words read in the current day bucket
  uint64_t dayMs = 0;       // time read in the current day bucket
};

class ReadingStats {
 public:
  ReadingStats() = default;
  explicit ReadingStats(const Snapshot &snapshot) : snapshot_(snapshot) {}

  // Fold a finished reading session into the totals and the current day bucket.
  // `words` is words advanced while Playing (callers must exclude scrub/browse
  // navigation and never pass a negative -- this is decrement-safe by type).
  // A `dayKey` different from the stored one rolls the day bucket over first.
  // A zero-word, zero-ms session is a no-op.
  void recordSession(uint32_t dayKey, uint32_t words, uint32_t ms);

  // Rolling average reading speed across all-time totals, in words per minute.
  // Returns 0 when there isn't enough time recorded to be meaningful.
  uint32_t averageWpm() const;

  const Snapshot &snapshot() const { return snapshot_; }

 private:
  Snapshot snapshot_;
};

}  // namespace stats
