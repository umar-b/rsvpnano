#include "display/BurnInJitter.h"

namespace burnin {

namespace {

// A balanced 4-corner cycle: each axis spends equal time at -1 and +1, so the
// pattern's component sums are (0, 0) and the time-average pixel position is the
// true centre. Cycling the corners (rather than jumping centre<->corner) keeps
// every step a single-pixel move, below the perception threshold.
constexpr JitterOffset kPattern[] = {
    {+1, +1},
    {-1, +1},
    {-1, -1},
    {+1, -1},
};
constexpr uint32_t kPatternLength = sizeof(kPattern) / sizeof(kPattern[0]);

}  // namespace

JitterOffset jitterOffset(uint32_t elapsedMs) {
  const uint32_t phase = (elapsedMs / kJitterStepMs) % kPatternLength;
  return kPattern[phase];
}

}  // namespace burnin
