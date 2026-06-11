#include "motion/FlickDetector.h"

namespace motion {

bool FlickDetector::updateWithSample(uint32_t nowMs, float x, float y, float z, bool enabled) {
  if (!enabled) {
    armed_ = true;
    return false;
  }

  const float mag2 = x * x + y * y + z * z;
  if (armed_ && mag2 >= config_.triggerG2) {
    armed_ = false;
    lastFireMs_ = nowMs;
    return true;
  }
  if (!armed_ && mag2 <= config_.rearmG2 && (nowMs - lastFireMs_) >= config_.cooldownMs) {
    armed_ = true;
  }
  return false;
}

void FlickDetector::reset() {
  armed_ = true;
  lastFireMs_ = 0;
}

}  // namespace motion
