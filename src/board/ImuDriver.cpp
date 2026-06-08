#include "board/ImuDriver.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t kImuAddress = 0x6B;
constexpr uint8_t kImuWhoAmIReg = 0x00;
constexpr uint8_t kImuCtrl1Reg = 0x02;
constexpr uint8_t kImuCtrl2Reg = 0x03;
constexpr uint8_t kImuCtrl5Reg = 0x06;
constexpr uint8_t kImuCtrl7Reg = 0x08;
constexpr uint8_t kImuCtrl8Reg = 0x09;
constexpr uint8_t kImuAccelStartReg = 0x35;
constexpr uint8_t kImuResetReg = 0x60;
constexpr uint8_t kImuResetValue = 0xB0;
constexpr uint8_t kImuResetResultReg = 0x4D;
constexpr uint8_t kImuResetResultValue = 0x80;
constexpr uint8_t kImuWhoAmIValue = 0x05;

}  // namespace

bool ImuDriver::begin() {
  if (available_) {
    return true;
  }

  Wire1.beginTransmission(kImuAddress);
  if (Wire1.endTransmission(true) != 0) {
    available_ = false;
    return false;
  }

  if (!writeRegister(kImuResetReg, kImuResetValue)) {
    available_ = false;
    return false;
  }

  const uint32_t waitStartedMs = millis();
  uint8_t resetResult = 0;
  bool resetReady = false;
  while (millis() - waitStartedMs < 500) {
    if (readRegister(kImuResetResultReg, resetResult) &&
        resetResult == kImuResetResultValue) {
      resetReady = true;
      break;
    }
    delay(10);
  }

  if (!resetReady) {
    available_ = false;
    return false;
  }

  uint8_t whoAmI = 0;
  if (!readRegister(kImuWhoAmIReg, whoAmI) || whoAmI != kImuWhoAmIValue) {
    available_ = false;
    return false;
  }

  if (!updateRegister(kImuCtrl1Reg, 0x40, 0x40) ||
      !writeRegister(kImuCtrl8Reg, 0x80) ||
      !writeRegister(kImuCtrl2Reg, 0x16) ||
      !updateRegister(kImuCtrl5Reg, 0x07, 0x07) ||
      !updateRegister(kImuCtrl7Reg, 0x01, 0x01)) {
    available_ = false;
    return false;
  }

  accelScale_ = 4.0f / 32768.0f;
  available_ = true;
  return true;
}

bool ImuDriver::available() const { return available_; }

bool ImuDriver::readRegister(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(kImuAddress);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }

  if (Wire1.requestFrom(static_cast<int>(kImuAddress), 1, 1) != 1) {
    return false;
  }

  value = Wire1.read();
  return true;
}

bool ImuDriver::writeRegister(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(kImuAddress);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission(true) == 0;
}

bool ImuDriver::readRegisters(uint8_t startReg, uint8_t *buffer, size_t len) {
  if (buffer == nullptr || len == 0 || len > 32) {
    return false;
  }

  Wire1.beginTransmission(kImuAddress);
  Wire1.write(startReg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }

  if (Wire1.requestFrom(static_cast<int>(kImuAddress), static_cast<int>(len), 1) !=
      static_cast<int>(len)) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire1.read();
  }
  return true;
}

bool ImuDriver::updateRegister(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!readRegister(reg, current)) {
    return false;
  }

  current = static_cast<uint8_t>((current & static_cast<uint8_t>(~mask)) | (value & mask));
  return writeRegister(reg, current);
}

bool ImuDriver::readAccel(float &x, float &y, float &z) {
  if (!available_) {
    return false;
  }

  uint8_t buffer[6] = {0};
  if (!readRegisters(kImuAccelStartReg, buffer, sizeof(buffer))) {
    return false;
  }

  const int16_t rawX = static_cast<int16_t>((buffer[1] << 8) | buffer[0]);
  const int16_t rawY = static_cast<int16_t>((buffer[3] << 8) | buffer[2]);
  const int16_t rawZ = static_cast<int16_t>((buffer[5] << 8) | buffer[4]);

  x = rawX * accelScale_;
  y = rawY * accelScale_;
  z = rawZ * accelScale_;
  return true;
}
