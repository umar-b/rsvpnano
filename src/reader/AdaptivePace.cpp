#include "reader/AdaptivePace.h"

namespace adaptivepace {

void Decider::reset() {
  hasPendingRewind_ = false;
  lastRewindMs_ = 0;
  lastQuietMarkMs_ = 0;
  steps_ = 0;
}

bool Decider::noteRewind(uint32_t nowMs) {
  const bool burst = hasPendingRewind_ && (nowMs - lastRewindMs_) <= config_.windowMs;
  lastRewindMs_ = nowMs;
  lastQuietMarkMs_ = nowMs;

  if (!burst) {
    hasPendingRewind_ = true;
    return false;
  }

  // A burst consumed this pair; the next step needs a fresh pair.
  hasPendingRewind_ = false;
  if (steps_ >= config_.maxSteps) {
    return false;
  }
  ++steps_;
  return true;
}

bool Decider::update(uint32_t nowMs) {
  if (steps_ == 0) {
    return false;
  }
  if (nowMs - lastQuietMarkMs_ < config_.recoverAfterMs) {
    return false;
  }
  --steps_;
  lastQuietMarkMs_ = nowMs;
  return true;
}

}  // namespace adaptivepace
