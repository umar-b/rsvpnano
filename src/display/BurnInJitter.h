#pragma once

#include <stdint.h>

// AMOLED burn-in jitter for the RSVP reader. The ORP guide, word anchor, footer
// and battery label sit at fixed pixels for hours; left unmoved they can ghost
// into the panel. This shifts the entire reader frame by +/-1 px on a slow
// schedule, cycling a small offset pattern whose time-average is centred so the
// motion is imperceptible. Pure -- elapsed active-reading ms in, an (x, y)
// offset pair out; no hardware, no clock of its own, host-testable. The display
// applies the returned offset at its one render choke point; nothing else needs
// to know the layout moved.
namespace burnin {

struct JitterOffset {
  int dx = 0;
  int dy = 0;

  constexpr JitterOffset() = default;
  constexpr JitterOffset(int dxValue, int dyValue) : dx(dxValue), dy(dyValue) {}
};

// How often the offset steps to the next pattern entry, in ms of active
// reading. ~3 minutes: long enough to be invisible, short enough to spread the
// wear before a single phase can etch in.
constexpr uint32_t kJitterStepMs = 3UL * 60UL * 1000UL;

// Offset for the given elapsed active-reading time. Deterministic: the same
// elapsedMs always maps to the same offset, and the pattern's entries sum to
// (0, 0) so the long-run average pixel position is centred. Steps every
// kJitterStepMs and wraps, so it runs forever without drifting off-centre.
JitterOffset jitterOffset(uint32_t elapsedMs);

}  // namespace burnin
