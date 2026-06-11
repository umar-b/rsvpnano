#include <unity.h>

#include <set>

#include "standby/BookCoverDrift.h"

void test_step_advances_once_per_interval() {
  TEST_ASSERT_EQUAL_UINT32(0, standby::bookCoverDriftStep(0));
  TEST_ASSERT_EQUAL_UINT32(0, standby::bookCoverDriftStep(standby::kDriftIntervalMs - 1));
  TEST_ASSERT_EQUAL_UINT32(1, standby::bookCoverDriftStep(standby::kDriftIntervalMs));
  TEST_ASSERT_EQUAL_UINT32(2, standby::bookCoverDriftStep(2 * standby::kDriftIntervalMs));
  TEST_ASSERT_EQUAL_UINT32(10, standby::bookCoverDriftStep(10 * standby::kDriftIntervalMs + 5));
}

void test_offset_constant_within_interval() {
  const standby::DriftOffset a = standby::bookCoverDrift(0, 20, 16, 0);
  const standby::DriftOffset b =
      standby::bookCoverDrift(standby::kDriftIntervalMs - 1, 20, 16, 0);
  TEST_ASSERT_EQUAL_INT(a.dx, b.dx);
  TEST_ASSERT_EQUAL_INT(a.dy, b.dy);
}

void test_offset_changes_across_interval() {
  // The first jump must move the card (the ring's second entry is non-zero).
  const standby::DriftOffset a = standby::bookCoverDrift(0, 20, 16, 0);
  const standby::DriftOffset b = standby::bookCoverDrift(standby::kDriftIntervalMs, 20, 16, 0);
  const bool moved = (a.dx != b.dx) || (a.dy != b.dy);
  TEST_ASSERT_TRUE(moved);
}

void test_offset_within_budget() {
  const int16_t maxX = 20;
  const int16_t maxY = 16;
  for (uint32_t i = 0; i < 50; ++i) {
    const standby::DriftOffset o =
        standby::bookCoverDrift(i * standby::kDriftIntervalMs, maxX, maxY, 0);
    TEST_ASSERT_TRUE(o.dx >= -maxX && o.dx <= maxX);
    TEST_ASSERT_TRUE(o.dy >= -maxY && o.dy <= maxY);
  }
}

void test_visits_multiple_positions() {
  // Over a full cycle the card should occupy several distinct positions so wear
  // is spread, not pinned to one spot.
  std::set<int> positions;
  for (uint32_t i = 0; i < 20; ++i) {
    const standby::DriftOffset o =
        standby::bookCoverDrift(i * standby::kDriftIntervalMs, 10, 10, 0);
    positions.insert(o.dx * 1000 + o.dy);
  }
  TEST_ASSERT_TRUE(positions.size() >= 5);
}

void test_seed_phases_the_cycle() {
  // A different seed should generally start the ring at a different offset.
  const standby::DriftOffset a = standby::bookCoverDrift(0, 20, 16, 0);
  const standby::DriftOffset b = standby::bookCoverDrift(0, 20, 16, 1);
  const bool differs = (a.dx != b.dx) || (a.dy != b.dy);
  TEST_ASSERT_TRUE(differs);
}

void test_deterministic() {
  const standby::DriftOffset a = standby::bookCoverDrift(123456, 20, 16, 3);
  const standby::DriftOffset b = standby::bookCoverDrift(123456, 20, 16, 3);
  TEST_ASSERT_EQUAL_INT(a.dx, b.dx);
  TEST_ASSERT_EQUAL_INT(a.dy, b.dy);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_step_advances_once_per_interval);
  RUN_TEST(test_offset_constant_within_interval);
  RUN_TEST(test_offset_changes_across_interval);
  RUN_TEST(test_offset_within_budget);
  RUN_TEST(test_visits_multiple_positions);
  RUN_TEST(test_seed_phases_the_cycle);
  RUN_TEST(test_deterministic);
  return UNITY_END();
}
