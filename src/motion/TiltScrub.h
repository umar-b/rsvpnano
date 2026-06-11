#pragma once

#include <stdint.h>

// Tilt-to-scrub: while Paused in the reader, rolling the device left/right past
// a dead zone scrubs backward/forward through words, at a rate proportional to
// the roll angle. Pure -- accel samples (units of g) and dt in, an accumulated
// signed step delta out; host-testable with synthetic samples and fake dt.
// Sibling of FlickDetector and the StandbyDecider: they all read the same accel
// stream but each owns one decision. This one owns "how many words has the
// current tilt gesture scrubbed so far," which App feeds straight into the same
// applyScrubTarget() path touch scrubbing uses.
//
// Roll is read off the x-axis of gravity (g units): level reads ~0, rolling one
// way drives x toward +1 g, the other toward -1 g. Inside the dead zone the
// gesture is idle and the accumulator holds; past it, words accrue at a rate
// that grows with the roll past the dead zone, capped so a hard tilt does not
// run away. Returning inside the dead zone ends the gesture (active() goes
// false) so the caller can finish the scrub exactly like a released touch.
namespace motion {

class TiltScrub {
 public:
  struct Config {
    // Roll magnitude (g, |x| of gravity) below which nothing scrubs. Generous
    // so ordinary hand tremor and reading posture never drift the position.
    float deadZoneG = 0.18f;
    // Roll magnitude at or beyond which the rate is pinned to maxWordsPerSec --
    // the curve saturates here so a steep tilt does not scrub uncontrollably.
    float fullScaleG = 0.65f;
    // Words per second at full scale. The rate ramps linearly from 0 at the
    // dead-zone edge to this at full scale.
    float maxWordsPerSec = 12.0f;
    // Rolling one way scrubs forward, the other back. +1: x>0 scrubs forward.
    int forwardRollSign = 1;
    // Ignore dt jumps larger than this (loop hitch, first sample) so the
    // accumulator never lurches; clamps each tick's contribution.
    uint32_t maxStepDtMs = 200;
  };

  TiltScrub() {}
  explicit TiltScrub(const Config &config) : config_(config) {}

  // Feed a fresh accel sample (units of g) and the ms since the previous fed
  // sample. enabled=false (tilt-scrub off, touch down, focus timer open, in a
  // menu, mid-flick, or not Paused in the reader) holds the gesture inactive
  // and resets the accumulator so the next enable starts clean. Returns the
  // current signed step delta for this gesture (negative = backward).
  int updateWithSample(float x, float y, float z, uint32_t dtMs, bool enabled);

  // True while a tilt-scrub gesture is engaged (roll is past the dead zone).
  // Flips false the moment the roll settles back inside the dead zone -- the
  // caller treats that edge like a released touch (finish + save).
  bool active() const { return active_; }

  // The signed step delta accumulated by the current gesture so far.
  int steps() const { return steps_; }

  void reset();

 private:
  // Words/sec for a roll magnitude, 0 inside the dead zone, ramped linearly to
  // maxWordsPerSec at fullScaleG and held there beyond. Static + pure so it can
  // be unit-tested on its own.
  float rateForRoll(float rollG) const;

  Config config_;
  bool active_ = false;
  int steps_ = 0;
  // Sub-word accumulation carried between ticks so a slow tilt still advances:
  // fractional words pile up here until a whole word crosses over into steps_.
  float wordAccumulator_ = 0.0f;
};

}  // namespace motion
