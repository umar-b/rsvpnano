#pragma once

#include <stdint.h>

// Adaptive pacing: repeated sentence rewinds in a short window mean the
// reader is losing the thread, so ease the effective speed down a step and
// creep back after a quiet spell. Pure decider -- App reports rewinds and
// ticks update(); the resulting interval scale is folded into ReadingLoop
// the same way ramp-in is. The set WPM (display, persistence, estimates)
// never changes.
namespace adaptivepace {

struct Config {
  // A second rewind within windowMs of the previous one triggers an ease step.
  uint32_t windowMs = 60000;
  // Each step stretches the word interval by this much (permille of base).
  uint16_t easeStepPermille = 100;  // 10% slower per step
  uint8_t maxSteps = 3;             // floor at 30% slower
  // Quiet time (no rewinds) before easing back one step.
  uint32_t recoverAfterMs = 180000;
};

class Decider {
 public:
  explicit Decider(const Config &config = Config()) : config_(config) {}

  void reset();
  // A sentence rewind happened. Returns true when the scale changed.
  bool noteRewind(uint32_t nowMs);
  // Periodic tick for recovery. Returns true when the scale changed.
  bool update(uint32_t nowMs);

  uint8_t steps() const { return steps_; }
  // Interval multiplier: 1000 = normal, 1100 = 10% slower.
  uint16_t scalePermille() const {
    return static_cast<uint16_t>(1000 + static_cast<uint16_t>(steps_) * config_.easeStepPermille);
  }

 private:
  Config config_;
  bool hasPendingRewind_ = false;
  uint32_t lastRewindMs_ = 0;
  uint32_t lastQuietMarkMs_ = 0;
  uint8_t steps_ = 0;
};

}  // namespace adaptivepace
