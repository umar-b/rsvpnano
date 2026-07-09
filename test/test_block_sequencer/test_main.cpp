#include <unity.h>

#include "timer/BlockSequencer.h"

using focustimer::BlockSequencer;
using Genre = BlockSequencer::Genre;
using State = BlockSequencer::State;
using Side = orientation::Side;

namespace {

// The orientation stabilizer promotes a face after 700 ms of persistence.
constexpr uint32_t kStableMs = 700;

focustimer::Config testConfig() {
  focustimer::Config cfg;
  cfg.touchStartArmDelayMs = 100;
  cfg.postTimerFlipGraceMs = 1000;
  cfg.feedbackMs = 150;
  cfg.touchDurationMs = 1000;
  cfg.workDurationMs = 2000;
  cfg.breakDurationMs = 500;
  return cfg;
}

// Holds `side` long enough to become stable, then updates. Returns the
// update time.
uint32_t holdSide(BlockSequencer &seq, Side side, uint32_t fromMs) {
  seq.feedOrientation(fromMs, side);
  seq.feedOrientation(fromMs + kStableMs, side);
  seq.update(fromMs + kStableMs, true);
  return fromMs + kStableMs;
}

// Fresh sequencer sitting in WaitForTouchStart (genre chosen at t=0).
BlockSequencer armedSequencer() {
  BlockSequencer seq(testConfig());
  seq.reset(true, 0);
  seq.chooseGenre(Genre::Chores, 0);
  return seq;
}

}  // namespace

void test_reset_lands_on_genre_select_or_unavailable(void) {
  BlockSequencer seq(testConfig());
  seq.reset(true, 0);
  TEST_ASSERT_EQUAL(State::GenreSelect, seq.state());
  TEST_ASSERT_FALSE(seq.hasLiveSession());

  seq.reset(false, 0);
  TEST_ASSERT_EQUAL(State::Unavailable, seq.state());
}

void test_choose_genre_arms_touch_wait(void) {
  BlockSequencer seq(testConfig());
  seq.reset(true, 0);

  seq.chooseGenre(Genre::None, 10);
  TEST_ASSERT_EQUAL(State::GenreSelect, seq.state());

  seq.chooseGenre(Genre::SelfCare, 10);
  TEST_ASSERT_EQUAL(State::WaitForTouchStart, seq.state());
  TEST_ASSERT_EQUAL(Genre::SelfCare, seq.genre());
  TEST_ASSERT_TRUE(seq.hasLiveSession());
  TEST_ASSERT_FALSE(seq.isBlockRunning());
}

void test_stable_short_side_starts_touch_block(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t t = holdSide(seq, Side::ShortSideA, 100);
  TEST_ASSERT_EQUAL(State::TouchRunning, seq.state());
  TEST_ASSERT_TRUE(seq.isBlockRunning());
  TEST_ASSERT_EQUAL(Side::ShortSideA, seq.activeStartSide());
  TEST_ASSERT_EQUAL_UINT32(1000, seq.remainingMs(t));
}

void test_touch_does_not_start_before_arm_delay(void) {
  focustimer::Config cfg = testConfig();
  cfg.touchStartArmDelayMs = 2000;
  BlockSequencer seq(cfg);
  seq.reset(true, 0);
  seq.chooseGenre(Genre::Chores, 0);

  holdSide(seq, Side::ShortSideA, 100);  // stable at 800, still inside the delay
  TEST_ASSERT_EQUAL(State::WaitForTouchStart, seq.state());

  seq.update(2000, true);  // face still held; arms now
  TEST_ASSERT_EQUAL(State::TouchRunning, seq.state());
}

void test_touch_expiry_completes_block_with_cue(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);

  seq.update(started + 999, true);
  TEST_ASSERT_EQUAL(State::TouchRunning, seq.state());

  seq.update(started + 1000, true);
  TEST_ASSERT_EQUAL(State::WaitAfterTouch, seq.state());
  TEST_ASSERT_EQUAL_UINT8(1, seq.completedTouchBlocks());
  TEST_ASSERT_FALSE(seq.isBlockRunning());
  TEST_ASSERT_TRUE(seq.hasLiveSession());
  TEST_ASSERT_TRUE(seq.consumeCompletionCue());
  TEST_ASSERT_FALSE(seq.consumeCompletionCue());
}

void test_flip_to_opposite_short_side_starts_work(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  seq.update(started + 1000, true);  // -> WaitAfterTouch at started+1000

  // Stable before the flip grace: nothing starts.
  const uint32_t stableAt = holdSide(seq, Side::ShortSideB, started + 1100);
  TEST_ASSERT_EQUAL(State::WaitAfterTouch, seq.state());

  // Past the grace with the face still held: work begins.
  seq.update(started + 1000 + 1000, true);
  TEST_ASSERT_EQUAL(State::WorkRunning, seq.state());
  TEST_ASSERT_EQUAL_UINT32(2000, seq.remainingMs(started + 2000));
  (void)stableAt;
}

