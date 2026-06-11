#pragma once

#include <stddef.h>
#include <stdint.h>

// Pure menu-navigation logic, split from the App god-object. App owns rendering
// and the per-screen actions (which touch prefs, storage, the display); this
// module owns the parts that are pure decisions: which screen is which kind, and
// how a selection index moves within a list. No Arduino, no I/O -- host-testable.
//
// App aliases `using MenuScreen = menu::Screen;` so its ~116 existing
// MenuScreen:: call sites are unchanged.
namespace menu {

// The menu screens the device can show. Mirrors the order App has always used;
// values are compared, never serialised, so the ordering is the only contract.
enum class Screen : uint8_t {
  Main = 0,
  SettingsHome,
  SettingsDisplay,
  SettingsPacing,
  WifiSettings,
  WifiNetworks,
  WifiSavedNetwork,
  TextEntry,
  TypographyTuning,
  BookPicker,
  ChapterPicker,
  BookmarkPicker,
  StarredPicker,
  RestartConfirm,
  SdCardRepairConfirm,
  UpdateConfirm,
  FocusTimerGenres,
  FocusTimerSession,
  // Per-book action sheet opened by a touch-hold on a book-picker row, and its
  // delete confirmation. Appended at the end -- Screen values are compared,
  // never serialised, so order of earlier entries is unchanged.
  BookActions,
  BookDeleteConfirm,
};

// The four screens that share the settings list + settingsSelectedIndex_. This
// predicate was copy-pasted nine times across App.cpp before it lived here.
bool isSettingsScreen(Screen screen);

// The two focus-timer screens.
bool isFocusTimerScreen(Screen screen);

// Move a selection index by `direction` within a list of `itemCount` items,
// wrapping once at each end (matching the device's long-standing behaviour:
// past the end -> first item, before the first -> last item). `direction` is
// normally +/-1. Returns `current` unchanged when the list is empty.
size_t wrapSelection(size_t current, int direction, size_t itemCount);

}  // namespace menu
