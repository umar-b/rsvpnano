#pragma once

#include <stddef.h>
#include <stdint.h>

// I2C accelerometer plumbing for the board's motion sensor (QMI8658-class part
// at address 0x6B on Wire1, configured for +/-4g). Lifted out of FocusTimer so
// more than one subsystem can read the sensor: FocusTimer drives the tilt-based
// block transitions, and the reader can poll the same driver for hands-free
// IMU shortcuts.
//
// This is an I/O class -- the pure orientation classification stays in the
// orientation module (orientation::classify / Stabilizer), which is host-tested
// independently. ImuDriver only does the register reads and the raw->g scaling.
class ImuDriver {
 public:
  // Probes and configures the sensor. Idempotent: returns true immediately if
  // already initialised. Returns false (and stays unavailable) if the sensor is
  // absent or the reset/whoami handshake fails.
  bool begin();

  // True once begin() has successfully configured the sensor.
  bool available() const;

  // Reads the latest acceleration in g (gravity-scaled). Returns false on an
  // I2C error or when the sensor is unavailable; x/y/z are left untouched then.
  bool readAccel(float &x, float &y, float &z);

 private:
  bool readRegister(uint8_t reg, uint8_t &value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t startReg, uint8_t *buffer, size_t len);
  bool updateRegister(uint8_t reg, uint8_t mask, uint8_t value);

  bool available_ = false;
  float accelScale_ = 4.0f / 32768.0f;
};
