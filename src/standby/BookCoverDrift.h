#pragma once

#include <cstdint>

// Burn-in drift schedule for the ambient book-cover standby card. The card is a
// dim status panel (title, progress, words-read) that App draws directly; to
// protect the AMOLED it is nudged to a new position every ~30 s. This module is
// the pure, host-testable part: given an elapsed time and a travel budget, it
// produces a deterministic, slowly-cycling (dx, dy) offset. No Arduino, no
// display, no clock -- the caller supplies the elapsed milliseconds.
namespace standby {

struct DriftOffset {
  int16_t dx;
  int16_t dy;
};

// How often the card jumps to a new offset.
constexpr uint32_t kDriftIntervalMs = 30000;

// Returns the card offset for a given elapsed time. The offset changes once per
// kDriftIntervalMs and walks a fixed cycle of positions bounded by
// [-maxOffsetX, maxOffsetX] x [-maxOffsetY, maxOffsetY], so the card visits the
// corners of its allowance over time and never lingers on one pixel set. The
// `seed` lets each standby entry start the cycle at a different phase.
DriftOffset bookCoverDrift(uint32_t elapsedMs, int16_t maxOffsetX, int16_t maxOffsetY,
                           uint32_t seed = 0);

// The drift step index for a given elapsed time (how many times the card has
// jumped). Exposed so callers can detect "a new drift is due" without diffing
// offsets, and so tests can assert the cadence directly.
uint32_t bookCoverDriftStep(uint32_t elapsedMs);

}  // namespace standby
