#include <unity.h>

#include "motion/StandbyDecider.h"

// The decider's contract, exercised with synthetic accel samples and fake
// timestamps at the real 40ms poll cadence. The scenario tests at the bottom
// encode the shipped standby bugs (sleep-while-held, wake/standby ping-pong,
// combo-standby self-wake) as regressions.

namespace {

using motion::StandbyContext;
using motion::StandbyDecider;
using motion::StandbyVerdict;

constexpr uint32_t kTickMs = 40;       // mirrors App's IMU poll cadence
constexpr uint32_t kHoldMs = 3000;     // mirrors Config::setDownHoldMs
constexpr uint32_t kLiftMs = 400;      // mirrors Config::liftStableMs
constexpr uint32_t kGraceMs = 900;     // mirrors Config::wakeGraceMs

// This board reads z ~ -1 g screen-down (Config::faceDownZSign = -1).
constexpr float kFlatUpZ = 0.98f;
constexpr float kFlatDownZ = -0.98f;
constexpr float kUprightZ = 0.30f;  // held in hand, clearly not flat

// Feeds identical samples every tick until a verdict fires or untilMs passes.
// Advances t; returns the verdict (Kind::None if nothing fired in time).
StandbyVerdict feedUntilVerdict(StandbyDecider &d, uint32_t &t, uint32_t untilMs, float x, float y,
                                float z, StandbyContext ctx) {
  while (t + kTickMs <= untilMs) {
    t += kTickMs;
    const StandbyVerdict v = d.updateWithSample(t, x, y, z, ctx);
    if (v.kind != StandbyVerdict::Kind::None) {
      return v;
    }
  }
  return StandbyVerdict{};
}

// Clock-only variant (sensor off): drives update() instead.
StandbyVerdict tickUntilVerdict(StandbyDecider &d, uint32_t &t, uint32_t untilMs,
                                StandbyContext ctx) {
  while (t + kTickMs <= untilMs) {
    t += kTickMs;
    const StandbyVerdict v = d.update(t, ctx);
    if (v.kind != StandbyVerdict::Kind::None) {
      return v;
    }
  }
  return StandbyVerdict{};
}

int asInt(StandbyVerdict::Kind k) { return static_cast<int>(k); }

}  // namespace

// --- Set-down -----------------------------------------------------------

