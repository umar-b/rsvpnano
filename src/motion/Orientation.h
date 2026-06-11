#pragma once

#include <stdint.h>

// Pure device-orientation logic, split from the IMU I2C plumbing. classify()
// maps a gravity vector to a face; the Stabilizer debounces a noisy stream of
// raw faces into a settled one. The one definition of "flat" lives here --
// the focus timer (faces) and the standby decider (set-down, lift) both build
// on it. No hardware, no clock of its own (time is passed in) -- host-testable
// with synthetic accelerometer samples and fake timestamps.
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

// True when gravity shows the device lying flat with the screen facing DOWN.
// classify() folds both flat orientations into FlatBack (it keys on |z|), so
// screen-down needs its own sign-aware check. faceDownZSign is the board's z
// sign for screen-down (+1 or -1); this hardware reads z ~ -1 g screen-down.
// Uses the same flatness gate as the FlatBack branch of classify().
bool isScreenDown(float x, float y, float z, int faceDownZSign);

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
