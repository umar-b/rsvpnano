#pragma once

#include <stdint.h>

// Pure battery filtering + protection policy, split from the App god-object.
// App still owns the ADC sampling cadence trigger, the actual hardware read
// (BoardConfig::readBatteryStatus), and every reaction to a decision (render a
// warning, pause playback, power off, build label strings). This module owns the
// parts that are pure given a raw reading and a timestamp:
//   * the exponential filter + display hysteresis + runtime-minutes estimate, and
//   * the protection policy (critical/low classification and the consecutive-
//     critical-sample shutdown counter).
// No Arduino, no I/O, no clock of its own (time is passed in) -- host-testable
// with synthetic discharge curves and fake timestamps. Same shape as
// orientation::Stabilizer.
namespace battery {

// What the protection policy wants the caller to do this sample.
enum class Action : uint8_t {
  None = 0,
  Warn,      // low: caller may show a warning (subject to its own repeat throttle)
  Shutdown,  // critical for long enough: caller must power off
};

class Monitor {
 public:
  void reset();

  // Sampling interval the caller should wait before the next reading, given
  // whether the device is actively playing. Uses the current (pre-update) state,
  // matching the device's long-standing cadence.
  uint32_t sampleIntervalMs(bool playing) const;

  // Feed a fresh reading. `present` is false when the cell can't be read; that
  // clears presence and the critical-sample counter but leaves the last filtered
  // values intact (matching the original behaviour). `force` bypasses the
  // display hysteresis so the shown percent snaps to the filtered value.
  void update(uint32_t nowMs, bool present, float rawVoltage, uint8_t rawPercent, bool force);

  // Advance the protection state machine and report the action for this sample.
  // Increments/clears the consecutive-critical counter internally.
  Action protectionAction();

  bool present() const { return present_; }
  bool initialized() const { return initialized_; }
  float filteredVoltage() const { return filteredVoltage_; }
  uint8_t displayedPercent() const { return displayedPercent_; }
  uint32_t runtimeMinutesRemaining() const { return runtimeMinutesRemaining_; }
  bool runtimeEstimateReady() const { return runtimeEstimateReady_; }

 private:
  bool present_ = false;
  bool initialized_ = false;
  bool runtimeEstimateReady_ = false;
  float filteredVoltage_ = 0.0f;
  float filteredPercent_ = 0.0f;
  uint8_t displayedPercent_ = 0;
  uint8_t runtimeAnchorPercent_ = 0;
  uint8_t criticalSampleCount_ = 0;
  uint32_t runtimeAnchorMs_ = 0;
  uint32_t runtimeMinutesRemaining_ = 0;
};

}  // namespace battery
