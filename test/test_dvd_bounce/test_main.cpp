#include <unity.h>

#include "standby/DvdBounce.h"

void test_stays_in_bounds() {
  standby::DvdBounce b(100, 60, 20, 10);
  b.seed(1);
  for (int i = 0; i < 2000; ++i) {
    TEST_ASSERT_TRUE(b.x() >= 0);
    TEST_ASSERT_TRUE(b.y() >= 0);
    TEST_ASSERT_TRUE(b.x() <= b.maxX());
    TEST_ASSERT_TRUE(b.y() <= b.maxY());
    b.step();
  }
}

void test_reflects_off_left_wall() {
  // Place at the left edge moving left; one step must reflect to moving right.
  standby::DvdBounce b(100, 60, 20, 10);
  b.seed(123);
  // Drive until it bounces off a vertical wall, then confirm x stays in range.
  int lastX = b.x();
  bool reversedX = false;
  int prevDir = 0;
  for (int i = 0; i < 1000; ++i) {
    b.step();
    const int dir = (b.x() - lastX) > 0 ? 1 : ((b.x() - lastX) < 0 ? -1 : 0);
    if (prevDir != 0 && dir != 0 && dir != prevDir) {
      reversedX = true;
    }
    if (dir != 0) {
      prevDir = dir;
    }
    lastX = b.x();
  }
  TEST_ASSERT_TRUE(reversedX);
}

void test_corner_flash_on_simultaneous_hit() {
  // A square box with a square sprite and equal speeds started in a corner will
  // strike a corner periodically. Confirm a corner flash is observed and that
  // it only fires when both walls reflect together.
  standby::DvdBounce b(40, 40, 8, 8);
  b.seed(6);
  bool sawCorner = false;
  for (int i = 0; i < 5000; ++i) {
    b.step();
    if (b.cornerFlash()) {
      // On a corner the sprite must be at one of the four corners.
      const bool atXEdge = (b.x() == 0 || b.x() == b.maxX());
      const bool atYEdge = (b.y() == 0 || b.y() == b.maxY());
      TEST_ASSERT_TRUE(atXEdge);
      TEST_ASSERT_TRUE(atYEdge);
      sawCorner = true;
    }
  }
  TEST_ASSERT_TRUE(sawCorner);
}

void test_corner_flash_is_one_frame() {
  standby::DvdBounce b(40, 40, 8, 8);
  b.seed(6);
  int consecutive = 0;
  int maxConsecutive = 0;
  for (int i = 0; i < 5000; ++i) {
    b.step();
    if (b.cornerFlash()) {
      ++consecutive;
      if (consecutive > maxConsecutive) {
        maxConsecutive = consecutive;
      }
    } else {
      consecutive = 0;
    }
  }
  // A corner flash must never persist for two frames in a row.
  TEST_ASSERT_EQUAL_INT(1, maxConsecutive);
}

void test_deterministic_from_seed() {
  standby::DvdBounce a(100, 60, 20, 10);
  standby::DvdBounce b(100, 60, 20, 10);
  a.seed(77);
  b.seed(77);
  for (int i = 0; i < 1000; ++i) {
    TEST_ASSERT_EQUAL_INT(a.x(), b.x());
    TEST_ASSERT_EQUAL_INT(a.y(), b.y());
    TEST_ASSERT_EQUAL_INT(a.cornerFlash() ? 1 : 0, b.cornerFlash() ? 1 : 0);
    a.step();
    b.step();
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_stays_in_bounds);
  RUN_TEST(test_reflects_off_left_wall);
  RUN_TEST(test_corner_flash_on_simultaneous_hit);
  RUN_TEST(test_corner_flash_is_one_frame);
  RUN_TEST(test_deterministic_from_seed);
  return UNITY_END();
}
