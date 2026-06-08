#pragma once

#include <stdint.h>

// Pure device-orientation logic for the focus timer, split from the IMU I2C
// plumbing in FocusTimer. classify() maps a gravity vector to a face; the
// Stabilizer debounces a noisy stream of raw faces into a settled one. No
// hardware, no clock of its own (time is passed in) -- host-testable with
// synthetic accelerometer samples and fake timestamps.
namespace orientation {

enum class Side : uint8_t {
  ShortSideA = 0,
  ShortSideB,
  LongSide,
  FlatBack,
  Unknown,
};

// Maps a normalised accelerometer reading (units of g) to the face pointing
// up, or Unknown when the device is between defined orientations.
Side classify(float x, float y, float z);

// Debounces raw faces: a new raw face must persist for the stable window
// before it is promoted to the settled face. update() takes the current time
// and the freshly classified raw face; stable() returns the settled face.
class Stabilizer {
 public:
  void reset();
  void update(uint32_t nowMs, Side raw);
  // Sensor unreadable: clear the raw and settled faces (leaves the in-flight
  // candidate untouched, matching the original FocusTimer behaviour).
  void markUnavailable();

  Side raw() const { return raw_; }
  Side stable() const { return stable_; }

 private:
  Side raw_ = Side::Unknown;
  Side candidate_ = Side::Unknown;
  Side stable_ = Side::Unknown;
  uint32_t candidateSinceMs_ = 0;
};

}  // namespace orientation
