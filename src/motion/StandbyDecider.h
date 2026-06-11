#pragma once

#include <stdint.h>

// The Standby decider: the one module that decides when the device enters and
// leaves Standby. Set-down (flat + still long enough), lift-to-wake, the wake
// grace shared by every wake path, and the idle timeout all live behind this
// interface. Pure -- accel samples (units of g) and clock ticks in, verdicts
// out; no hardware, host-testable. App executes the transitions it is handed
// and reports the ones it initiates itself (button-combo standby, any wake)
// so the mode here never desyncs. Flatness comes from the orientation
// module's single definition.
namespace motion {

// What the device is doing, as far as standby policy cares. Busy never
// standbys (boot, companion sync, USB transfer, sleeping, powering off);
// set-down arms only on the reading screens; the idle timeout runs while
// Paused or in the Menu, never while Playing.
enum class StandbyContext : uint8_t {
  Busy = 0,
  Playing,
  Paused,
  Menu,
};

struct StandbyVerdict {
  enum class Kind : uint8_t {
    None = 0,
    EnterStandby,  // caller should enter Standby now
    Wake,          // caller should leave Standby now (lift-to-wake)
  };
  Kind kind = Kind::None;
  // EnterStandby flavour: set down screen-down skips the screensaver and goes
  // straight to screen-off.
  bool screenOff = false;
};

class StandbyDecider {
 public:
  struct Config {
    uint32_t setDownHoldMs = 3000;     // flat AND still this long -> standby
    uint32_t liftStableMs = 400;       // non-flat this long in standby -> wake
    uint32_t wakeGraceMs = 900;        // ignore every wake signal this long
    float stillnessThresholdG = 0.02f; // drift beyond this re-baselines the hold
    int faceDownZSign = -1;            // board's z sign when screen-down
    uint32_t idleTimeoutMs = 0;        // 0 = idle standby off
  };

  StandbyDecider() {}
  explicit StandbyDecider(const Config &config) : config_(config) {}

  void setIdleTimeoutMs(uint32_t ms) { config_.idleTimeoutMs = ms; }

  // Any user input. Resets the idle countdown.
  void noteActivity(uint32_t nowMs) { lastActivityMs_ = nowMs; }

  // Standby entered for a reason of the caller's own (button combo). Anchors
  // the wake grace. Idempotent: re-notifying while already in standby (e.g.
  // echoing this decider's own EnterStandby verdict) changes nothing.
  void noteStandbyEntered(uint32_t nowMs);

  // Standby left for a reason of the caller's own (tap, button). Counts as
  // activity, and disarms set-down until the device leaves flat once -- a
  // wake while still on the table must not ping-pong back to standby.
  void noteWoke(uint32_t nowMs);

  bool inStandby() const { return inStandby_; }

  // The one wake-grace gate. Every wake path (tap, button, lift) consults
  // this; lift applies it internally. Always true when not in standby.
  bool canWakeNow(uint32_t nowMs) const;

  // Clock-only tick for when no accel sample is available this loop (sensor
  // absent, disabled, or between polls). Runs the idle timeout only.
  StandbyVerdict update(uint32_t nowMs, StandbyContext context);

  // Tick with a fresh accel sample. Runs set-down, lift, and idle. A Wake
  // verdict has already flipped the mode -- inStandby() is false on return,
  // and the caller's wake path echoing noteWoke() is a safe no-op.
  StandbyVerdict updateWithSample(uint32_t nowMs, float x, float y, float z,
                                  StandbyContext context);

  void reset();

 private:
  enum class Flat : uint8_t { No = 0, ScreenUp, ScreenDown };

  Flat flatness(float x, float y, float z) const;
  StandbyVerdict enterStandby(uint32_t nowMs, bool screenOff);
  StandbyVerdict idleVerdict(uint32_t nowMs, StandbyContext context);
  void noteContext(uint32_t nowMs, StandbyContext context);
  void clearSetDown();

  Config config_;
  bool inStandby_ = false;
  uint32_t enteredMs_ = 0;
  uint32_t lastActivityMs_ = 0;

  // Set-down hold: which flat flavour is being held, since when, and the
  // stillness reference it must not drift from.
  Flat setDownCandidate_ = Flat::No;
  uint32_t setDownSinceMs_ = 0;
  float stillRefX_ = 0.0f;
  float stillRefY_ = 0.0f;
  float stillRefZ_ = 0.0f;
  // Disarmed after any wake while flat; re-arms when the device leaves flat.
  bool setDownArmed_ = true;

  // Lift-to-wake: only after standby has actually seen the device flat once
  // ("rested") can a lift fire -- standby entered in-hand must not self-wake.
  bool restedFlat_ = false;
  bool liftCandidate_ = false;
  uint32_t liftSinceMs_ = 0;

  bool contextValid_ = false;
  StandbyContext lastContext_ = StandbyContext::Busy;
};

}  // namespace motion
