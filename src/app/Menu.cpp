#include "app/Menu.h"

namespace menu {

bool isSettingsScreen(Screen screen) {
  return screen == Screen::SettingsHome || screen == Screen::SettingsDisplay ||
         screen == Screen::SettingsPacing || screen == Screen::WifiSettings;
}

bool isFocusTimerScreen(Screen screen) {
  return screen == Screen::FocusTimerGenres || screen == Screen::FocusTimerSession;
}

size_t wrapSelection(size_t current, int direction, size_t itemCount) {
  if (itemCount == 0) {
    return current;
  }
  const int next = static_cast<int>(current) + direction;
  if (next < 0) {
    return itemCount - 1;
  }
  if (next >= static_cast<int>(itemCount)) {
    return 0;
  }
  return static_cast<size_t>(next);
}

}  // namespace menu
