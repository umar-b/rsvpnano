#include "timer/FocusTimer.h"

bool FocusTimer::begin() {
  const bool ready = imu_.begin();
  if (ready) {
    sequencer_.resetOrientation();
  }
  return ready;
}

void FocusTimer::open() {
  if (!imu_.available()) {
    imu_.begin();
  }

  sequencer_.reset(imu_.available(), millis());
}

void FocusTimer::update(uint32_t nowMs) {
  if (imu_.available()) {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (imu_.readAccel(x, y, z)) {
      sequencer_.feedOrientation(nowMs, orientation::classify(x, y, z));
    }
  }

  sequencer_.update(nowMs, imu_.available());
}

void FocusTimer::chooseGenre(Genre genre, uint32_t nowMs) {
  sequencer_.chooseGenre(genre, nowMs);
}

void FocusTimer::cancelActiveTimer(uint32_t nowMs) { sequencer_.cancelActiveBlock(nowMs); }

void FocusTimer::abandon() { sequencer_.reset(imu_.available(), millis()); }

bool FocusTimer::available() const { return imu_.available(); }

bool FocusTimer::isActiveTimerRunning() const { return sequencer_.isBlockRunning(); }

bool FocusTimer::hasLiveSession() const { return sequencer_.hasLiveSession(); }

FocusTimer::State FocusTimer::state() const { return sequencer_.state(); }

FocusTimer::Genre FocusTimer::genre() const { return sequencer_.genre(); }

BoardConfig::UiOrientation FocusTimer::uiOrientation() const {
  switch (sequencer_.state()) {
    case State::GenreSelect:
    case State::Unavailable:
    case State::Complete:
      return BoardConfig::UiOrientation::Landscape;

    case State::WaitForTouchStart:
    case State::TouchRunning:
    case State::Cancelled:
      return portraitOrientationForShortSide(sequencer_.activeStartSide());

    case State::WaitAfterTouch:
    case State::WorkRunning:
    case State::WaitAfterBreak:
      return portraitOrientationForShortSide(sequencer_.lastShortSide());

    case State::BreakRunning:
    case State::WaitAfterWork:
      return BoardConfig::UiOrientation::Landscape;

    default:
      return BoardConfig::UiOrientation::Portrait;
  }
}

uint32_t FocusTimer::remainingMs(uint32_t nowMs) const { return sequencer_.remainingMs(nowMs); }

uint8_t FocusTimer::progressPercent(uint32_t nowMs) const {
  return sequencer_.progressPercent(nowMs);
}

uint8_t FocusTimer::completedTouchBlocks() const { return sequencer_.completedTouchBlocks(); }

uint8_t FocusTimer::completedWorkBlocks() const { return sequencer_.completedWorkBlocks(); }

uint8_t FocusTimer::completedBreakBlocks() const { return sequencer_.completedBreakBlocks(); }

bool FocusTimer::consumeCompletionCue() { return sequencer_.consumeCompletionCue(); }

const char *FocusTimer::genreLabel(Genre genre) {
  switch (genre) {
    case Genre::Chores:
      return "Chores";
    case Genre::RsvpNano:
      return "Work";
    case Genre::StrengthLabs:
      return "Fitness";
    case Genre::SelfCare:
      return "Self Care";
    case Genre::Other:
      return "Other";
    case Genre::None:
    default:
      return "";
  }
}

BoardConfig::UiOrientation FocusTimer::portraitOrientationForShortSide(orientation::Side side) {
  return side == orientation::Side::ShortSideB ? BoardConfig::UiOrientation::PortraitFlipped
                                               : BoardConfig::UiOrientation::Portrait;
}
