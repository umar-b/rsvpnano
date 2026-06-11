#pragma once

#include <stdint.h>

// Flick rewind spike detection: a sharp shake while reading jumps the Reader
// back a sentence. Fires on an acceleration-magnitude spike, then stays
// disarmed until the motion settles below the re-arm level after a cooldown.
// Pure -- samples and clocks in, a fired/not-fired flag out; host-testable.
// Sibling of the Standby decider: they share the accel stream, not a charter.
namespace motion {

class FlickDetector {
 public:
  struct Config {
    float triggerG2 = 1.7f * 1.7f;  // |a|^2 at or above this fires (units g^2)
    float rearmG2 = 1.25f * 1.25f;  // |a|^2 at or below this re-arms
    uint32_t cooldownMs = 800;      // earliest re-arm after a fire
  };

  FlickDetector() {}
  explicit FlickDetector(const Config &config) : config_(config) {}

  // Feed a fresh accel sample (units of g). Returns true when a flick fires.
  // enabled=false (not on a reading screen) never fires and re-arms.
  bool updateWithSample(uint32_t nowMs, float x, float y, float z, bool enabled);

  void reset();

 private:
  Config config_;
  bool armed_ = true;
  uint32_t lastFireMs_ = 0;
};

}  // namespace motion