void test_set_down_screen_up_enters_standby_after_hold() {
  StandbyDecider d;
  uint32_t t = 0;
  d.noteActivity(t);
  // No verdict strictly before the hold completes.
  StandbyVerdict v = feedUntilVerdict(d, t, kHoldMs, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  // ...and fires within a tick or two after it.
  v = feedUntilVerdict(d, t, kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
  TEST_ASSERT_FALSE(v.screenOff);
  TEST_ASSERT_TRUE(d.inStandby());

  // Staying flat afterwards never re-fires: the decider knows its own mode.
  v = feedUntilVerdict(d, t, t + 5000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
}

void test_set_down_screen_down_goes_screen_off() {
  StandbyDecider d;
  uint32_t t = 0;
  const StandbyVerdict v =
      feedUntilVerdict(d, t, kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatDownZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
  TEST_ASSERT_TRUE(v.screenOff);
}

void test_drifting_hand_never_sets_down() {
  // Flat-ish in a hand: the reading wanders more than the stillness threshold,
  // so the reference keeps re-baselining and the hold never completes. This is
  // the sleep-while-held regression.
  StandbyDecider d;
  uint32_t t = 0;
  bool high = false;
  while (t < 10000) {
    // Drift 0.05 g every 400 ms (threshold is 0.02 g).
    if (t % 400 == 0) {
      high = !high;
    }
    t += kTickMs;
    const StandbyVerdict v =
        d.updateWithSample(t, high ? 0.05f : 0.0f, 0.0f, kFlatUpZ, StandbyContext::Playing);
    TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  }
}

void test_set_down_only_while_reading() {
  StandbyDecider d;
  uint32_t t = 0;
  StandbyVerdict v = feedUntilVerdict(d, t, 5000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Menu);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = feedUntilVerdict(d, t, 10000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Busy);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
}

void test_menu_flat_time_does_not_count_toward_set_down() {
  // Lying flat through the Menu must not bank hold time that an entry into
  // Paused then cashes in instantly.
  StandbyDecider d;
  uint32_t t = 0;
  StandbyVerdict v = feedUntilVerdict(d, t, 2900, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Menu);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  const uint32_t switchedMs = t;
  v = feedUntilVerdict(d, t, switchedMs + kHoldMs - kTickMs, 0.0f, 0.0f, kFlatUpZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = feedUntilVerdict(d, t, switchedMs + kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

void test_flatness_definition_is_shared_with_orientation() {
  // |z| alone used to qualify as flat; the orientation module's gate also
  // bounds x and y. A propped-up device (z high but x well off axis) must not
  // set down.
  StandbyDecider d;
  uint32_t t = 0;
  const StandbyVerdict v =
      feedUntilVerdict(d, t, 8000, 0.5f, 0.0f, 0.9f, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
}

// --- Lift-to-wake and the wake grace -------------------------------------

void test_lift_wakes_after_stable_window() {
  StandbyDecider d;
  uint32_t t = 0;
  feedUntilVerdict(d, t, kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_TRUE(d.inStandby());
  // Rest on the table well past the grace, then pick it up.
  feedUntilVerdict(d, t, t + 2000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  const uint32_t liftedMs = t;
  StandbyVerdict v =
      feedUntilVerdict(d, t, liftedMs + kLiftMs - kTickMs, 0.0f, 0.0f, kUprightZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = feedUntilVerdict(d, t, liftedMs + kLiftMs + 4 * kTickMs, 0.0f, 0.0f, kUprightZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::Wake), asInt(v.kind));
  TEST_ASSERT_FALSE(d.inStandby());
}

void test_lift_blocked_by_wake_grace() {
  // Picked up immediately after standby entry: the lift window completes at
  // 400 ms but the wake must still wait for the 900 ms grace.
  StandbyDecider d;
  uint32_t t = 0;
  feedUntilVerdict(d, t, kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  const uint32_t enteredMs = t;
  StandbyVerdict v = feedUntilVerdict(d, t, enteredMs + kGraceMs - kTickMs, 0.0f, 0.0f, kUprightZ,
                                      StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = feedUntilVerdict(d, t, enteredMs + kGraceMs + 4 * kTickMs, 0.0f, 0.0f, kUprightZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::Wake), asInt(v.kind));
}

void test_combo_standby_in_hand_does_not_self_wake() {
  // Standby entered by the button combo while the device is held upright: the
  // device was never down, so a lift must not fire. It has to rest flat once
  // before lift-to-wake arms. (Latent bug in the smeared version: the lift
  // candidate started accumulating immediately and woke the device 900 ms in.)
  StandbyDecider d;
  uint32_t t = 1000;
  d.noteStandbyEntered(t);
  StandbyVerdict v =
      feedUntilVerdict(d, t, t + 5000, 0.0f, 0.0f, kUprightZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  // Rest it flat, then lift: now it wakes.
  feedUntilVerdict(d, t, t + 1000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  const uint32_t liftedMs = t;
  v = feedUntilVerdict(d, t, liftedMs + kLiftMs + 4 * kTickMs, 0.0f, 0.0f, kUprightZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::Wake), asInt(v.kind));
}

void test_can_wake_now_owns_the_grace() {
  StandbyDecider d;
  TEST_ASSERT_TRUE(d.canWakeNow(0));  // active: wake paths may always act
  d.noteStandbyEntered(1000);
  TEST_ASSERT_FALSE(d.canWakeNow(1000 + kGraceMs - 1));
  TEST_ASSERT_TRUE(d.canWakeNow(1000 + kGraceMs));
  // Re-notifying while already in standby must not re-anchor the grace.
  d.noteStandbyEntered(1500);
  TEST_ASSERT_TRUE(d.canWakeNow(1000 + kGraceMs));
}

// --- Re-arm rule (the wake/standby ping-pong regression) ------------------

void test_tap_wake_while_flat_disarms_set_down_until_lifted() {
  StandbyDecider d;
  uint32_t t = 0;
  feedUntilVerdict(d, t, kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_TRUE(d.inStandby());

  // Tap-wake (App handles the touch; it reports the wake here). Device stays
  // flat and still on the table.
  t += 1000;
  d.noteWoke(t);
  TEST_ASSERT_FALSE(d.inStandby());
  StandbyVerdict v =
      feedUntilVerdict(d, t, t + 10000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));

  // Pick it up once (one non-flat sample re-arms), set it down again: fires.
  t += kTickMs;
  d.updateWithSample(t, 0.0f, 0.0f, kUprightZ, StandbyContext::Paused);
  const uint32_t setDownMs = t;
  v = feedUntilVerdict(d, t, setDownMs + kHoldMs + 4 * kTickMs, 0.0f, 0.0f, kFlatUpZ,
                       StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

// --- Idle timeout ---------------------------------------------------------

void test_idle_fires_in_paused_without_samples() {
  // The idle timer must run even with the motion sensor off or absent.
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);
  StandbyVerdict v = tickUntilVerdict(d, t, 60000 - kTickMs, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = tickUntilVerdict(d, t, 60000 + 4 * kTickMs, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
  TEST_ASSERT_FALSE(v.screenOff);
}

void test_idle_fires_in_menu() {
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);
  const StandbyVerdict v = tickUntilVerdict(d, t, 60000 + 4 * kTickMs, StandbyContext::Menu);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

void test_idle_never_fires_while_playing_or_busy() {
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);
  StandbyVerdict v = tickUntilVerdict(d, t, 130000, StandbyContext::Playing);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  // (Context switch counts as activity; Busy gets its own full window.)
  v = tickUntilVerdict(d, t, t + 130000, StandbyContext::Busy);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
}

void test_idle_zero_means_off() {
  StandbyDecider d;
  uint32_t t = 0;
  d.noteActivity(t);
  const StandbyVerdict v = tickUntilVerdict(d, t, 600000, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
}

void test_activity_resets_idle() {
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);
  // Held upright in Paused so only the idle path is in play.
  StandbyVerdict v =
      feedUntilVerdict(d, t, 30000, 0.0f, 0.0f, kUprightZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  d.noteActivity(t);
  v = feedUntilVerdict(d, t, 90000 - kTickMs, 0.0f, 0.0f, kUprightZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = feedUntilVerdict(d, t, 90000 + 4 * kTickMs, 0.0f, 0.0f, kUprightZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

void test_context_change_counts_as_activity() {
  // Leaving a busy mode (or any screen change) must not inherit a stale idle
  // countdown and standby on the spot.
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);
  StandbyVerdict v = tickUntilVerdict(d, t, 50000, StandbyContext::Busy);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  const uint32_t switchedMs = t;
  v = tickUntilVerdict(d, t, switchedMs + 60000 - kTickMs, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));
  v = tickUntilVerdict(d, t, switchedMs + 60000 + 4 * kTickMs, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

// --- The full oscillation scenario ---------------------------------------

void test_wake_standby_ping_pong_regression() {
  // The shipped bug: wake from standby kept bouncing between the screensaver
  // and the reading display. Full cycle: set-down -> standby -> tap-wake while
  // still flat -> must stay awake (idle takes over) -> lift -> set down again
  // -> standby.
  StandbyDecider d;
  d.setIdleTimeoutMs(60000);
  uint32_t t = 0;
  d.noteActivity(t);

  StandbyVerdict v =
      feedUntilVerdict(d, t, 60000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));

  // Tap-wake past the grace; device untouched on the table.
  t += 2000;
  TEST_ASSERT_TRUE(d.canWakeNow(t));
  d.noteWoke(t);

  // Flat and still for 30 s: no posture standby (disarmed), no idle yet.
  v = feedUntilVerdict(d, t, t + 30000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::None), asInt(v.kind));

  // Idle eventually standbys it again (noteWoke counted as activity).
  v = feedUntilVerdict(d, t, t + 31000, 0.0f, 0.0f, kFlatUpZ, StandbyContext::Paused);
  TEST_ASSERT_EQUAL_INT(asInt(StandbyVerdict::Kind::EnterStandby), asInt(v.kind));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_set_down_screen_up_enters_standby_after_hold);
  RUN_TEST(test_set_down_screen_down_goes_screen_off);
  RUN_TEST(test_drifting_hand_never_sets_down);
  RUN_TEST(test_set_down_only_while_reading);
  RUN_TEST(test_menu_flat_time_does_not_count_toward_set_down);
  RUN_TEST(test_flatness_definition_is_shared_with_orientation);
  RUN_TEST(test_lift_wakes_after_stable_window);
  RUN_TEST(test_lift_blocked_by_wake_grace);
  RUN_TEST(test_combo_standby_in_hand_does_not_self_wake);
  RUN_TEST(test_can_wake_now_owns_the_grace);
  RUN_TEST(test_tap_wake_while_flat_disarms_set_down_until_lifted);
  RUN_TEST(test_idle_fires_in_paused_without_samples);
  RUN_TEST(test_idle_fires_in_menu);
  RUN_TEST(test_idle_never_fires_while_playing_or_busy);
  RUN_TEST(test_idle_zero_means_off);
  RUN_TEST(test_activity_resets_idle);
  RUN_TEST(test_context_change_counts_as_activity);
  RUN_TEST(test_wake_standby_ping_pong_regression);
  return UNITY_END();
}
