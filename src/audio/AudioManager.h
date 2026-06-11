#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include "audio/Jingle.h"

class AudioManager {
 public:
  bool begin();
  bool beep();
  // Play a named note sequence (see audio/Jingle.h). Synthesised on the fly and
  // streamed to I2S in small chunks so it needs no large buffer. Obeys the same
  // mute/volume gating as beep(): when muted (or volume 0) it returns true --
  // "handled, silently" -- without touching the codec. Sequences that exceed
  // the jingle duration budget are rejected (returns false) rather than played.
  bool playJingle(const jingle::Sequence &sequence);
  bool available() const;

  // Device-wide audio controls. Muting gates beep() entirely (it returns true
  // -- "handled, silently" -- so callers don't fall back to the backlight cue).
  // Volume is a 0-100 percentage mapped onto the ES8311 DAC volume register,
  // written just before each beep. setVolume(0) is equivalent to mute.
  void setMuted(bool muted);
  bool muted() const;
  void setVolume(uint8_t percent);  // clamps to 0-100
  uint8_t volume() const;

 private:
  static constexpr uint32_t kSampleRateHz = 16000;
  static constexpr uint32_t kBeepDurationMs = 120;
  static constexpr size_t kBeepFrames =
      (static_cast<size_t>(kSampleRateHz) * kBeepDurationMs) / 1000U;
  static constexpr size_t kBeepSamples = kBeepFrames * 2U;
  static constexpr i2s_port_t kI2sPort = I2S_NUM_0;

  bool enableAudioRail();
  bool initI2s();
  bool initCodec();
  bool configureCodec();
  bool configureCodecSampleFormat();
  bool startCodec();
  bool prepareForBeep();
  bool recoverOutputPath();
  bool writeBeepBuffer();
  bool writeSamples(const int16_t *samples, size_t sampleCount);
  bool synthesizeAndWriteJingle(const jingle::Sequence &sequence);
  bool readIoRegister(uint8_t reg, uint8_t &value);
  bool writeIoRegister(uint8_t reg, uint8_t value);
  bool readCodecRegister(uint8_t reg, uint8_t &value);
  bool writeCodecRegister(uint8_t reg, uint8_t value);
  void fillBeepBuffer();

  uint8_t dacVolumeRegisterValue() const;

  bool available_ = false;
  bool i2sInitialized_ = false;
  bool muted_ = false;
  uint8_t volumePercent_ = 100;
  int16_t beepBuffer_[kBeepSamples] = {};
};
