#include <unity.h>

#include "board/BatteryManager.h"

using battery::Action;
using battery::Monitor;

namespace {
constexpr uint32_t kMinute = 60UL * 1000UL;
}  // namespace

// First reading seeds the filter directly (no smoothing) and shows the raw
// percent immediately.
void test_first_sample_seeds_directly() {
  Monitor m;
  TEST_ASSERT_FALSE(m.initialized());
  m.update(0, true, 4.00f, 80, false);
  TEST_ASSERT_TRUE(m.present());
  TEST_ASSERT_TRUE(m.initialized());
  TEST_ASSERT_EQUAL_UINT(80, m.displayedPercent());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.00f, m.filteredVoltage());
}

// Display percent only moves once the filtered value clears the hysteresis band.
void test_display_hysteresis() {
  Monitor m;
  m.update(0, true, 4.00f, 80, false);
  // A single 1% nudge stays inside the 2% band -> display unchanged.
  m.update(kMinute, true, 4.00f, 81, false);
  TEST_ASSERT_EQUAL_UINT(80, m.displayedPercent());
  // Repeated higher readings eventually drag the EMA past the band.
  for (int i = 0; i < 20; ++i) {
    m.update(kMinute * (2 + i), true, 4.00f, 90, false);
  }
  TEST_ASSERT_TRUE(m.displayedPercent() > 80);
}

// `force` snaps the display to the filtered value, bypassing the hysteresis
// band. A raw of 82 nudges the EMA to ~80.6 -> rounds to 81 (a delta of 1, which
// the 2% band would normally swallow); force lets it through.
void test_force_snaps_display() {
  Monitor m;
  m.update(0, true, 4.00f, 80, false);
  m.update(kMinute, true, 4.00f, 82, true);
  TEST_ASSERT_EQUAL_UINT(81, m.displayedPercent());
}

// Two consecutive critical samples trip the shutdown; a single one does not.
void test_shutdown_after_consecutive_critical() {
  Monitor m;
  m.update(0, true, 3.20f, 0, false);  // below critical voltage + percent
  // First critical sample: counter = 1, below the threshold of 2.
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(Action::Shutdown),
                        static_cast<int>(m.protectionAction()));
  m.update(kMinute, true, 3.20f, 0, true);  // second consecutive critical
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Shutdown), static_cast<int>(m.protectionAction()));
}

void test_single_critical_then_recover_no_shutdown() {
  Monitor m;
  m.update(0, true, 4.00f, 80, false);
  // One critical reading -> Warn/None range, not Shutdown, then recover.
  m.update(kMinute, true, 3.20f, 0, true);
  const Action first = m.protectionAction();  // count = 1, below threshold
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(Action::Shutdown), static_cast<int>(first));
  m.update(kMinute * 2, true, 4.00f, 80, true);  // recover -> resets counter
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None), static_cast<int>(m.protectionAction()));
}

// Low (but not critical) battery asks for a warning.
void test_low_battery_warns() {
  Monitor m;
  m.update(0, true, 3.45f, 4, false);  // <= low voltage / percent, above critical
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Warn), static_cast<int>(m.protectionAction()));
}

// Healthy battery: no action.
void test_healthy_no_action() {
  Monitor m;
  m.update(0, true, 4.00f, 80, false);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None), static_cast<int>(m.protectionAction()));
}

// Absent cell clears presence and the critical counter without shutting down.
void test_absent_clears_state() {
  Monitor m;
  m.update(0, true, 3.20f, 0, false);  // one critical
  m.update(kMinute, false, 0.0f, 0, false);
  TEST_ASSERT_FALSE(m.present());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None), static_cast<int>(m.protectionAction()));
}

// Sample cadence: slow when healthy, fast when low, slowest while playing.
void test_sample_cadence() {
  Monitor m;
  m.update(0, true, 4.00f, 80, false);
  const uint32_t healthy = m.sampleIntervalMs(false);
  const uint32_t playing = m.sampleIntervalMs(true);
  TEST_ASSERT_TRUE(playing > healthy);  // playing stretches the interval

  Monitor low;
  low.update(0, true, 3.45f, 4, false);
  TEST_ASSERT_TRUE(low.sampleIntervalMs(false) < healthy);  // low battery polls faster
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_first_sample_seeds_directly);
  RUN_TEST(test_display_hysteresis);
  RUN_TEST(test_force_snaps_display);
  RUN_TEST(test_shutdown_after_consecutive_critical);
  RUN_TEST(test_single_critical_then_recover_no_shutdown);
  RUN_TEST(test_low_battery_warns);
  RUN_TEST(test_healthy_no_action);
  RUN_TEST(test_absent_clears_state);
  RUN_TEST(test_sample_cadence);
  return UNITY_END();
}
