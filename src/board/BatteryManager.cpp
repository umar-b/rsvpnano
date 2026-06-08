#include "board/BatteryManager.h"

#include <algorithm>
#include <cstdlib>

namespace battery {
namespace {

// Sampling cadence.
constexpr uint32_t kSampleIntervalMs = 180000;
constexpr uint32_t kPlayingSampleIntervalMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kLowSampleIntervalMs = 60UL * 1000UL;

// Display smoothing + runtime estimate.
constexpr uint8_t kDisplayHysteresisPercent = 2;
constexpr uint8_t kRuntimeMinDropPercent = 3;
constexpr uint32_t kRuntimeMinElapsedMs = 10UL * 60UL * 1000UL;

// Protection thresholds.
constexpr float kLowWarningVoltage = 3.50f;
constexpr float kCriticalVoltage = 3.30f;
constexpr uint8_t kLowWarningPercent = 5;
constexpr uint8_t kCriticalPercent = 1;
constexpr uint8_t kCriticalConsecutiveSamples = 2;

}  // namespace

void Monitor::reset() { *this = Monitor(); }

uint32_t Monitor::sampleIntervalMs(bool playing) const {
  const bool lowBatteryKnown =
      present_ && initialized_ &&
      (filteredVoltage_ <= kLowWarningVoltage || displayedPercent_ <= kLowWarningPercent);
  uint32_t interval = lowBatteryKnown ? kLowSampleIntervalMs : kSampleIntervalMs;
  if (playing && !lowBatteryKnown) {
    interval = kPlayingSampleIntervalMs;
  }
  return interval;
}

void Monitor::update(uint32_t nowMs, bool present, float rawVoltage, uint8_t rawPercent,
                     bool force) {
  if (!present) {
    present_ = false;
    criticalSampleCount_ = 0;
    return;
  }

  present_ = true;
  if (!initialized_) {
    filteredVoltage_ = rawVoltage;
    filteredPercent_ = rawPercent;
    displayedPercent_ = rawPercent;
    runtimeAnchorPercent_ = rawPercent;
    runtimeAnchorMs_ = nowMs;
    initialized_ = true;
    return;
  }

  filteredVoltage_ = (filteredVoltage_ * 0.72f) + (rawVoltage * 0.28f);
  filteredPercent_ = (filteredPercent_ * 0.72f) + (rawPercent * 0.28f);

  const int filteredPercent =
      std::max(0, std::min(100, static_cast<int>(filteredPercent_ + 0.5f)));
  const int delta = filteredPercent - static_cast<int>(displayedPercent_);
  if (force || std::abs(delta) >= kDisplayHysteresisPercent || filteredPercent <= 10 ||
      filteredPercent >= 99) {
    displayedPercent_ = static_cast<uint8_t>(filteredPercent);
  }

  if (displayedPercent_ > runtimeAnchorPercent_) {
    runtimeAnchorPercent_ = displayedPercent_;
    runtimeAnchorMs_ = nowMs;
    runtimeEstimateReady_ = false;
  } else {
    const uint8_t percentDrop = runtimeAnchorPercent_ - displayedPercent_;
    const uint32_t elapsedMs = nowMs - runtimeAnchorMs_;
    if (percentDrop >= kRuntimeMinDropPercent && elapsedMs >= kRuntimeMinElapsedMs) {
      const float minutesPerPercent =
          (static_cast<float>(elapsedMs) / 60000.0f) / static_cast<float>(percentDrop);
      runtimeMinutesRemaining_ =
          static_cast<uint32_t>(displayedPercent_ * minutesPerPercent + 0.5f);
      runtimeEstimateReady_ = true;
    }
  }
}

Action Monitor::protectionAction() {
  if (!present_ || !initialized_) {
    criticalSampleCount_ = 0;
    return Action::None;
  }

  const bool critical =
      filteredVoltage_ <= kCriticalVoltage || displayedPercent_ <= kCriticalPercent;
  if (critical) {
    if (criticalSampleCount_ < 255) {
      ++criticalSampleCount_;
    }
  } else {
    criticalSampleCount_ = 0;
  }

  if (criticalSampleCount_ >= kCriticalConsecutiveSamples) {
    return Action::Shutdown;
  }

  const bool low =
      filteredVoltage_ <= kLowWarningVoltage || displayedPercent_ <= kLowWarningPercent;
  return low ? Action::Warn : Action::None;
}

}  // namespace battery
