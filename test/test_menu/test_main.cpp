#include <unity.h>

#include "app/Menu.h"

using menu::Screen;

void test_is_settings_screen() {
  TEST_ASSERT_TRUE(menu::isSettingsScreen(Screen::SettingsHome));
  TEST_ASSERT_TRUE(menu::isSettingsScreen(Screen::SettingsDisplay));
  TEST_ASSERT_TRUE(menu::isSettingsScreen(Screen::SettingsPacing));
  TEST_ASSERT_TRUE(menu::isSettingsScreen(Screen::WifiSettings));
  // Neighbours that are NOT settings screens.
  TEST_ASSERT_FALSE(menu::isSettingsScreen(Screen::Main));
  TEST_ASSERT_FALSE(menu::isSettingsScreen(Screen::WifiNetworks));
  TEST_ASSERT_FALSE(menu::isSettingsScreen(Screen::TypographyTuning));
  TEST_ASSERT_FALSE(menu::isSettingsScreen(Screen::FocusTimerGenres));
}

void test_is_focus_timer_screen() {
  TEST_ASSERT_TRUE(menu::isFocusTimerScreen(Screen::FocusTimerGenres));
  TEST_ASSERT_TRUE(menu::isFocusTimerScreen(Screen::FocusTimerSession));
  TEST_ASSERT_FALSE(menu::isFocusTimerScreen(Screen::Main));
  TEST_ASSERT_FALSE(menu::isFocusTimerScreen(Screen::SettingsHome));
}

void test_wrap_selection_moves_within_bounds() {
  TEST_ASSERT_EQUAL_UINT(1, menu::wrapSelection(0, 1, 5));
  TEST_ASSERT_EQUAL_UINT(3, menu::wrapSelection(4, -1, 5));
  TEST_ASSERT_EQUAL_UINT(2, menu::wrapSelection(2, 0, 5));  // no movement
}

void test_wrap_selection_wraps_at_ends() {
  // Past the end -> first item.
  TEST_ASSERT_EQUAL_UINT(0, menu::wrapSelection(4, 1, 5));
  // Before the first -> last item.
  TEST_ASSERT_EQUAL_UINT(4, menu::wrapSelection(0, -1, 5));
}

void test_wrap_selection_single_item_stays() {
  TEST_ASSERT_EQUAL_UINT(0, menu::wrapSelection(0, 1, 1));
  TEST_ASSERT_EQUAL_UINT(0, menu::wrapSelection(0, -1, 1));
}

void test_wrap_selection_empty_list_returns_current() {
  // Empty list: caller guards on this too, but the function must be safe.
  TEST_ASSERT_EQUAL_UINT(7, menu::wrapSelection(7, 1, 0));
  TEST_ASSERT_EQUAL_UINT(0, menu::wrapSelection(0, -1, 0));
}

void test_wrap_selection_single_step_not_modulo() {
  // A jump larger than the list wraps once to the near end, matching the
  // device's long-standing behaviour (not a modulo).
  TEST_ASSERT_EQUAL_UINT(0, menu::wrapSelection(1, 5, 3));    // overshoot forward -> first
  TEST_ASSERT_EQUAL_UINT(2, menu::wrapSelection(1, -5, 3));   // overshoot back -> last
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_is_settings_screen);
  RUN_TEST(test_is_focus_timer_screen);
  RUN_TEST(test_wrap_selection_moves_within_bounds);
  RUN_TEST(test_wrap_selection_wraps_at_ends);
  RUN_TEST(test_wrap_selection_single_item_stays);
  RUN_TEST(test_wrap_selection_empty_list_returns_current);
  RUN_TEST(test_wrap_selection_single_step_not_modulo);
  return UNITY_END();
}
