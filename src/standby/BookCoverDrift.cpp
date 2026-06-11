#include "standby/BookCoverDrift.h"

namespace standby {
namespace {

// A small fixed cycle of unit offsets (eight points around a ring plus centre).
// The card walks this ring one step per interval; scaled by the caller's
// allowance it spreads wear across the whole budget rectangle.
struct UnitOffset {
  int8_t x;
  int8_t y;
};
constexpr UnitOffset kRing[] = {
    {0, 0},   {1, -1}, {1, 0},  {1, 1},  {0, 1},
    {-1, 1},  {-1, 0}, {-1, -1}, {0, -1},
};
constexpr uint32_t kRingCount = sizeof(kRing) / sizeof(kRing[0]);

}  // namespace

uint32_t bookCoverDriftStep(uint32_t elapsedMs) { return elapsedMs / kDriftIntervalMs; }

DriftOffset bookCoverDrift(uint32_t elapsedMs, int16_t maxOffsetX, int16_t maxOffsetY,
                           uint32_t seed) {
  const uint32_t step = bookCoverDriftStep(elapsedMs) + seed;
  const UnitOffset unit = kRing[step % kRingCount];
  DriftOffset out;
  out.dx = static_cast<int16_t>(unit.x * maxOffsetX);
  out.dy = static_cast<int16_t>(unit.y * maxOffsetY);
  return out;
}

}  // namespace standby
