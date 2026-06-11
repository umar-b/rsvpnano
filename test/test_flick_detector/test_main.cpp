#include <unity.h>

#include "motion/FlickDetector.h"

// Flick rewind spike detection: trigger, hysteresis re-arm, cooldown. Pure
// module, synthetic samples, fake timestamps.

namespace {

using motion::FlickDetector;

constexpr uint32_t kTickMs = 40;
constexpr uint32_t kCooldownMs = 800;  // mirrors Config::cooldownMs

constexpr float kQuietG = 1.0f;   // resting magnitude
constexpr float kSpikeG = 2.0f;   // well above the 1.7 g trigger
// 1.0 g sits below the 1.25 g re-arm level; 1.5 g sits between re-arm and
// trigger (settled enough for nothing, spiky enough for nothing).
constexpr float kBetweenG = 1.5f;

}  // namespace

void test_quiet_hold_never_fires() {
  FlickDetector f;
  for (uint32_t t = kTickMs; t <= 5000; t += kTickMs) {
    TEST_ASSERT_FALSE(f.updateWithSample(t, kQuietG, 0.0f, 0.0f, true));
  }
}

void test_fires_on_spike_then_disarms() {
  FlickDetector f;
  TEST_ASSERT_FALSE(f.updateWithSample(40, kQuietG, 0.0f, 0.0f, true));
  TEST_ASSERT_TRUE(f.updateWithSample(80, kSpikeG, 0.0f, 0.0f, true));
  // Still spiking: disarmed, no repeat fire.
  TEST_ASSERT_FALSE(f.updateWithSample(120, kSpikeG, 0.0f, 0.0f, true));
}

void test_rearm_needs_cooldown_and_settle() {
  FlickDetector f;
  TEST_ASSERT_TRUE(f.updateWithSample(40, kSpikeG, 0.0f, 0.0f, true));
  // Settled immediately, but the cooldown has not elapsed: stays disarmed.
  TEST_ASSERT_FALSE(f.updateWithSample(80, kQuietG, 0.0f, 0.0f, true));
  TEST_ASSERT_FALSE(f.updateWithSample(400, kSpikeG, 0.0f, 0.0f, true));
  // Past the cooldown (anchored on the fire) but not settled below the
  // re-arm level: still disarmed.
  TEST_ASSERT_FALSE(f.updateWithSample(40 + kCooldownMs, kBetweenG, 0.0f, 0.0f, true));
  TEST_ASSERT_FALSE(f.updateWithSample(40 + kCooldownMs + kTickMs, kSpikeG, 0.0f, 0.0f, true));
  // A settled sample past the cooldown re-arms; the next spike fires.
  const uint32_t settleMs = 40 + 2 * kCooldownMs + kTickMs;
  TEST_ASSERT_FALSE(f.updateWithSample(settleMs, kQuietG, 0.0f, 0.0f, true));
  TEST_ASSERT_TRUE(f.updateWithSample(settleMs + kTickMs, kSpikeG, 0.0f, 0.0f, true));
}

void test_disabled_rearms_and_never_fires() {
  FlickDetector f;
  // Spikes while not reading (menu, standby) never fire...
  TEST_ASSERT_FALSE(f.updateWithSample(40, kSpikeG, 0.0f, 0.0f, false));
  // ...and being disabled re-arms, matching the smeared version's behaviour.
  TEST_ASSERT_TRUE(f.updateWithSample(80, kSpikeG, 0.0f, 0.0f, true));
}

void test_reset_rearms() {
  FlickDetector f;
  TEST_ASSERT_TRUE(f.updateWithSample(40, kSpikeG, 0.0f, 0.0f, true));
  f.reset();
  TEST_ASSERT_TRUE(f.updateWithSample(80, kSpikeG, 0.0f, 0.0f, true));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_quiet_hold_never_fires);
  RUN_TEST(test_fires_on_spike_then_disarms);
  RUN_TEST(test_rearm_needs_cooldown_and_settle);
  RUN_TEST(test_disabled_rearms_and_never_fires);
  RUN_TEST(test_reset_rearms);
  return UNITY_END();
}
