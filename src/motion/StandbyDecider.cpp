#include "motion/StandbyDecider.h"

#include "motion/Orientation.h"

namespace motion {

namespace {

float absDiff(float a, float b) { return a > b ? a - b : b - a; }

bool isReading(StandbyContext c) {
  return c == StandbyContext::Playing || c == StandbyContext::Paused;
}

bool idleApplies(StandbyContext c) {
  return c == StandbyContext::Paused || c == StandbyContext::Menu;
}

}  // namespace

void StandbyDecider::noteStandbyEntered(uint32_t nowMs) {
  if (inStandby_) {
    return;
  }
  inStandby_ = true;
  enteredMs_ = nowMs;
  restedFlat_ = false;
  liftCandidate_ = false;
  clearSetDown();
}

void StandbyDecider::noteWoke(uint32_t nowMs) {
  if (!inStandby_) {
    return;
  }
  inStandby_ = false;
  lastActivityMs_ = nowMs;
  setDownArmed_ = false;
  liftCandidate_ = false;
  clearSetDown();
}

bool StandbyDecider::canWakeNow(uint32_t nowMs) const {
  if (!inStandby_) {
    return true;
  }
  return nowMs - enteredMs_ >= config_.wakeGraceMs;
}

StandbyDecider::Flat StandbyDecider::flatness(float x, float y, float z) const {
  if (orientation::isScreenDown(x, y, z, config_.faceDownZSign)) {
    return Flat::ScreenDown;
  }
  if (orientation::classify(x, y, z) == orientation::Side::FlatBack) {
    return Flat::ScreenUp;
  }
  return Flat::No;
}

StandbyVerdict StandbyDecider::enterStandby(uint32_t nowMs, bool screenOff) {
  inStandby_ = true;
  enteredMs_ = nowMs;
  // Entered by a verdict of our own: set-down means the device is flat, so a
  // lift may fire as soon as its windows allow; idle entry re-rests on the
  // first flat sample like any other in-hand entry.
  restedFlat_ = setDownCandidate_ != Flat::No;
  liftCandidate_ = false;
  clearSetDown();
  StandbyVerdict v;
  v.kind = StandbyVerdict::Kind::EnterStandby;
  v.screenOff = screenOff;
  return v;
}

StandbyVerdict StandbyDecider::idleVerdict(uint32_t nowMs, StandbyContext context) {
  if (inStandby_ || config_.idleTimeoutMs == 0 || !idleApplies(context)) {
    return StandbyVerdict{};
  }
  if (nowMs - lastActivityMs_ < config_.idleTimeoutMs) {
    return StandbyVerdict{};
  }
  return enterStandby(nowMs, false);
}

void StandbyDecider::noteContext(uint32_t nowMs, StandbyContext context) {
  // A context switch is driven by the user (or ends a busy mode); either way
  // it must not inherit a stale idle countdown.
  if (!contextValid_ || context != lastContext_) {
    contextValid_ = true;
    lastContext_ = context;
    lastActivityMs_ = nowMs;
  }
}

void StandbyDecider::clearSetDown() { setDownCandidate_ = Flat::No; }

StandbyVerdict StandbyDecider::update(uint32_t nowMs, StandbyContext context) {
  noteContext(nowMs, context);
  if (inStandby_) {
    return StandbyVerdict{};  // lift needs samples; other wakes are the caller's
  }
  return idleVerdict(nowMs, context);
}

StandbyVerdict StandbyDecider::updateWithSample(uint32_t nowMs, float x, float y, float z,
                                                StandbyContext context) {
  noteContext(nowMs, context);
  const Flat flat = flatness(x, y, z);

  if (inStandby_) {
    if (flat != Flat::No) {
      restedFlat_ = true;
      liftCandidate_ = false;
      return StandbyVerdict{};
    }
    if (!restedFlat_) {
      // Standby entered in-hand (button combo): the device was never down, so
      // there is nothing to lift. It must rest flat once first.
      return StandbyVerdict{};
    }
    if (!liftCandidate_) {
      liftCandidate_ = true;
      liftSinceMs_ = nowMs;
      return StandbyVerdict{};
    }
    if (nowMs - liftSinceMs_ >= config_.liftStableMs && canWakeNow(nowMs)) {
      inStandby_ = false;
      lastActivityMs_ = nowMs;
      liftCandidate_ = false;
      // The device is in a hand (non-flat), so set-down re-arms on this very
      // sample's evidence; keep it armed.
      setDownArmed_ = true;
      clearSetDown();
      StandbyVerdict v;
      v.kind = StandbyVerdict::Kind::Wake;
      return v;
    }
    return StandbyVerdict{};
  }

  if (flat == Flat::No) {
    setDownArmed_ = true;
    clearSetDown();
    return idleVerdict(nowMs, context);
  }

  // Flat. Set-down only arms on the reading screens, and only when armed
  // (a wake while still flat keeps it disarmed until the device is lifted).
  if (!isReading(context) || !setDownArmed_) {
    clearSetDown();
    return idleVerdict(nowMs, context);
  }

  if (flat != setDownCandidate_) {
    // New hold (or flipped face): restart the clock and the stillness ref.
    setDownCandidate_ = flat;
    setDownSinceMs_ = nowMs;
    stillRefX_ = x;
    stillRefY_ = y;
    stillRefZ_ = z;
    return idleVerdict(nowMs, context);
  }

  // Stillness: a hand, even held "still", slowly wanders. Any drift from the
  // reference re-baselines and restarts the hold, so only a table completes it.
  const float drift = absDiff(x, stillRefX_) + absDiff(y, stillRefY_) + absDiff(z, stillRefZ_);
  if (drift > config_.stillnessThresholdG) {
    setDownSinceMs_ = nowMs;
    stillRefX_ = x;
    stillRefY_ = y;
    stillRefZ_ = z;
    return idleVerdict(nowMs, context);
  }

  if (nowMs - setDownSinceMs_ >= config_.setDownHoldMs) {
    return enterStandby(nowMs, flat == Flat::ScreenDown);
  }

  return idleVerdict(nowMs, context);
}

void StandbyDecider::reset() {
  inStandby_ = false;
  enteredMs_ = 0;
  lastActivityMs_ = 0;
  setDownArmed_ = true;
  restedFlat_ = false;
  liftCandidate_ = false;
  contextValid_ = false;
  clearSetDown();
  setDownSinceMs_ = 0;
  liftSinceMs_ = 0;
  stillRefX_ = 0.0f;
  stillRefY_ = 0.0f;
  stillRefZ_ = 0.0f;
}

}  // namespace motion