void test_same_short_side_does_not_start_work(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  seq.update(started + 1000, true);  // -> WaitAfterTouch

  holdSide(seq, Side::ShortSideA, started + 1100);
  seq.update(started + 3000, true);  // well past the grace
  TEST_ASSERT_EQUAL(State::WaitAfterTouch, seq.state());
}

void test_long_side_starts_break_then_short_side_resumes_work(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  seq.update(started + 1000, true);  // -> WaitAfterTouch at t0
  const uint32_t t0 = started + 1000;

  holdSide(seq, Side::LongSide, t0 + 400);
  seq.update(t0 + 1100, true);  // past grace, long side held
  TEST_ASSERT_EQUAL(State::BreakRunning, seq.state());

  seq.update(t0 + 1100 + 500, true);  // break expires
  TEST_ASSERT_EQUAL(State::WaitAfterBreak, seq.state());
  TEST_ASSERT_EQUAL_UINT8(1, seq.completedBreakBlocks());

  const uint32_t t1 = t0 + 1600;
  holdSide(seq, Side::ShortSideB, t1 + 400);
  seq.update(t1 + 1200, true);  // past grace; any short side resumes work
  TEST_ASSERT_EQUAL(State::WorkRunning, seq.state());
}

void test_quick_flip_through_face_starts_nothing(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  seq.update(started + 1000, true);  // -> WaitAfterTouch at t0
  const uint32_t t0 = started + 1000;

  // The device tumbles through faces; none persists for the stable window.
  seq.feedOrientation(t0 + 200, Side::ShortSideB);
  seq.feedOrientation(t0 + 500, Side::LongSide);
  seq.feedOrientation(t0 + 800, Side::ShortSideB);
  seq.feedOrientation(t0 + 1100, Side::FlatBack);
  seq.update(t0 + 1400, true);  // past grace, but nothing ever settled
  TEST_ASSERT_EQUAL(State::WaitAfterTouch, seq.state());
}

void test_cancel_returns_to_touch_wait_after_feedback(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);

  seq.cancelActiveBlock(started + 200);
  TEST_ASSERT_EQUAL(State::Cancelled, seq.state());
  TEST_ASSERT_FALSE(seq.isBlockRunning());
  TEST_ASSERT_FALSE(seq.hasLiveSession());
  TEST_ASSERT_EQUAL(Genre::Chores, seq.genre());

  seq.update(started + 200 + 149, true);
  TEST_ASSERT_EQUAL(State::Cancelled, seq.state());
  seq.update(started + 200 + 150, true);
  TEST_ASSERT_EQUAL(State::WaitForTouchStart, seq.state());
  TEST_ASSERT_EQUAL(Genre::Chores, seq.genre());
  TEST_ASSERT_EQUAL_UINT8(0, seq.completedTouchBlocks());
}

void test_remaining_and_progress(void) {
  BlockSequencer seq = armedSequencer();
  TEST_ASSERT_EQUAL_UINT32(0, seq.remainingMs(50));
  TEST_ASSERT_EQUAL_UINT8(0, seq.progressPercent(50));

  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  TEST_ASSERT_EQUAL_UINT32(500, seq.remainingMs(started + 500));
  TEST_ASSERT_EQUAL_UINT8(50, seq.progressPercent(started + 500));
  TEST_ASSERT_EQUAL_UINT8(100, seq.progressPercent(started + 5000));
}

void test_work_expiry_counts_and_next_flip_resumes(void) {
  BlockSequencer seq = armedSequencer();
  const uint32_t started = holdSide(seq, Side::ShortSideA, 100);
  seq.update(started + 1000, true);
  const uint32_t t0 = started + 1000;

  holdSide(seq, Side::ShortSideB, t0 + 400);
  seq.update(t0 + 1100, true);  // work starts from B
  TEST_ASSERT_EQUAL(State::WorkRunning, seq.state());

  seq.update(t0 + 1100 + 2000, true);  // work expires
  TEST_ASSERT_EQUAL(State::WaitAfterWork, seq.state());
  TEST_ASSERT_EQUAL_UINT8(1, seq.completedWorkBlocks());

  const uint32_t t1 = t0 + 3100;
  holdSide(seq, Side::ShortSideA, t1 + 400);  // opposite of B
  seq.update(t1 + 1200, true);
  TEST_ASSERT_EQUAL(State::WorkRunning, seq.state());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reset_lands_on_genre_select_or_unavailable);
  RUN_TEST(test_choose_genre_arms_touch_wait);
  RUN_TEST(test_stable_short_side_starts_touch_block);
  RUN_TEST(test_touch_does_not_start_before_arm_delay);
  RUN_TEST(test_touch_expiry_completes_block_with_cue);
  RUN_TEST(test_flip_to_opposite_short_side_starts_work);
  RUN_TEST(test_same_short_side_does_not_start_work);
  RUN_TEST(test_long_side_starts_break_then_short_side_resumes_work);
  RUN_TEST(test_quick_flip_through_face_starts_nothing);
  RUN_TEST(test_cancel_returns_to_touch_wait_after_feedback);
  RUN_TEST(test_remaining_and_progress);
  RUN_TEST(test_work_expiry_counts_and_next_flip_resumes);
  return UNITY_END();
}
