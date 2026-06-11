#include "library/MenuGesture.h"

namespace library {

namespace {
int absInt(int value) { return value < 0 ? -value : value; }
}  // namespace

bool isHold(const MenuHoldSample &sample, const MenuHoldConfig &config) {
  if (absInt(sample.deltaX) > static_cast<int>(config.maxDriftPx) ||
      absInt(sample.deltaY) > static_cast<int>(config.maxDriftPx)) {
    return false;
  }
  return sample.elapsedMs >= config.holdMs;
}

}  // namespace library
