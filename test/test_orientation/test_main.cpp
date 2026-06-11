#include <unity.h>

#include "motion/Orientation.h"

namespace {

constexpr uint32_t kStableMs = 700;  // mirrors Orientation.cpp's window

int as_int(orientation::Side s) { return static_cast<int>(s); }

}  // namespace

void test_classify_each_face() {
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::ShortSideA),
                        as_int(orientation::classify(1.0f, 0.0f, 0.0f)));
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::ShortSideB),
                        as_int(orientation::classify(-1.0f, 0.0f, 0.0f)));
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::LongSide),
                        as_int(orientation::classify(0.0f, 1.0f, 0.0f)));
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::FlatBack),
                        as_int(orientation::classify(0.0f, 0.0f, 0.9f)));
}

void test_classify_between_faces_is_unknown() {
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown),
                        as_int(orientation::classify(0.5f, 0.5f, 0.5f)));
}

void test_stabilizer_starts_unknown() {
  orientation::Stabilizer s;
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
}

void test_stabilizer_promotes_only_after_window() {
  orientation::Stabilizer s;
  s.update(0, orientation::Side::ShortSideA);
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
  s.update(kStableMs - 1, orientation::Side::ShortSideA);
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
  s.update(kStableMs, orientation::Side::ShortSideA);
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::ShortSideA), as_int(s.stable()));
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::ShortSideA), as_int(s.raw()));
}

void test_stabilizer_flicker_never_settles() {
  orientation::Stabilizer s;
  s.update(0, orientation::Side::ShortSideA);
  s.update(400, orientation::Side::LongSide);   // changed -> timer restarts
  s.update(700, orientation::Side::ShortSideA);  // changed again -> restarts
  // No single face has persisted a full window, so nothing settled.
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
}

void test_stabilizer_new_face_needs_its_own_window() {
  orientation::Stabilizer s;
  s.update(0, orientation::Side::ShortSideA);
  s.update(kStableMs, orientation::Side::ShortSideA);  // settled A
  s.update(kStableMs + 1, orientation::Side::LongSide);  // candidate B
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::ShortSideA), as_int(s.stable()));
  s.update(kStableMs + 1 + kStableMs, orientation::Side::LongSide);  // B held a full window
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::LongSide), as_int(s.stable()));
}

void test_mark_unavailable_clears_stable() {
  orientation::Stabilizer s;
  s.update(0, orientation::Side::ShortSideA);
  s.update(kStableMs, orientation::Side::ShortSideA);
  s.markUnavailable();
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.raw()));
}

void test_reset_clears_everything() {
  orientation::Stabilizer s;
  s.update(0, orientation::Side::LongSide);
  s.update(kStableMs, orientation::Side::LongSide);
  s.reset();
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
  // After reset, the window starts over from the next sample.
  s.update(10000, orientation::Side::LongSide);
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::Unknown), as_int(s.stable()));
  s.update(10000 + kStableMs, orientation::Side::LongSide);
  TEST_ASSERT_EQUAL_INT(as_int(orientation::Side::LongSide), as_int(s.stable()));
}

void test_screen_down_keys_on_z_sign() {
  // This board reads z ~ -1 g screen-down, so callers pass faceDownZSign = -1.
  TEST_ASSERT_TRUE(orientation::isScreenDown(0.02f, 0.05f, -0.99f, -1));
  // Screen up (z ~ +1) must NOT count as down.
  TEST_ASSERT_FALSE(orientation::isScreenDown(0.0f, 0.05f, 1.03f, -1));
  // Not flat (resting on a long side) is never screen-down.
  TEST_ASSERT_FALSE(orientation::isScreenDown(0.0f, -0.94f, 0.0f, -1));
  // Sign flips with the board convention.
  TEST_ASSERT_TRUE(orientation::isScreenDown(0.0f, 0.0f, 0.99f, 1));
  TEST_ASSERT_FALSE(orientation::isScreenDown(0.0f, 0.0f, -0.99f, 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_classify_each_face);
  RUN_TEST(test_screen_down_keys_on_z_sign);
  RUN_TEST(test_classify_between_faces_is_unknown);
  RUN_TEST(test_stabilizer_starts_unknown);
  RUN_TEST(test_stabilizer_promotes_only_after_window);
  RUN_TEST(test_stabilizer_flicker_never_settles);
  RUN_TEST(test_stabilizer_new_face_needs_its_own_window);
  RUN_TEST(test_mark_unavailable_clears_stable);
  RUN_TEST(test_reset_clears_everything);
  return UNITY_END();
}
