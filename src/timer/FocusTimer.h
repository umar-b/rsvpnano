#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "board/BoardConfig.h"
#include "board/ImuDriver.h"
#include "motion/Orientation.h"
#include "timer/BlockSequencer.h"

// IMU adapter around the pure focustimer::BlockSequencer: polls the
// accelerometer, classifies faces, and maps the sequencer state to the
// board's UI orientation. All block sequencing lives (and is tested) in the
// sequencer.
class FocusTimer {
 public:
  using Genre = focustimer::BlockSequencer::Genre;
  using State = focustimer::BlockSequencer::State;

  bool begin();
  void open();
  void update(uint32_t nowMs);
  void chooseGenre(Genre genre, uint32_t nowMs);
  void cancelActiveTimer(uint32_t nowMs);
  void abandon();

  bool available() const;
  bool isActiveTimerRunning() const;
  // True when a session is in progress -- a block is running OR the timer is
  // between blocks waiting for the next orientation flip. Lets App reopen the
  // Focus Timer page onto a backgrounded session instead of clearing it.
  bool hasLiveSession() const;
  State state() const;
  Genre genre() const;
  BoardConfig::UiOrientation uiOrientation() const;
  uint32_t remainingMs(uint32_t nowMs) const;
  uint8_t progressPercent(uint32_t nowMs) const;
  uint8_t completedTouchBlocks() const;
  uint8_t completedWorkBlocks() const;
  uint8_t completedBreakBlocks() const;
  bool consumeCompletionCue();

  static const char *genreLabel(Genre genre);

 private:
  static BoardConfig::UiOrientation portraitOrientationForShortSide(orientation::Side side);

  ImuDriver imu_;
  focustimer::BlockSequencer sequencer_;
};
