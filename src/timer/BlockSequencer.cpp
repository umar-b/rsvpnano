#include "timer/BlockSequencer.h"

namespace focustimer {

void BlockSequencer::reset(bool available, uint32_t nowMs) {
  clearSession();
  resetOrientation();
  state_ = available ? State::GenreSelect : State::Unavailable;
  stateStartedMs_ = nowMs;
}

void BlockSequencer::resetOrientation() { stabilizer_.reset(); }

void BlockSequencer::markOrientationUnavailable() { stabilizer_.markUnavailable(); }

void BlockSequencer::feedOrientation(uint32_t nowMs, orientation::Side raw) {
  stabilizer_.update(nowMs, raw);
}

void BlockSequencer::update(uint32_t nowMs, bool available) {
  switch (state_) {
    case State::Unavailable:
    case State::GenreSelect:
      break;

    case State::WaitForTouchStart:
      if (orientationInputArmed(nowMs) && isShortSide(stabilizer_.stable())) {
        startBlock(BlockKind::Touch, nowMs, config_.touchDurationMs, stabilizer_.stable());
        transitionTo(State::TouchRunning, nowMs);
      }
      break;

    case State::TouchRunning:
      if (blockExpired(nowMs)) {
        completeActiveBlock();
        resetOrientation();
        transitionTo(State::WaitAfterTouch, nowMs);
      }
      break;

    case State::WaitAfterTouch:
      if (!orientationInputArmed(nowMs)) {
        break;
      }
      if (stabilizer_.stable() == oppositeShortSide(lastShortSide_)) {
        startBlock(BlockKind::Work, nowMs, config_.workDurationMs, stabilizer_.stable());
        transitionTo(State::WorkRunning, nowMs);
      } else if (stabilizer_.stable() == orientation::Side::LongSide) {
        startBlock(BlockKind::Break, nowMs, config_.breakDurationMs,
                   orientation::Side::LongSide);
        transitionTo(State::BreakRunning, nowMs);
      }
      break;

    case State::WorkRunning:
      if (blockExpired(nowMs)) {
        completeActiveBlock();
        resetOrientation();
        transitionTo(State::WaitAfterWork, nowMs);
      }
      break;

    case State::WaitAfterWork:
      if (!orientationInputArmed(nowMs)) {
        break;
      }
      if (stabilizer_.stable() == oppositeShortSide(lastShortSide_)) {
        startBlock(BlockKind::Work, nowMs, config_.workDurationMs, stabilizer_.stable());
        transitionTo(State::WorkRunning, nowMs);
      } else if (stabilizer_.stable() == orientation::Side::LongSide) {
        startBlock(BlockKind::Break, nowMs, config_.breakDurationMs,
                   orientation::Side::LongSide);
        transitionTo(State::BreakRunning, nowMs);
      }
      break;

    case State::BreakRunning:
      if (blockExpired(nowMs)) {
        completeActiveBlock();
        resetOrientation();
        transitionTo(State::WaitAfterBreak, nowMs);
      }
      break;

    case State::WaitAfterBreak:
      if (orientationInputArmed(nowMs) && isShortSide(stabilizer_.stable())) {
        startBlock(BlockKind::Work, nowMs, config_.workDurationMs, stabilizer_.stable());
        transitionTo(State::WorkRunning, nowMs);
      }
      break;

    case State::Cancelled:
      if (nowMs - feedbackStartedMs_ >= config_.feedbackMs) {
        resetOrientation();
        transitionTo(State::WaitForTouchStart, nowMs);
      }
      break;

    case State::Complete:
      if (nowMs - feedbackStartedMs_ >= config_.feedbackMs) {
        clearSession();
        resetOrientation();
        transitionTo(available ? State::GenreSelect : State::Unavailable, nowMs);
      }
      break;
  }
}

void BlockSequencer::chooseGenre(Genre genre, uint32_t nowMs) {
  if (genre == Genre::None) {
    return;
  }

  clearSession();
  genre_ = genre;
  resetOrientation();
  transitionTo(State::WaitForTouchStart, nowMs);
}

void BlockSequencer::cancelActiveBlock(uint32_t nowMs) {
  if (!timerRunning_) {
    return;
  }

  stopActiveBlock();
  resetOrientation();
  feedbackStartedMs_ = nowMs;
  transitionTo(State::Cancelled, nowMs);
}

bool BlockSequencer::hasLiveSession() const {
  switch (state_) {
    case State::WaitForTouchStart:
    case State::TouchRunning:
    case State::WaitAfterTouch:
    case State::WorkRunning:
    case State::BreakRunning:
    case State::WaitAfterWork:
    case State::WaitAfterBreak:
      return true;
    case State::Unavailable:
    case State::GenreSelect:
    case State::Cancelled:
    case State::Complete:
    default:
      return false;
  }
}

uint32_t BlockSequencer::remainingMs(uint32_t nowMs) const {
  if (!timerRunning_) {
    return 0;
  }

  const uint32_t elapsed = nowMs - timerStartedMs_;
  return (elapsed >= timerDurationMs_) ? 0 : (timerDurationMs_ - elapsed);
}

uint8_t BlockSequencer::progressPercent(uint32_t nowMs) const {
  if (!timerRunning_ || timerDurationMs_ == 0) {
    return 0;
  }

  const uint32_t elapsed = nowMs - timerStartedMs_;
  const uint32_t clamped = (elapsed >= timerDurationMs_) ? timerDurationMs_ : elapsed;
  return static_cast<uint8_t>((clamped * 100U) / timerDurationMs_);
}

bool BlockSequencer::consumeCompletionCue() {
  const bool pending = completionCuePending_;
  completionCuePending_ = false;
  return pending;
}

bool BlockSequencer::orientationInputArmed(uint32_t nowMs) const {
  switch (state_) {
    case State::WaitForTouchStart:
      return (nowMs - stateStartedMs_) >= config_.touchStartArmDelayMs;
    case State::WaitAfterTouch:
    case State::WaitAfterWork:
    case State::WaitAfterBreak:
      return (nowMs - stateStartedMs_) >= config_.postTimerFlipGraceMs;
    default:
      return true;
  }
}

void BlockSequencer::transitionTo(State nextState, uint32_t nowMs) {
  state_ = nextState;
  stateStartedMs_ = nowMs;
}

void BlockSequencer::clearSession() {
  genre_ = Genre::None;
  activeBlock_ = BlockKind::None;
  activeStartOrientation_ = orientation::Side::Unknown;
  lastShortSide_ = orientation::Side::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  timerRunning_ = false;
  feedbackStartedMs_ = 0;
  completionCuePending_ = false;
  completedTouchBlocks_ = 0;
  completedWorkBlocks_ = 0;
  completedBreakBlocks_ = 0;
}

void BlockSequencer::startBlock(BlockKind kind, uint32_t nowMs, uint32_t durationMs,
                                orientation::Side startSide) {
  activeBlock_ = kind;
  activeStartOrientation_ = startSide;
  timerStartedMs_ = nowMs;
  timerDurationMs_ = durationMs;
  timerRunning_ = true;

  if (isShortSide(startSide)) {
    lastShortSide_ = startSide;
  }
}

void BlockSequencer::stopActiveBlock() {
  timerRunning_ = false;
  activeBlock_ = BlockKind::None;
  activeStartOrientation_ = orientation::Side::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  lastShortSide_ = orientation::Side::Unknown;
}

void BlockSequencer::completeActiveBlock() {
  if (!timerRunning_) {
    return;
  }

  switch (activeBlock_) {
    case BlockKind::Touch:
      ++completedTouchBlocks_;
      break;
    case BlockKind::Work:
      ++completedWorkBlocks_;
      break;
    case BlockKind::Break:
      ++completedBreakBlocks_;
      break;
    case BlockKind::None:
    default:
      break;
  }

  timerRunning_ = false;
  activeBlock_ = BlockKind::None;
  activeStartOrientation_ = orientation::Side::Unknown;
  timerStartedMs_ = 0;
  timerDurationMs_ = 0;
  completionCuePending_ = true;
}

bool BlockSequencer::blockExpired(uint32_t nowMs) const {
  return timerRunning_ && (nowMs - timerStartedMs_ >= timerDurationMs_);
}

bool BlockSequencer::isShortSide(orientation::Side side) {
  return side == orientation::Side::ShortSideA || side == orientation::Side::ShortSideB;
}

orientation::Side BlockSequencer::oppositeShortSide(orientation::Side side) {
  switch (side) {
    case orientation::Side::ShortSideA:
      return orientation::Side::ShortSideB;
    case orientation::Side::ShortSideB:
      return orientation::Side::ShortSideA;
    default:
      return orientation::Side::Unknown;
  }
}

}  // namespace focustimer
