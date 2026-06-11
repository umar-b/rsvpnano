#include "motion/TiltScrub.h"

namespace motion {

namespace {

float absf(float v) { return v < 0.0f ? -v : v; }

}  // namespace

float TiltScrub::rateForRoll(float rollG) const {
  const float mag = absf(rollG);
  if (mag <= config_.deadZoneG) {
    return 0.0f;
  }
  const float span = config_.fullScaleG - config_.deadZoneG;
  if (span <= 0.0f) {
    return config_.maxWordsPerSec;
  }
  float fraction = (mag - config_.deadZoneG) / span;
  if (fraction > 1.0f) {
    fraction = 1.0f;
  }
  return fraction * config_.maxWordsPerSec;
}

int TiltScrub::updateWithSample(float x, float y, float z, uint32_t dtMs, bool enabled) {
  (void)y;
  (void)z;
  if (!enabled) {
    reset();
    return 0;
  }

  const float rate = rateForRoll(x);
  if (rate <= 0.0f) {
    // Inside the dead zone: gesture idles. The accumulated steps are preserved
    // for the caller to read until it acts on the released gesture; only the
    // sub-word remainder is dropped so a re-entry starts at a word boundary.
    active_ = false;
    wordAccumulator_ = 0.0f;
    return steps_;
  }

  active_ = true;

  // Clamp the integration step so a hitch or the first post-enable sample never
  // injects a huge jump.
  uint32_t stepDt = dtMs;
  if (stepDt > config_.maxStepDtMs) {
    stepDt = config_.maxStepDtMs;
  }

  const float direction = (x >= 0.0f) ? static_cast<float>(config_.forwardRollSign)
                                      : -static_cast<float>(config_.forwardRollSign);
  wordAccumulator_ += direction * rate * (static_cast<float>(stepDt) / 1000.0f);

  // Move whole words out of the accumulator into the step delta.
  while (wordAccumulator_ >= 1.0f) {
    steps_ += 1;
    wordAccumulator_ -= 1.0f;
  }
  while (wordAccumulator_ <= -1.0f) {
    steps_ -= 1;
    wordAccumulator_ += 1.0f;
  }

  return steps_;
}

void TiltScrub::reset() {
  active_ = false;
  steps_ = 0;
  wordAccumulator_ = 0.0f;
}

}  // namespace motion
