#pragma once

#include <stdint.h>

#include "motion/Orientation.h"

// The Focus Timer's block-sequencing decider: the tilt-driven state machine
// that orders touch -> work -> break blocks, owns the block clock and
// completion counters, and debounces orientation flips through its own
// orientation::Stabilizer. No IMU, no millis() -- time and classified faces
// are passed in, so the whole session flow runs host-side (the same carve-out
// motion::StandbyDecider is). FocusTimer keeps the IMU polling and the board
// UI-orientation mapping as the adapter.
namespace focustimer {

struct Config {
  // Ignore orientation right after entering the touch wait so the tap that
  // opened the page cannot start a block.
  uint32_t touchStartArmDelayMs = 350;
  // Ignore orientation right after a block ends so the flip that ended it
  // cannot also start the next one.
  uint32_t postTimerFlipGraceMs = 900;
  // How long the Cancelled/Complete feedback screens hold.
  uint32_t feedbackMs = 900;
  uint32_t touchDurationMs = 2UL * 60UL * 1000UL;
  uint32_t workDurationMs = 20UL * 60UL * 1000UL;
  uint32_t breakDurationMs = 5UL * 60UL * 1000UL;
};

class BlockSequencer {
 public:
  enum class Genre : uint8_t {
    Chores = 0,
    RsvpNano,
    StrengthLabs,
    SelfCare,
    Other,
    None = 0xFF,
  };

  enum class State : uint8_t {
    Unavailable = 0,
    GenreSelect,
    WaitForTouchStart,
    TouchRunning,
    WaitAfterTouch,
    WorkRunning,
    BreakRunning,
    WaitAfterWork,
    WaitAfterBreak,
    Cancelled,
    Complete,
  };

  explicit BlockSequencer(const Config &config = Config()) : config_(config) {}

  // Fresh page open / abandon: clears the session and lands on GenreSelect
  // (Unavailable without a motion sensor).
  void reset(bool available, uint32_t nowMs);
  void resetOrientation();
  void markOrientationUnavailable();
  // One classified orientation sample; the stabilizer debounces internally.
  void feedOrientation(uint32_t nowMs, orientation::Side raw);
  void update(uint32_t nowMs, bool available);
  void chooseGenre(Genre genre, uint32_t nowMs);
  void cancelActiveBlock(uint32_t nowMs);

  bool isBlockRunning() const { return timerRunning_; }
  // True while a session is in progress -- a block running OR the timer
  // between blocks waiting for the next orientation flip.
  bool hasLiveSession() const;
  State state() const { return state_; }
  Genre genre() const { return genre_; }
  orientation::Side activeStartSide() const { return activeStartOrientation_; }
  orientation::Side lastShortSide() const { return lastShortSide_; }
  uint32_t remainingMs(uint32_t nowMs) const;
  uint8_t progressPercent(uint32_t nowMs) const;
  uint8_t completedTouchBlocks() const { return completedTouchBlocks_; }
  uint8_t completedWorkBlocks() const { return completedWorkBlocks_; }
  uint8_t completedBreakBlocks() const { return completedBreakBlocks_; }
  bool consumeCompletionCue();

  static bool isShortSide(orientation::Side side);
  static orientation::Side oppositeShortSide(orientation::Side side);

 private:
  enum class BlockKind : uint8_t {
    None = 0,
    Touch,
    Work,
    Break,
  };

  bool orientationInputArmed(uint32_t nowMs) const;
  void transitionTo(State nextState, uint32_t nowMs);
  void clearSession();
  void startBlock(BlockKind kind, uint32_t nowMs, uint32_t durationMs,
                  orientation::Side startSide);
  void stopActiveBlock();
  void completeActiveBlock();
  bool blockExpired(uint32_t nowMs) const;

  Config config_;
  orientation::Stabilizer stabilizer_;
  orientation::Side activeStartOrientation_ = orientation::Side::Unknown;
  orientation::Side lastShortSide_ = orientation::Side::Unknown;
  State state_ = State::Unavailable;
  Genre genre_ = Genre::None;
  BlockKind activeBlock_ = BlockKind::None;
  uint32_t stateStartedMs_ = 0;
  uint32_t feedbackStartedMs_ = 0;
  uint32_t timerStartedMs_ = 0;
  uint32_t timerDurationMs_ = 0;
  bool timerRunning_ = false;
  bool completionCuePending_ = false;
  uint8_t completedTouchBlocks_ = 0;
  uint8_t completedWorkBlocks_ = 0;
  uint8_t completedBreakBlocks_ = 0;
};

}  // namespace focustimer
