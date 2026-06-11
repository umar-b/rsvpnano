#include "app/App.h"

#include "app/TouchGesture.h"

#include <esp_sleep.h>
#include <esp_log.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>

#include "board/BoardConfig.h"
#include "settings/PreferenceKeys.h"
#include "settings/PreferenceSpec.h"

#ifndef RSVP_USB_TRANSFER_ENABLED
#define RSVP_USB_TRANSFER_ENABLED 0
#endif

#ifndef RSVP_USB_TRANSFER_AUTO_START
#define RSVP_USB_TRANSFER_AUTO_START 0
#endif

static const char *kAppTag = "app";
constexpr uint32_t kOtaCheckTaskStackBytes = 10240;
constexpr uint32_t kBootSplashMs = 750;
constexpr uint32_t kWpmFeedbackMs = 900;
constexpr uint32_t kStarOverlayMs = 800;
constexpr uint32_t kPowerOffHoldMs = 1600;
constexpr uint32_t kPowerOffReleaseWaitMs = 4000;
constexpr uint32_t kReaderDoubleTapWindowMs = 520;
constexpr uint32_t kThemeToggleHoldMs = 900;
constexpr uint32_t kScrollAnimationFrameMs = 16;
constexpr uint16_t kReaderDoubleTapSlopPx = 92;
constexpr uint16_t kFocusTimerCancelHoldMaxDriftPx = 20;
constexpr uint32_t kFocusTimerCancelHoldMs = 850;
constexpr size_t kContextPreviewWindowWords = 288;
constexpr size_t kContextPreviewAnchorLeadWords = 112;
constexpr size_t kContextPreviewMaxParagraphSnapWords = 48;
constexpr uint32_t kProgressSaveIntervalMs = 15000;
constexpr uint32_t kUsbTransferExitHoldMs = 1200;
constexpr size_t kTimeEstimateBlockWords = 256;
constexpr size_t kTimeEstimateBlocksPerUpdate = 1;
constexpr uint32_t kTimeEstimateProgressLogMs = 5000;
constexpr uint32_t kNominalBatteryRuntimeMinutes = 330;
constexpr uint32_t kBatteryLowWarningRepeatMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kBatteryWarningVisibleMs = 2500;
constexpr uint32_t kBatteryShutdownNoticeMs = 1500;
constexpr uint32_t kStandbyFrameMs = 160;
constexpr uint16_t kStandbyLifeCellPixels = 2;
constexpr uint16_t kStandbyLifeColumns = BoardConfig::DISPLAY_WIDTH / kStandbyLifeCellPixels;
constexpr uint16_t kStandbyLifeRows = BoardConfig::DISPLAY_HEIGHT / kStandbyLifeCellPixels;
constexpr uint32_t kChapterTransitionMs = 1400;
constexpr uint8_t kBrightnessLevels[] = {40, 55, 70, 85, 100};
constexpr uint8_t kNightBrightnessLevels[] = {35, 40, 45, 50, 55};
constexpr size_t kBrightnessLevelCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);
// Bind the option array to the shared domain so a new brightness level forces
// updating settings::kBrightnessIndexRange (which the companion validates).
static_assert(kBrightnessLevelCount == settings::kBrightnessIndexRange.max + 1,
              "brightness levels out of sync with settings::kBrightnessIndexRange");

namespace {

enum MenuItem : size_t {
  MenuResume,
  MenuChapters,
  MenuMarkFinished,
  MenuBookmarks,
  MenuStarred,
  MenuStats,
  MenuBooks,
  MenuArticles,
  MenuFocusTimer,
  MenuSettings,
  MenuSdCardCheck,
  MenuRssFeeds,
  MenuCompanionSync,
#if RSVP_USB_TRANSFER_ENABLED
  MenuUsbTransfer,
#endif
  MenuPowerOff,
  MenuItemCount,
};

enum SettingsItem : size_t {
  SettingsBack,
  SettingsDisplay,
  SettingsTypography,
  SettingsWordPacing,
  SettingsHandedness,
  SettingsBrightness,
  SettingsTheme,
  SettingsPhantomWords,
  SettingsFontSize,
  SettingsLongWords,
  SettingsComplexWords,
  SettingsPunctuation,
  SettingsReset,
  SettingsItemCount,
};

enum TypographyTuningItem : size_t {
  TypographyTuningBack,
  TypographyTuningFontSize,
  TypographyTuningTypeface,
  TypographyTuningPhantomWords,
  TypographyTuningFocusHighlight,
  TypographyTuningTracking,
  TypographyTuningAnchor,
  TypographyTuningGuideWidth,
  TypographyTuningGuideGap,
  TypographyTuningReset,
  TypographyTuningItemCount,
};

enum RestartConfirmItem : size_t {
  RestartConfirmNo,
  RestartConfirmYes,
  RestartConfirmItemCount,
};

enum SdCardRepairConfirmItem : size_t {
  SdCardRepairConfirmNo,
  SdCardRepairConfirmYes,
  SdCardRepairConfirmItemCount,
};

enum UpdateConfirmItem : size_t {
  UpdateConfirmSkip,
  UpdateConfirmUpdate,
  UpdateConfirmItemCount,
};

constexpr size_t kRestartConfirmHeaderRows = 1;
constexpr size_t kSdCardRepairConfirmHeaderRows = 1;
constexpr size_t kUpdateConfirmHeaderRows = 2;
constexpr size_t kSettingsBackIndex = 0;
constexpr size_t kSettingsHomePacingIndex = 1;
constexpr size_t kSettingsHomeDisplayIndex = 2;
constexpr size_t kSettingsHomeTypographyIndex = 3;
constexpr size_t kSettingsHomeWifiIndex = 4;
constexpr size_t kSettingsHomeUpdateIndex = 5;
constexpr size_t kSettingsDisplayThemeIndex = 1;
constexpr size_t kSettingsDisplayBrightnessIndex = 2;
constexpr size_t kSettingsDisplayHandednessIndex = 3;
constexpr size_t kSettingsDisplayGestureIndex = 4;
constexpr size_t kSettingsDisplayFooterIndex = 5;
constexpr size_t kSettingsDisplayBatteryIndex = 6;
constexpr size_t kSettingsDisplayScreensaverIndex = 7;
constexpr size_t kSettingsDisplayIdleStandbyIndex = 8;
constexpr size_t kSettingsDisplayMuteIndex = 9;
constexpr size_t kSettingsDisplayVolumeIndex = 10;
constexpr size_t kSettingsDisplayReaderBatteryIndex = 11;
constexpr size_t kSettingsDisplayReaderChapterIndex = 12;
constexpr size_t kSettingsDisplayReaderProgressIndex = 13;
constexpr size_t kSettingsDisplayLanguageIndex = 14;
constexpr size_t kSettingsDisplayMotionPauseIndex = 15;
constexpr size_t kSettingsPacingReadingModeIndex = 1;
constexpr size_t kSettingsPacingPauseModeIndex = 2;
constexpr size_t kSettingsPacingWpmIndex = 3;
constexpr size_t kSettingsPacingLongWordsIndex = 4;
constexpr size_t kSettingsPacingComplexityIndex = 5;
constexpr size_t kSettingsPacingPunctuationIndex = 6;
constexpr size_t kSettingsPacingResetIndex = 7;
constexpr size_t kWifiSettingsNetworkIndex = 1;
constexpr size_t kWifiSettingsChooseIndex = 2;
constexpr size_t kWifiSettingsAutoUpdateIndex = 3;
constexpr size_t kWifiSettingsForgetIndex = 4;
constexpr size_t kWifiSettingsOtaOwnerIndex = 5;
constexpr size_t kWifiSettingsFirmwareVersionIndex = 6;
constexpr size_t kWifiSettingsCheckNowIndex = 7;
constexpr size_t kWifiSettingsLastResultIndex = 8;

constexpr size_t kBookPickerBackIndex = 0;
constexpr size_t kChapterPickerBackIndex = 0;
constexpr size_t kChapterPickerFallbackIndex = 1;
constexpr size_t kBookmarkPickerBackIndex = 0;
constexpr size_t kBookmarkPickerDropIndex = 1;
constexpr size_t kBookmarkPickerFirstMarkIndex = 2;
constexpr size_t kWifiNetworksBackIndex = 0;
constexpr size_t kWifiNetworksFirstItemIndex = 1;
constexpr size_t kFocusTimerGenreBackIndex = 0;
constexpr size_t kFocusTimerGenreFirstIndex = 1;
// Preference keys are defined once in settings/PreferenceKeys.h and shared with
// the web companion; pull them into scope so existing call sites are unchanged.
using namespace settings;
constexpr size_t kReaderFontSizeCount = 3;
static_assert(kReaderFontSizeCount == settings::kReaderFontSizeRange.max + 1,
              "reader font sizes out of sync with settings::kReaderFontSizeRange");
constexpr size_t kPhantomBeforeCharTargets[] = {64, 96, 144};
constexpr size_t kPhantomAfterCharTargets[] = {96, 144, 208};
// Stored-value bounds are aliases onto the canonical domains in
// settings/PreferenceSpec.h, shared with CompanionSyncManager's strict path.
// Change a range in PreferenceSpec.h, not here; the device clamp and the
// companion validator both track it. Step/low-WPM constants are UI cadence,
// not domains, so they stay local.
constexpr uint16_t kPacingDelayMinMs = kPacingDelayMsRange.min;
constexpr uint16_t kPacingDelayMaxMs = kPacingDelayMsRange.max;
constexpr uint16_t kPacingDelayStepMs = 50;
constexpr uint16_t kDefaultPacingDelayMs = 200;
constexpr uint16_t kSettingsWpmMin = kWpmRange.min;
constexpr uint16_t kSettingsWpmLowMax = 100;
constexpr uint16_t kSettingsWpmLowStep = 10;
constexpr uint16_t kSettingsWpmMax = kWpmRange.max;
constexpr uint16_t kSettingsWpmHighStep = 25;
constexpr int8_t kTypographyTrackingMin = kTypographyTrackingRange.min;
constexpr int8_t kTypographyTrackingMax = kTypographyTrackingRange.max;
constexpr uint8_t kTypographyAnchorMin = kTypographyAnchorRange.min;
constexpr uint8_t kTypographyAnchorMax = kTypographyAnchorRange.max;
constexpr uint8_t kLeftHandAnchorOffset = 20;
constexpr uint8_t kLeftHandAnchorMin = kTypographyAnchorMin + kLeftHandAnchorOffset;
constexpr uint8_t kLeftHandAnchorMax = kTypographyAnchorMax + kLeftHandAnchorOffset;
constexpr uint8_t kTypographyGuideWidthMin = kTypographyGuideWidthRange.min;
constexpr uint8_t kTypographyGuideWidthMax = kTypographyGuideWidthRange.max;
constexpr uint8_t kTypographyGuideWidthStep = 2;
constexpr uint8_t kTypographyGuideGapMin = kTypographyGuideGapRange.min;
constexpr uint8_t kTypographyGuideGapMax = kTypographyGuideGapRange.max;

// Motion sensor poll cadence: fast enough to catch a flick. The standby and
// flick timing constants live in motion::StandbyDecider and
// motion::FlickDetector; only the I2C pacing is App's.
constexpr uint32_t kImuShortcutPollMs = 40;  // ~25 Hz
constexpr const char *kTypographyPreviewWords[] = {
    "minimum",
    "encyclopaedia",
    "state-of-the-art",
    "HTTP/2",
    "well-known",
    "rhythms",
    "illumination",
    "WAVEFORM",
    "I",
};
constexpr size_t kTypographyPreviewWordCount =
    sizeof(kTypographyPreviewWords) / sizeof(kTypographyPreviewWords[0]);
constexpr size_t kWifiPasswordMaxLength = 63;
constexpr uint16_t kKeyboardMarginX = 8;
constexpr uint16_t kKeyboardTopY = 48;
constexpr uint16_t kKeyboardRowGap = 4;
constexpr uint16_t kKeyboardRowHeight = 27;

void logApp(const char *message) {
  ESP_LOGI(kAppTag, "%s", message);
  Serial.printf("[app] %s\n", message);
}

String displayNameForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  String name = separator >= 0 ? path.substring(separator + 1) : path;
  String lowered = name;
  lowered.toLowerCase();
  if (lowered.endsWith(".txt")) {
    name.remove(name.length() - 4);
  }
  if (lowered.endsWith(".rsvp")) {
    name.remove(name.length() - 5);
  }
  return name;
}

int clampIntSetting(int value, int minValue, int maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}

int nextCyclicSetting(int value, int minValue, int maxValue, int step = 1) {
  step = std::max(1, step);
  const int normalized = clampIntSetting(value, minValue, maxValue);
  int next = normalized + step;
  if (next > maxValue) {
    next = minValue;
  }
  return next;
}

uint16_t nextReaderWpmSetting(uint16_t value) {
  int normalized = clampIntSetting(value, kSettingsWpmMin, kSettingsWpmMax);
  if (normalized < static_cast<int>(kSettingsWpmLowMax)) {
    normalized += kSettingsWpmLowStep;
    if (normalized > static_cast<int>(kSettingsWpmLowMax)) {
      normalized = kSettingsWpmLowMax;
    }
    return static_cast<uint16_t>(normalized);
  }

  int next = normalized + kSettingsWpmHighStep;
  if (next > static_cast<int>(kSettingsWpmMax)) {
    next = kSettingsWpmMin;
  }
  return static_cast<uint16_t>(next);
}

DisplayManager::TypographyConfig defaultTypographyConfig() {
  return DisplayManager::TypographyConfig();
}

bool wifiNetworkRequiresPassword(uint8_t authMode) {
  return static_cast<wifi_auth_mode_t>(authMode) != WIFI_AUTH_OPEN;
}

String wifiSecurityLabel(uint8_t authMode) {
  return wifiNetworkRequiresPassword(authMode) ? "Secure" : "Open";
}

String maskedValue(const String &value) {
  String masked;
  masked.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    masked += '*';
  }
  return masked;
}

const char *keyboardRowText(uint8_t modeValue, size_t rowIndex) {
  static constexpr const char *kLowerRows[] = {
      "qwertyuiop",
      "asdfghjkl",
      "zxcvbnm",
  };
  static constexpr const char *kUpperRows[] = {
      "QWERTYUIOP",
      "ASDFGHJKL",
      "ZXCVBNM",
  };
  static constexpr const char *kSymbolRows[] = {
      "1234567890",
      "!@#$%^&*?",
      "-_=+/:;.,",
  };

  if (rowIndex >= 3) {
    return "";
  }

  switch (modeValue) {
    case 1:
      return kUpperRows[rowIndex];
    case 2:
      return kSymbolRows[rowIndex];
    default:
      return kLowerRows[rowIndex];
  }
}

String storedOrFallbackLabel(const String &value, const String &fallback) {
  return value.isEmpty() ? fallback : value;
}

void copyOtaLabel(char *destination, size_t destinationSize, const String &source) {
  if (destination == nullptr || destinationSize == 0) {
    return;
  }

  const size_t copyLength = std::min(destinationSize - 1, source.length());
  for (size_t i = 0; i < copyLength; ++i) {
    destination[i] = source[i];
  }
  destination[copyLength] = '\0';
}

bool sdCardFolderRepairNeeded(const StorageManager::DiagnosticResult &result) {
  return result.mounted &&
         (!result.booksDirectory || !result.bookFilesDirectory ||
          !result.articleFilesDirectory || !result.configDirectory);
}

DisplayManager::ReaderTypeface readerTypefaceFromSetting(uint8_t value) {
  switch (static_cast<DisplayManager::ReaderTypeface>(value)) {
    case DisplayManager::ReaderTypeface::Standard:
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return static_cast<DisplayManager::ReaderTypeface>(value);
  }
  return DisplayManager::ReaderTypeface::Standard;
}

DisplayManager::ReaderTypeface nextReaderTypeface(DisplayManager::ReaderTypeface current) {
  switch (readerTypefaceFromSetting(static_cast<uint8_t>(current))) {
    case DisplayManager::ReaderTypeface::Standard:
      return DisplayManager::ReaderTypeface::AtkinsonHyperlegible;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return DisplayManager::ReaderTypeface::OpenDyslexic;
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    default:
      return DisplayManager::ReaderTypeface::Standard;
  }
}

App::ReaderMode readerModeFromSetting(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(App::ReaderMode::Scroll):
    case 2:  // Migrate the removed word-scroll mode to page scroll.
      return App::ReaderMode::Scroll;
    case static_cast<uint8_t>(App::ReaderMode::Rsvp):
    default:
      return App::ReaderMode::Rsvp;
  }
}

App::HandednessMode handednessModeFromSetting(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(App::HandednessMode::Left):
      return App::HandednessMode::Left;
    case static_cast<uint8_t>(App::HandednessMode::Right):
    default:
      return App::HandednessMode::Right;
  }
}

App::HandednessMode nextHandednessMode(App::HandednessMode current) {
  switch (handednessModeFromSetting(static_cast<uint8_t>(current))) {
    case App::HandednessMode::Left:
      return App::HandednessMode::Right;
    case App::HandednessMode::Right:
    default:
      return App::HandednessMode::Left;
  }
}

App::ReaderMode nextReaderMode(App::ReaderMode current) {
  switch (readerModeFromSetting(static_cast<uint8_t>(current))) {
    case App::ReaderMode::Rsvp:
      return App::ReaderMode::Scroll;
    case App::ReaderMode::Scroll:
    default:
      return App::ReaderMode::Rsvp;
  }
}

uint16_t pacingDelayMsForLegacyLevel(uint8_t levelIndex) {
  constexpr uint16_t kLegacyPacingDelayMs[] = {100, 150, 200, 250, 300};
  constexpr size_t kLegacyPacingLevelCount =
      sizeof(kLegacyPacingDelayMs) / sizeof(kLegacyPacingDelayMs[0]);

  if (levelIndex >= kLegacyPacingLevelCount) {
    levelIndex = 2;
  }
  return kLegacyPacingDelayMs[levelIndex];
}

uint16_t loadPacingDelayMs(Preferences &preferences, const char *key, const char *legacyKey) {
  if (preferences.isKey(key)) {
    return static_cast<uint16_t>(
        clampIntSetting(preferences.getUShort(key, kDefaultPacingDelayMs), kPacingDelayMinMs,
                        kPacingDelayMaxMs));
  }

  if (preferences.isKey(legacyKey)) {
    const uint16_t migratedDelayMs =
        pacingDelayMsForLegacyLevel(preferences.getUChar(legacyKey, 2));
    preferences.putUShort(key, migratedDelayMs);
    return migratedDelayMs;
  }

  return kDefaultPacingDelayMs;
}

}  // namespace

App::App() : button_(BoardConfig::PIN_BOOT_BUTTON), powerButton_(BoardConfig::PIN_PWR_BUTTON) {}

void App::begin() {
  BoardConfig::begin();
  button_.begin();
  powerButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ = !powerButton_.isHeld();
  powerButtonLongPressHandled_ = false;
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  preferences_.begin(kPrefsNamespace, false);
  brightnessLevelIndex_ = preferences_.getUChar(kPrefBrightness, brightnessLevelIndex_);
  if (brightnessLevelIndex_ >= kBrightnessLevelCount) {
    brightnessLevelIndex_ = kBrightnessLevelCount - 1;
  }
  phantomWordsEnabled_ = preferences_.getBool(kPrefPhantomWords, phantomWordsEnabled_);
  imuShortcutsEnabled_ = preferences_.getBool(kPrefImuShortcuts, imuShortcutsEnabled_);
  readerBatteryVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
  readerChapterVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
  readerProgressVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
  idleStandbyMinutes_ = preferences_.getUChar(kPrefIdleStandbyMin, idleStandbyMinutes_);
  standbyDecider_.setIdleTimeoutMs(static_cast<uint32_t>(idleStandbyMinutes_) * 60000UL);
  audioMuted_ = preferences_.getBool(kPrefAudioMuted, audioMuted_);
  audioVolumePercent_ = preferences_.getUChar(kPrefAudioVolume, audioVolumePercent_);
  if (audioVolumePercent_ > 100) {
    audioVolumePercent_ = 100;
  }
  uiLanguage_ =
      Localization::sanitizeLanguage(preferences_.getUChar(
          kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_)));
  readerMode_ = readerModeFromSetting(
      preferences_.getUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_)));
  handednessMode_ = handednessModeFromSetting(
      preferences_.getUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_)));
  readerFontSizeIndex_ = preferences_.getUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  if (readerFontSizeIndex_ >= kReaderFontSizeCount) {
    readerFontSizeIndex_ = 0;
  }
  switch (preferences_.getUChar(kPrefFooterMetricMode,
                                static_cast<uint8_t>(footerMetricMode_))) {
    case static_cast<uint8_t>(FooterMetricMode::ChapterTime):
      footerMetricMode_ = FooterMetricMode::ChapterTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::BookTime):
      footerMetricMode_ = FooterMetricMode::BookTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::Percentage):
    default:
      footerMetricMode_ = FooterMetricMode::Percentage;
      break;
  }
  switch (preferences_.getUChar(kPrefBatteryLabelMode,
                                static_cast<uint8_t>(batteryLabelMode_))) {
    case static_cast<uint8_t>(BatteryLabelMode::TimeRemaining):
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Voltage):
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Percent):
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }
  switch (preferences_.getUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_))) {
    case static_cast<uint8_t>(ScreensaverMode::Maze):
      screensaverMode_ = ScreensaverMode::Maze;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Voronoi):
      screensaverMode_ = ScreensaverMode::Voronoi;
      break;
    case static_cast<uint8_t>(ScreensaverMode::ScreenOff):
      screensaverMode_ = ScreensaverMode::ScreenOff;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Life):
    default:
      screensaverMode_ = ScreensaverMode::Life;
      break;
  }
  switch (preferences_.getUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_))) {
    case static_cast<uint8_t>(PauseMode::Instant):
      pauseMode_ = PauseMode::Instant;
      break;
    case static_cast<uint8_t>(PauseMode::SentenceEnd):
    default:
      pauseMode_ = PauseMode::SentenceEnd;
      break;
  }
  pacingLongWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingLongMs, kPrefLegacyPacingLong);
  pacingComplexWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingComplexMs, kPrefLegacyPacingComplex);
  pacingPunctuationDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingPunctuationMs, kPrefLegacyPacingPunctuation);
  accurateTimeEstimateEnabled_ = true;
  typographyConfig_ = defaultTypographyConfig();
  typographyConfig_.typeface = readerTypefaceFromSetting(
      preferences_.getUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface)));
  typographyConfig_.focusHighlight =
      preferences_.getBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
  typographyConfig_.trackingPx = static_cast<int8_t>(clampIntSetting(
      preferences_.getChar(kPrefTypographyTracking, typographyConfig_.trackingPx),
      kTypographyTrackingMin, kTypographyTrackingMax));
  typographyConfig_.anchorPercent = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent),
      kTypographyAnchorMin, kTypographyAnchorMax));
  typographyConfig_.guideHalfWidth = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth),
      kTypographyGuideWidthMin, kTypographyGuideWidthMax));
  typographyConfig_.guideGap = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap),
      kTypographyGuideGapMin, kTypographyGuideGapMax));
  darkMode_ = preferences_.getBool(kPrefDarkMode, darkMode_);
  nightMode_ = preferences_.getBool(kPrefNightMode, nightMode_);
  loadGestureConfig();
  applyHandednessSettings(0, false);
  applyDisplayPreferences(0, false);
  applyTypographySettings(0, false);
  applyPacingSettings();
  bootStartedMs_ = millis();
  standbyDecider_.noteActivity(bootStartedMs_);
  lastStateLogMs_ = bootStartedMs_;
  lastScrollAnimationRenderMs_ = 0;
  Serial.printf("[app] version=%s\n", otaUpdater_.currentVersion().c_str());

  logApp("Initializing hardware modules");
  const bool displayReady = display_.begin();
  updateBatteryStatus(bootStartedMs_, true);

  if (displayReady) {
    display_.renderCenteredWord("READY");
    logApp("Display init ok");
  } else {
    ESP_LOGE(kAppTag, "Display init failed");
    Serial.println("[app] Display init failed");
  }

  touchInitialized_ = touch_.begin();
  audio_.begin();
  applyAudioSettings();
  focusTimer_.begin();
  imuShortcutSensorReady_ = imuShortcutImu_.begin();

#if RSVP_USB_TRANSFER_ENABLED && RSVP_USB_TRANSFER_AUTO_START
  state_ = AppState::Booting;
  Serial.println("[app] USB transfer auto-start active");
  enterUsbTransfer(millis());
  return;
#endif

  display_.renderProgress("SD", "Loading books", "Use SD converter for EPUB", 0);
  storageReady_ = storage_.begin();
  loadReadingStats();
  const uint16_t savedWpm = preferences_.getUShort(kPrefWpm, reader_.wpm());
  reader_.setWpm(savedWpm);

  pendingBootBookLoad_ = prepareBootBookLoad();
  if (!pendingBootBookLoad_) {
    usingStorageBook_ = false;
    chapterMarkers_.clear();
    paragraphStarts_.clear();
    currentBookPath_ = "";
    currentBookTitle_ = "Demo";
    reader_.begin(bootStartedMs_);
    invalidateContextPreviewWindow();
    rebuildTimeEstimateCache();
    Serial.println("[app] using built-in demo text");
  } else {
    currentBookTitle_ = storage_.bookDisplayName(pendingBootBookIndex_);
    if (currentBookTitle_.isEmpty()) {
      currentBookTitle_ = "Loading book";
    }
  }

  maybeAutoCheckForUpdates(bootStartedMs_);
  Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                static_cast<unsigned long>(reader_.wordIntervalMs()));

  state_ = AppState::Booting;
  Serial.println("[app] READY splash active");
}

void App::update(uint32_t nowMs) {
  button_.update(nowMs);
  powerButton_.update(nowMs);
  if (button_.isHeld() || powerButton_.isHeld() || button_.wasPressedEvent() ||
      powerButton_.wasPressedEvent() || button_.wasReleasedEvent() ||
      powerButton_.wasReleasedEvent()) {
    noteUserInput(nowMs);
  }
  const bool standbyComboConsumed = handleStandbyCombo(nowMs);
  if (!standbyComboConsumed) {
    handleBootButton(nowMs);
    handlePowerButton(nowMs);
  }
  if (powerOffStarted_) {
    return;
  }

  const bool batteryChanged = updateBatteryStatus(nowMs);
  if (powerOffStarted_) {
    return;
  }

  if (batteryWarningOverlayVisible_) {
    updateBatteryWarningOverlay(nowMs);
    if (batteryWarningOverlayVisible_) {
      if (nowMs - lastStateLogMs_ > 1500) {
        lastStateLogMs_ = nowMs;
        ESP_LOGI(kAppTag, "state=%s", stateName(state_));
        Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                      static_cast<unsigned long>(nowMs));
      }
      return;
    }
  }

  if (state_ == AppState::Standby) {
    handleStandbyTouchWake(nowMs);
    if (state_ != AppState::Standby) {
      return;
    }
    updateStandbyDecision(nowMs);  // lift-to-wake
    if (state_ != AppState::Standby) {
      return;
    }
    updateStandbyScreensaver(nowMs);
    if (nowMs - lastStateLogMs_ > 1500) {
      lastStateLogMs_ = nowMs;
      ESP_LOGI(kAppTag, "state=%s", stateName(state_));
      Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                    static_cast<unsigned long>(nowMs));
    }
    return;
  }

  pollOtaCheckResult(nowMs);
  updateState(nowMs);
  loadPendingBootBook(nowMs);
  maybeOpenUpdateConfirm(nowMs);
  updateFocusTimer(nowMs);
  updateReader(nowMs);
  updateStandbyDecision(nowMs);
  handleTouch(nowMs);
  updateWpmFeedback(nowMs);
  updateStarOverlay(nowMs);
  maybeSaveReadingPosition(nowMs);
  updateTimeEstimateBuild(nowMs);
  if (state_ == AppState::Standby) {
    return;
  }

  if (batteryChanged && (state_ == AppState::Paused || state_ == AppState::Playing)) {
    renderActiveReader(nowMs);
  } else if (batteryChanged && state_ == AppState::Menu) {
    renderMenu();
  }

  if (nowMs - lastStateLogMs_ > 1500) {
    lastStateLogMs_ = nowMs;
    ESP_LOGI(kAppTag, "state=%s", stateName(state_));
    Serial.printf("[app] state=%s ms=%lu\n", stateName(state_),
                  static_cast<unsigned long>(nowMs));
  }
}

const char *App::stateName(AppState state) const {
  switch (state) {
    case AppState::Booting:
      return "Booting";
    case AppState::Paused:
      return "Paused";
    case AppState::Playing:
      return "Playing";
    case AppState::Menu:
      return "Menu";
    case AppState::CompanionSync:
      return "CompanionSync";
    case AppState::UsbTransfer:
      return "UsbTransfer";
    case AppState::Standby:
      return "Standby";
    case AppState::Sleeping:
      return "Sleeping";
  }
  return "Unknown";
}

const char *App::touchPhaseName(TouchPhase phase) const {
  switch (phase) {
    case TouchPhase::Start:
      return "Start";
    case TouchPhase::Move:
      return "Move";
    case TouchPhase::End:
      return "End";
  }
  return "Unknown";
}

void App::setState(AppState nextState, uint32_t nowMs) {
  if (nextState == state_) {
    return;
  }

  const AppState previousState = state_;
  if (previousState == AppState::Menu && nextState != AppState::Menu) {
    flushPendingTimeEstimateRebuild();
  }

  // Reading-stats session boundaries: count only words advanced while Playing.
  if (previousState == AppState::Playing && nextState != AppState::Playing) {
    endStatsSession(nowMs);
  } else if (previousState != AppState::Playing && nextState == AppState::Playing) {
    beginStatsSession(nowMs);
  }

  if (nextState != AppState::Paused) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    contextViewVisible_ = false;
    invalidateContextPreviewWindow();
    wpmFeedbackVisible_ = false;
  }
  if (nextState != AppState::Playing) {
    touchPlayHeld_ = false;
    playLocked_ = false;
    pauseAtSentenceEndRequested_ = false;
    chapterTransitionVisible_ = false;
  }
  if (nextState != AppState::Paused && nextState != AppState::Playing) {
    resetReaderTapTracking();
  }

  state_ = nextState;

  switch (state_) {
    case AppState::Paused:
      renderActiveReader(nowMs);
      break;
    case AppState::Playing:
      reader_.start(nowMs);
      renderActiveReader(nowMs);
      break;
    case AppState::Menu:
      renderMenu();
      break;
    case AppState::CompanionSync:
      display_.renderStatus("Sync", companionSync_.statusLine1(), companionSync_.statusLine2());
      break;
    case AppState::UsbTransfer:
      display_.renderStatus("USB", "Preparing SD", "Eject when done");
      break;
    case AppState::Standby:
      seedStandbyScreensaver(nowMs);
      updateStandbyScreensaver(nowMs, true);
      break;
    case AppState::Sleeping:
      display_.renderCenteredWord("SLEEP");
      break;
    case AppState::Booting:
      display_.renderCenteredWord("READY");
      break;
  }

  if (state_ == AppState::Paused && previousState == AppState::Playing) {
    saveReadingPosition(true);
  }

  ESP_LOGI(kAppTag, "state -> %s", stateName(state_));
  Serial.printf("[app] state -> %s at %lu ms\n", stateName(state_),
                static_cast<unsigned long>(nowMs));
}

void App::updateState(uint32_t nowMs) {
  if (state_ == AppState::Booting) {
    if (nowMs - bootStartedMs_ < kBootSplashMs) {
      return;
    }

    setState((touchPlayHeld_ || playLocked_ || pauseAtSentenceEndRequested_) ? AppState::Playing
                                                                              : AppState::Paused,
             nowMs);
    return;
  }

  if (state_ == AppState::UsbTransfer) {
    updateUsbTransfer(nowMs);
    return;
  }

  if (state_ == AppState::CompanionSync) {
    updateCompanionSync(nowMs);
    return;
  }

  if (state_ == AppState::Menu || state_ == AppState::Standby || state_ == AppState::Sleeping) {
    // Menu, standby, and sleeping state changes are driven by direct input and power events.
    return;
  }

  if (touchPlayHeld_ || playLocked_ || pauseAtSentenceEndRequested_) {
    setState(AppState::Playing, nowMs);
    return;
  }

  setState(AppState::Paused, nowMs);
}

motion::StandbyContext App::standbyContext() const {
  if (powerOffStarted_) {
    return motion::StandbyContext::Busy;
  }
  switch (state_) {
    case AppState::Playing:
      return motion::StandbyContext::Playing;
    case AppState::Paused:
      return motion::StandbyContext::Paused;
    case AppState::Menu:
      return motion::StandbyContext::Menu;
    default:
      // Standby itself maps to Busy too: the decider tracks its own standby
      // mode, and a Busy context keeps the idle timer inert until App is back
      // on a real screen.
      return motion::StandbyContext::Busy;
  }
}

void App::updateStandbyDecision(uint32_t nowMs) {
  const motion::StandbyContext context = standbyContext();

  // Poll the sensor on its own cadence; between polls (or with the sensor off
  // or absent) the decider still ticks so the idle timeout keeps running.
  bool sampled = false;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (imuShortcutsEnabled_ && imuShortcutSensorReady_ &&
      nowMs - imuShortcutLastPollMs_ >= kImuShortcutPollMs) {
    imuShortcutLastPollMs_ = nowMs;
    sampled = imuShortcutImu_.readAccel(x, y, z);
  }

  motion::StandbyVerdict verdict;
  if (sampled) {
    const bool reading = state_ == AppState::Playing || state_ == AppState::Paused;
    if (flickDetector_.updateWithSample(nowMs, x, y, z, reading)) {
      Serial.println("[motion] flick -> rewind sentence");
      noteUserInput(nowMs);  // flick navigation is activity for the idle timer
      rewindToPreviousSentence(nowMs);
    }
    verdict = standbyDecider_.updateWithSample(nowMs, x, y, z, context);
  } else {
    verdict = standbyDecider_.update(nowMs, context);
  }

  switch (verdict.kind) {
    case motion::StandbyVerdict::Kind::EnterStandby:
      Serial.println(verdict.screenOff ? "[standby] set down screen-down -> screen off"
                                       : "[standby] decider -> standby");
      standbyScreenOffForced_ = verdict.screenOff;
      enterStandby(nowMs);
      break;
    case motion::StandbyVerdict::Kind::Wake:
      Serial.println("[motion] lift -> wake");
      exitStandby(nowMs);
      break;
    case motion::StandbyVerdict::Kind::None:
      break;
  }
}

void App::updateReader(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  if (updateChapterTransition(nowMs)) {
    return;
  }

  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
    return;
  }

  const size_t previousIndex = reader_.currentIndex();
  const bool changed = reader_.update(nowMs, !pauseAtSentenceEndRequested_);
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }
  if (changed && maybeStartChapterTransition(previousIndex, reader_.currentIndex(), nowMs)) {
    return;
  }
  if (scrollModeEnabled()) {
    if (changed || nowMs - lastScrollAnimationRenderMs_ >= kScrollAnimationFrameMs) {
      renderScrollReader(nowMs);
      lastScrollAnimationRenderMs_ = nowMs;
    }
    return;
  }

  if (changed) {
    renderReaderWord();
  }
}

void App::maybeSaveReadingPosition(uint32_t nowMs) {
  if (!usingStorageBook_ || currentBookPath_.isEmpty() || state_ != AppState::Playing) {
    return;
  }

  if (nowMs - lastProgressSaveMs_ < kProgressSaveIntervalMs) {
    return;
  }

  lastProgressSaveMs_ = nowMs;
  saveReadingPosition(false);
}

bool App::handleStandbyCombo(uint32_t nowMs) {
  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::CompanionSync ||
      state_ == AppState::Sleeping || powerOffStarted_ || !bootButtonReleasedSinceBoot_ ||
      !powerButtonReleasedSinceBoot_) {
    return false;
  }

  const bool bothHeld = button_.isHeld() && powerButton_.isHeld();
  if (state_ == AppState::Standby) {
    const bool pastGrace = standbyDecider_.canWakeNow(nowMs);
    if (!bothHeld && !button_.isHeld() && !powerButton_.isHeld() && pastGrace) {
      standbyButtonsReleased_ = true;
    }

    if (bothHeld) {
      if (standbyButtonsReleased_) {
        bootButtonLongPressHandled_ = true;
        powerButtonLongPressHandled_ = true;
        exitStandby(nowMs);
      }
      return true;
    }

    if (standbyComboActive_) {
      standbyComboActive_ = false;
      standbyComboHandled_ = false;
      bootButtonLongPressHandled_ = false;
      powerButtonLongPressHandled_ = false;
      return true;
    }

    return false;
  }

  if (bothHeld) {
    if (!standbyComboActive_) {
      standbyComboActive_ = true;
      standbyComboHandled_ = true;
      standbyComboStartedMs_ = nowMs;
      bootButtonLongPressHandled_ = true;
      powerButtonLongPressHandled_ = true;
      enterStandby(nowMs);
    }
    return true;
  }

  if (standbyComboActive_) {
    standbyComboActive_ = false;
    standbyComboHandled_ = false;
    bootButtonLongPressHandled_ = false;
    powerButtonLongPressHandled_ = false;
    return true;
  }

  return false;
}

void App::handleBootButton(uint32_t nowMs) {
  if (state_ == AppState::Standby) {
    if (!standbyButtonsReleased_ && !button_.isHeld() && !powerButton_.isHeld() &&
        standbyDecider_.canWakeNow(nowMs)) {
      standbyButtonsReleased_ = true;
    }
    if (standbyButtonsReleased_ && button_.wasPressedEvent()) {
      bootButtonLongPressHandled_ = true;
      exitStandby(nowMs);
    }
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::CompanionSync ||
      state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  if (!bootButtonReleasedSinceBoot_) {
    if (!button_.isHeld()) {
      bootButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (button_.isHeld() && !bootButtonLongPressHandled_ &&
      button_.heldDurationMs(nowMs) >= kThemeToggleHoldMs) {
    bootButtonLongPressHandled_ = true;
    cycleThemeMode(nowMs);
    return;
  }

  if (!button_.wasReleasedEvent()) {
    return;
  }

  if (bootButtonLongPressHandled_) {
    bootButtonLongPressHandled_ = false;
    return;
  }

  if (button_.lastHoldDurationMs() < kThemeToggleHoldMs) {
    cycleBrightness();
  }
}

void App::handlePowerButton(uint32_t nowMs) {
  if (!powerButtonReleasedSinceBoot_) {
    if (!powerButton_.isHeld()) {
      powerButtonReleasedSinceBoot_ = true;
    }
    return;
  }

  if (state_ == AppState::Standby) {
    if (!standbyButtonsReleased_ && !button_.isHeld() && !powerButton_.isHeld() &&
        standbyDecider_.canWakeNow(nowMs)) {
      standbyButtonsReleased_ = true;
    }
    if (standbyButtonsReleased_ && powerButton_.wasPressedEvent()) {
      powerButtonLongPressHandled_ = true;
      exitStandby(nowMs);
    }
    return;
  }

  if (state_ == AppState::UsbTransfer || state_ == AppState::CompanionSync || powerOffStarted_) {
    return;
  }

  if (powerButtonLongPressHandled_ && powerButton_.isHeld()) {
    return;
  }

  if (state_ == AppState::Menu && isFocusTimerMenuScreen(menuScreen_) &&
      powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs) {
    powerButtonLongPressHandled_ = true;
    resetFocusTimer();
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  if (powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kPowerOffHoldMs) {
    powerButtonLongPressHandled_ = true;
    enterPowerOff(nowMs);
    return;
  }

  if (!powerButton_.wasReleasedEvent()) {
    return;
  }

  if (powerButtonLongPressHandled_) {
    powerButtonLongPressHandled_ = false;
    return;
  }

  toggleMenuFromPowerButton(nowMs);
}

void App::toggleMenuFromPowerButton(uint32_t nowMs) {
  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::CompanionSync || state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::Main) {
      setState(AppState::Paused, nowMs);
    } else {
      if (isFocusTimerMenuScreen(menuScreen_)) {
        resetFocusTimer();
      }
      menuScreen_ = MenuScreen::Main;
      renderMainMenu();
    }
    return;
  }

  openMainMenu(nowMs);
}

void App::openMainMenu(uint32_t nowMs) {
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  menuScreen_ = MenuScreen::Main;
  menuSelectedIndex_ = MenuResume;
  wpmFeedbackVisible_ = false;
  contextViewVisible_ = false;
  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }
  setState(AppState::Menu, nowMs);
}

uint8_t App::currentBrightnessPercent() const {
  return nightMode_ ? kNightBrightnessLevels[brightnessLevelIndex_]
                    : kBrightnessLevels[brightnessLevelIndex_];
}

void App::applyDisplayPreferences(uint32_t nowMs, bool rerender) {
  display_.setDarkMode(darkMode_);
  display_.setNightMode(nightMode_);
  display_.setBrightnessPercent(currentBrightnessPercent());

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu) {
    if (menu::isSettingsScreen(menuScreen_)) {
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    }
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
    return;
  }

  if (state_ == AppState::Booting) {
    display_.renderCenteredWord("READY");
  }
}

void App::applyHandednessSettings(uint32_t nowMs, bool rerender) {
  (void)nowMs;
  applyReaderUiOrientation();

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu &&
      menu::isSettingsScreen(menuScreen_)) {
    rebuildSettingsMenuItems();
  }

  applyTypographySettings(nowMs);
}

void App::reloadRuntimePreferences(uint32_t nowMs, bool rerender) {
  brightnessLevelIndex_ = preferences_.getUChar(kPrefBrightness, brightnessLevelIndex_);
  if (brightnessLevelIndex_ >= kBrightnessLevelCount) {
    brightnessLevelIndex_ = kBrightnessLevelCount - 1;
  }
  phantomWordsEnabled_ = preferences_.getBool(kPrefPhantomWords, phantomWordsEnabled_);
  imuShortcutsEnabled_ = preferences_.getBool(kPrefImuShortcuts, imuShortcutsEnabled_);
  readerBatteryVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
  readerChapterVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
  readerProgressVisibleWhilePlaying_ =
      preferences_.getBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
  idleStandbyMinutes_ = preferences_.getUChar(kPrefIdleStandbyMin, idleStandbyMinutes_);
  standbyDecider_.setIdleTimeoutMs(static_cast<uint32_t>(idleStandbyMinutes_) * 60000UL);
  audioMuted_ = preferences_.getBool(kPrefAudioMuted, audioMuted_);
  audioVolumePercent_ = preferences_.getUChar(kPrefAudioVolume, audioVolumePercent_);
  if (audioVolumePercent_ > 100) {
    audioVolumePercent_ = 100;
  }
  uiLanguage_ =
      Localization::sanitizeLanguage(preferences_.getUChar(
          kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_)));
  readerMode_ = readerModeFromSetting(
      preferences_.getUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_)));
  handednessMode_ = handednessModeFromSetting(
      preferences_.getUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_)));
  readerFontSizeIndex_ = preferences_.getUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  if (readerFontSizeIndex_ >= kReaderFontSizeCount) {
    readerFontSizeIndex_ = 0;
  }

  switch (preferences_.getUChar(kPrefFooterMetricMode,
                                static_cast<uint8_t>(footerMetricMode_))) {
    case static_cast<uint8_t>(FooterMetricMode::ChapterTime):
      footerMetricMode_ = FooterMetricMode::ChapterTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::BookTime):
      footerMetricMode_ = FooterMetricMode::BookTime;
      break;
    case static_cast<uint8_t>(FooterMetricMode::Percentage):
    default:
      footerMetricMode_ = FooterMetricMode::Percentage;
      break;
  }

  switch (preferences_.getUChar(kPrefBatteryLabelMode,
                                static_cast<uint8_t>(batteryLabelMode_))) {
    case static_cast<uint8_t>(BatteryLabelMode::TimeRemaining):
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Voltage):
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case static_cast<uint8_t>(BatteryLabelMode::Percent):
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }

  switch (preferences_.getUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_))) {
    case static_cast<uint8_t>(ScreensaverMode::Maze):
      screensaverMode_ = ScreensaverMode::Maze;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Voronoi):
      screensaverMode_ = ScreensaverMode::Voronoi;
      break;
    case static_cast<uint8_t>(ScreensaverMode::ScreenOff):
      screensaverMode_ = ScreensaverMode::ScreenOff;
      break;
    case static_cast<uint8_t>(ScreensaverMode::Life):
    default:
      screensaverMode_ = ScreensaverMode::Life;
      break;
  }

  switch (preferences_.getUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_))) {
    case static_cast<uint8_t>(PauseMode::Instant):
      pauseMode_ = PauseMode::Instant;
      break;
    case static_cast<uint8_t>(PauseMode::SentenceEnd):
    default:
      pauseMode_ = PauseMode::SentenceEnd;
      break;
  }

  pacingLongWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingLongMs, kPrefLegacyPacingLong);
  pacingComplexWordDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingComplexMs, kPrefLegacyPacingComplex);
  pacingPunctuationDelayMs_ =
      loadPacingDelayMs(preferences_, kPrefPacingPunctuationMs, kPrefLegacyPacingPunctuation);
  accurateTimeEstimateEnabled_ = true;

  typographyConfig_ = defaultTypographyConfig();
  typographyConfig_.typeface = readerTypefaceFromSetting(
      preferences_.getUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface)));
  typographyConfig_.focusHighlight =
      preferences_.getBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
  typographyConfig_.trackingPx = static_cast<int8_t>(clampIntSetting(
      preferences_.getChar(kPrefTypographyTracking, typographyConfig_.trackingPx),
      kTypographyTrackingMin, kTypographyTrackingMax));
  typographyConfig_.anchorPercent = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent),
      kTypographyAnchorMin, kTypographyAnchorMax));
  typographyConfig_.guideHalfWidth = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth),
      kTypographyGuideWidthMin, kTypographyGuideWidthMax));
  typographyConfig_.guideGap = static_cast<uint8_t>(clampIntSetting(
      preferences_.getUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap),
      kTypographyGuideGapMin, kTypographyGuideGapMax));
  darkMode_ = preferences_.getBool(kPrefDarkMode, darkMode_);
  nightMode_ = preferences_.getBool(kPrefNightMode, nightMode_);

  reader_.setWpm(preferences_.getUShort(kPrefWpm, reader_.wpm()));
  applyReaderUiOrientation();
  applyDisplayPreferences(nowMs, false);
  applyTypographySettings(nowMs, false);
  applyPacingSettings();
  applyAudioSettings();
  loadGestureConfig();
  if (rerender) {
    renderActiveReader(nowMs);
  }
}

void App::applyTypographySettings(uint32_t nowMs, bool rerender) {
  display_.setTypographyConfig(effectiveTypographyConfig());

  Serial.printf("[typography] face=%s highlight=%s track=%d anchor=%u guideWidth=%u guideGap=%u\n",
                readerTypefaceLabel().c_str(),
                focusHighlightLabel().c_str(),
                static_cast<int>(typographyConfig_.trackingPx),
                static_cast<unsigned int>(effectiveAnchorPercent()),
                static_cast<unsigned int>(typographyConfig_.guideHalfWidth),
                static_cast<unsigned int>(typographyConfig_.guideGap));

  if (!rerender) {
    return;
  }

  if (state_ == AppState::Menu) {
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  }
}

void App::cycleBrightness() {
  brightnessLevelIndex_ = static_cast<uint8_t>((brightnessLevelIndex_ + 1) % kBrightnessLevelCount);
  preferences_.putUChar(kPrefBrightness, brightnessLevelIndex_);
  const uint8_t percent = currentBrightnessPercent();
  Serial.printf("[display] brightness level %u/%u (%u%%)\n",
                static_cast<unsigned int>(brightnessLevelIndex_ + 1),
                static_cast<unsigned int>(kBrightnessLevelCount),
                static_cast<unsigned int>(percent));
  applyDisplayPreferences(millis());
}

void App::cycleThemeMode(uint32_t nowMs) {
  if (nightMode_) {
    nightMode_ = false;
    darkMode_ = true;
  } else if (darkMode_) {
    darkMode_ = false;
  } else {
    darkMode_ = true;
    nightMode_ = true;
  }

  preferences_.putBool(kPrefDarkMode, darkMode_);
  preferences_.putBool(kPrefNightMode, nightMode_);
  Serial.printf("[display] theme=%s\n", themeModeLabel().c_str());
  applyDisplayPreferences(nowMs);
}

void App::cycleUiLanguage(uint32_t nowMs) {
  uiLanguage_ = Localization::nextLanguage(uiLanguage_);
  preferences_.putUChar(kPrefUiLanguage, static_cast<uint8_t>(uiLanguage_));
  Serial.printf("[display] language=%s\n", uiLanguageLabel().c_str());

  if (state_ == AppState::Menu) {
    if (menu::isSettingsScreen(menuScreen_)) {
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    }
    renderMenu();
    return;
  }

  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  }
}

void App::cycleReaderMode(uint32_t nowMs) {
  readerMode_ = nextReaderMode(readerMode_);
  preferences_.putUChar(kPrefReaderMode, static_cast<uint8_t>(readerMode_));
  Serial.printf("[display] reader mode=%s\n", readerModeLabel().c_str());
  invalidateContextPreviewWindow();

  if (state_ == AppState::Menu) {
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  renderActiveReader(nowMs);
}

void App::cycleHandednessMode(uint32_t nowMs) {
  handednessMode_ = nextHandednessMode(handednessMode_);
  preferences_.putUChar(kPrefHandedness, static_cast<uint8_t>(handednessMode_));
  Serial.printf("[display] handedness=%s rotation180=%u\n", handednessLabel().c_str(),
                uiRotated180() ? 1U : 0U);
  applyHandednessSettings(nowMs);
}

void App::togglePhantomWords(uint32_t nowMs) {
  phantomWordsEnabled_ = !phantomWordsEnabled_;
  preferences_.putBool(kPrefPhantomWords, phantomWordsEnabled_);
  Serial.printf("[display] phantom words=%s\n", phantomWordsLabel().c_str());
  applyDisplayPreferences(nowMs);
}

void App::cycleReaderFontSize(uint32_t nowMs) {
  readerFontSizeIndex_ = static_cast<uint8_t>((readerFontSizeIndex_ + 1) % kReaderFontSizeCount);
  preferences_.putUChar(kPrefReaderFontSize, readerFontSizeIndex_);
  Serial.printf("[display] font size=%s\n", readerFontSizeLabel().c_str());
  applyDisplayPreferences(nowMs);
}

bool App::updateBatteryStatus(uint32_t nowMs, bool force) {
  if (!force) {
    const uint32_t sampleIntervalMs =
        batteryMonitor_.sampleIntervalMs(state_ == AppState::Playing);
    if (nowMs - lastBatterySampleMs_ < sampleIntervalMs) {
      return false;
    }
  }

  lastBatterySampleMs_ = nowMs;

  BoardConfig::BatteryStatus status;
  const bool present = BoardConfig::readBatteryStatus(status);
  batteryMonitor_.update(nowMs, present, status.voltage, status.percent, force);

  handleBatteryProtection(nowMs);
  if (powerOffStarted_) {
    return false;
  }

  const String nextLabel = currentBatteryLabel();
  if (nextLabel == batteryLabel_) {
    return false;
  }

  batteryLabel_ = nextLabel;
  display_.setBatteryLabel(batteryLabel_);
  if (!batteryLabel_.isEmpty()) {
    Serial.printf("[power] battery %.2f V raw=%u%% shown=%u%% label=%s\n", status.voltage,
                  static_cast<unsigned int>(status.percent),
                  static_cast<unsigned int>(batteryMonitor_.displayedPercent()),
                  batteryLabel_.c_str());
  } else {
    Serial.println("[power] battery not detected");
  }
  return true;
}

void App::handleBatteryProtection(uint32_t nowMs) {
  switch (batteryMonitor_.protectionAction()) {
    case battery::Action::Shutdown: {
      const String line2 =
          batteryVoltageLabel() + " " +
          String(static_cast<unsigned int>(batteryMonitor_.displayedPercent())) + "%";
      Serial.printf("[power] critical battery %.2f V %u%%; powering off\n",
                    static_cast<double>(batteryMonitor_.filteredVoltage()),
                    static_cast<unsigned int>(batteryMonitor_.displayedPercent()));
      display_.renderStatus("LOW BATTERY", "Powering off", line2);
      delay(kBatteryShutdownNoticeMs);
      enterPowerOff(millis());
      return;
    }
    case battery::Action::Warn:
      if (lastLowBatteryWarningMs_ == 0 ||
          nowMs - lastLowBatteryWarningMs_ >= kBatteryLowWarningRepeatMs) {
        showLowBatteryWarning(nowMs);
      }
      return;
    case battery::Action::None:
      return;
  }
}

void App::showLowBatteryWarning(uint32_t nowMs) {
  lastLowBatteryWarningMs_ = nowMs;
  batteryWarningOverlayVisible_ = true;
  batteryWarningRestoreAtMs_ = nowMs + kBatteryWarningVisibleMs;
  touchPlayHeld_ = false;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  wpmFeedbackVisible_ = false;
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;

  if (state_ == AppState::Playing) {
    setState(AppState::Paused, nowMs);
  }

  const String line1 =
      String(static_cast<unsigned int>(batteryMonitor_.displayedPercent())) + "% remaining";
  display_.renderStatus("LOW BATTERY", line1, batteryVoltageLabel() + " charge soon");
  Serial.printf("[power] low battery warning %.2f V %u%%\n",
                static_cast<double>(batteryMonitor_.filteredVoltage()),
                static_cast<unsigned int>(batteryMonitor_.displayedPercent()));
}

void App::updateBatteryWarningOverlay(uint32_t nowMs) {
  if (!batteryWarningOverlayVisible_ || nowMs < batteryWarningRestoreAtMs_) {
    return;
  }

  batteryWarningOverlayVisible_ = false;
  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  } else if (state_ == AppState::Menu) {
    renderMenu();
  } else if (state_ == AppState::Standby) {
    updateStandbyScreensaver(nowMs, true);
  }
}

void App::updateWpmFeedback(uint32_t nowMs) {
  if (!wpmFeedbackVisible_ || state_ != AppState::Paused) {
    return;
  }

  if (nowMs < wpmFeedbackUntilMs_) {
    return;
  }

  wpmFeedbackVisible_ = false;
  renderActiveReader(nowMs);
}

void App::resetReaderTapTracking() { lastReaderTapValid_ = false; }

bool App::isFooterMetricTap(uint16_t x, uint16_t y) const {
  return touchgesture::isFooterMetricTap(x, y, BoardConfig::DISPLAY_WIDTH,
                                         BoardConfig::DISPLAY_HEIGHT);
}

bool App::isBatteryBadgeTap(uint16_t x, uint16_t y) const {
  return touchgesture::isBatteryBadgeTap(x, y, BoardConfig::DISPLAY_WIDTH);
}

bool App::isPreviousSentenceTap(uint16_t x, uint16_t y) const {
  return touchgesture::isPreviousSentenceTap(x, y);
}

bool App::isStarSentenceTap(uint16_t x, uint16_t y) const {
  return touchgesture::isStarSentenceTap(x, y);
}

bool App::isActivelyReading() const { return state_ == AppState::Playing; }

DisplayManager::ReaderChrome App::readerChrome() const {
  DisplayManager::ReaderChrome chrome;
  const bool reading = isActivelyReading();
  chrome.showBattery = !reading || readerBatteryVisibleWhilePlaying_;
  chrome.showChapter = !reading || readerChapterVisibleWhilePlaying_;
  chrome.showProgress = !reading || readerProgressVisibleWhilePlaying_;
  chrome.showPreviousSentenceHint = !contextViewVisible_ || scrollModeEnabled();
  return chrome;
}

bool App::readerFooterVisible() const {
  const DisplayManager::ReaderChrome chrome = readerChrome();
  return chrome.showChapter || chrome.showProgress;
}

String App::readerFooterStatusLabel() const {
  if (isActivelyReading()) {
    return String(readingProgressPercent()) + "%";
  }

  return currentFooterMetricLabel();
}

String App::onOffLabel(bool enabled) const { return enabled ? uiText(UiText::On) : uiText(UiText::Off); }

bool App::handlePreviousSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  const bool previewBrowseMode = contextViewVisible_ && !scrollModeEnabled();
  if (previewBrowseMode || !isPreviousSentenceTap(x, y)) {
    return false;
  }

  rewindToPreviousSentence(nowMs);
  return true;
}

void App::rewindToPreviousSentence(uint32_t nowMs) {
  resetReaderTapTracking();
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  reader_.rewindSentence();

  if (state_ == AppState::Playing) {
    setState(AppState::Paused, nowMs);
  } else {
    renderActiveReader(nowMs);
    saveReadingPosition(true);
  }

  Serial.printf("[app] sentence rewind index=%u word=%s\n",
                static_cast<unsigned int>(reader_.currentIndex()), reader_.currentWord().c_str());
}

bool App::handleStarSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  // Only when paused on a real SD book: the demo text has no path to save under,
  // and the previous-sentence corner must keep priority where the zones touch.
  if (isActivelyReading() || isPreviousSentenceTap(x, y) || !isStarSentenceTap(x, y)) {
    return false;
  }
  if (!usingStorageBook_ || currentBookPath_.isEmpty()) {
    return false;
  }

  starCurrentSentence(nowMs);
  return true;
}

void App::starCurrentSentence(uint32_t nowMs) {
  resetReaderTapTracking();
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;

  const size_t wordCount = reader_.wordCount();
  const quotes::SentenceSpan span = quotes::extractSentence(
      reader_.currentIndex(), wordCount,
      [this](size_t i) { return reader_.wordAt(i); },
      [this](size_t i) { return reader_.wordEndsSentenceAt(i); });
  if (!span.found || span.text.isEmpty()) {
    showStarOverlay("Nothing to star", nowMs);
    return;
  }

  quotes::Quote quote;
  quote.bookPath = currentBookPath_;
  quote.bookTitle = currentBookTitle_;
  quote.wordIndex = static_cast<uint32_t>(span.startIndex);
  quote.sentence = span.text;

  bool writeFailed = false;
  const bool added = quoteStore_.add(quote, &writeFailed);
  if (added) {
    showStarOverlay("* Saved", nowMs);
  } else if (writeFailed) {
    showStarOverlay("Save failed", nowMs);
  } else {
    showStarOverlay("Already starred", nowMs);
  }
}

void App::showStarOverlay(const String &line, uint32_t nowMs) {
  starOverlayVisible_ = true;
  starOverlayUntilMs_ = nowMs + kStarOverlayMs;
  applyReaderUiOrientation();
  display_.renderStatus("Starred", line, currentBookTitle_);
}

void App::updateStarOverlay(uint32_t nowMs) {
  if (!starOverlayVisible_ || nowMs < starOverlayUntilMs_) {
    return;
  }
  starOverlayVisible_ = false;
  if (state_ == AppState::Paused || state_ == AppState::Playing) {
    renderActiveReader(nowMs);
  } else if (state_ == AppState::Menu) {
    renderMenu();
  }
}

bool App::handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (isActivelyReading() || !readerFooterVisible() || !isFooterMetricTap(x, y)) {
    return false;
  }

  switch (footerMetricMode_) {
    case FooterMetricMode::Percentage:
      footerMetricMode_ = FooterMetricMode::ChapterTime;
      break;
    case FooterMetricMode::ChapterTime:
      footerMetricMode_ = FooterMetricMode::BookTime;
      break;
    case FooterMetricMode::BookTime:
    default:
      footerMetricMode_ = FooterMetricMode::Percentage;
      break;
  }

  preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(footerMetricMode_));
  resetReaderTapTracking();
  renderActiveReader(nowMs);
  const char *modeName = "percent";
  switch (footerMetricMode_) {
    case FooterMetricMode::ChapterTime:
      modeName = "chapter";
      break;
    case FooterMetricMode::BookTime:
      modeName = "book";
      break;
    case FooterMetricMode::Percentage:
    default:
      modeName = "percent";
      break;
  }
  Serial.printf("[reader] footer metric=%s\n", modeName);
  return true;
}

bool App::handleBatteryBadgeTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (batteryLabel_.isEmpty() || !readerChrome().showBattery || !isBatteryBadgeTap(x, y)) {
    return false;
  }

  switch (batteryLabelMode_) {
    case BatteryLabelMode::Percent:
      batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
      break;
    case BatteryLabelMode::TimeRemaining:
      batteryLabelMode_ = BatteryLabelMode::Voltage;
      break;
    case BatteryLabelMode::Voltage:
    default:
      batteryLabelMode_ = BatteryLabelMode::Percent;
      break;
  }
  preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(batteryLabelMode_));
  batteryLabel_ = currentBatteryLabel();
  display_.setBatteryLabel(batteryLabel_);
  resetReaderTapTracking();
  renderActiveReader(nowMs);
  const char *modeName = "percent";
  if (batteryLabelMode_ == BatteryLabelMode::TimeRemaining) {
    modeName = "time";
  } else if (batteryLabelMode_ == BatteryLabelMode::Voltage) {
    modeName = "voltage";
  }
  Serial.printf("[power] battery label mode=%s label=%s\n", modeName, batteryLabel_.c_str());
  return true;
}

void App::handleReaderTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  const bool recentTap =
      lastReaderTapValid_ && nowMs - lastReaderTapMs_ <= kReaderDoubleTapWindowMs;
  const bool sameRegion =
      recentTap &&
      abs(static_cast<int>(x) - static_cast<int>(lastReaderTapX_)) <=
          static_cast<int>(kReaderDoubleTapSlopPx) &&
      abs(static_cast<int>(y) - static_cast<int>(lastReaderTapY_)) <=
          static_cast<int>(kReaderDoubleTapSlopPx);

  if (sameRegion) {
    resetReaderTapTracking();

    if (state_ == AppState::Playing) {
      requestReaderPauseAtSentenceEnd(nowMs);
    } else if (state_ == AppState::Paused) {
      playLocked_ = true;
      pauseAtSentenceEndRequested_ = false;
      wpmFeedbackVisible_ = false;
      setState(AppState::Playing, nowMs);
    }
    Serial.printf("[touch] reader double tap state=%s\n", stateName(state_));
    return;
  }

  if (recentTap) {
    Serial.printf("[touch] double tap miss dx=%d dy=%d dt=%lu\n",
                  static_cast<int>(x) - static_cast<int>(lastReaderTapX_),
                  static_cast<int>(y) - static_cast<int>(lastReaderTapY_),
                  static_cast<unsigned long>(nowMs - lastReaderTapMs_));
  }

  lastReaderTapValid_ = true;
  lastReaderTapMs_ = nowMs;
  lastReaderTapX_ = x;
  lastReaderTapY_ = y;
}

void App::requestReaderPauseAtSentenceEnd(uint32_t nowMs) {
  if (state_ != AppState::Playing) {
    return;
  }

  playLocked_ = false;
  touchPlayHeld_ = false;
  if (pauseMode_ == PauseMode::Instant) {
    pauseAtSentenceEndRequested_ = false;
    setState(AppState::Paused, nowMs);
    return;
  }

  if (!pauseAtSentenceEndRequested_) {
    pauseAtSentenceEndRequested_ = true;
    Serial.println("[app] pause requested at sentence end");
  }

  if (shouldFinalizeReaderPause(nowMs)) {
    finalizeReaderPause(nowMs);
  }
}

bool App::shouldFinalizeReaderPause(uint32_t nowMs) const {
  if (state_ != AppState::Playing || !pauseAtSentenceEndRequested_) {
    return false;
  }

  const uint32_t durationMs = reader_.currentWordDurationMs();
  if (durationMs == 0 || reader_.elapsedInCurrentWordMs(nowMs) < durationMs) {
    return false;
  }

  return reader_.currentWordEndsSentence() || reader_.atEnd();
}

void App::finalizeReaderPause(uint32_t nowMs) {
  pauseAtSentenceEndRequested_ = false;
  playLocked_ = false;
  touchPlayHeld_ = false;
  setState(AppState::Paused, nowMs);
}

void App::handleTouch(uint32_t nowMs) {
  if (!touchInitialized_) {
    return;
  }

  if (state_ == AppState::Booting || state_ == AppState::UsbTransfer ||
      state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    touch_.cancel();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    touchPlayHeld_ = false;
    resetReaderTapTracking();
    return;
  }

  TouchEvent ev;
  if (!touch_.poll(ev)) {
    return;
  }

  noteUserInput(nowMs);
  Serial.printf("[touch] phase=%s touched=%u x=%u y=%u gesture=%u state=%s\n",
                touchPhaseName(ev.phase), ev.touched ? 1 : 0, ev.x, ev.y, ev.gesture,
                stateName(state_));
  if (state_ == AppState::Menu) {
    if (menuScreen_ == MenuScreen::FocusTimerSession) {
      applyFocusTimerTouch(ev, nowMs);
    } else {
      applyMenuTouchGesture(ev, nowMs);
    }
  } else {
    applyPausedTouchGesture(ev, nowMs);
  }
}

void App::applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::End && touchPlayHeld_) {
    resetReaderTapTracking();
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    touchPlayHeld_ = false;
    requestReaderPauseAtSentenceEnd(nowMs);
    return;
  }

  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouchIntent_ = TouchIntent::None;
    if (state_ != AppState::Playing) {
      invalidateContextPreviewWindow();
    }
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    pausedTouch_.startWordIndex = reader_.currentIndex();
    pausedTouch_.gestureStepsApplied = 0;
    pausedTouch_.browseOffsetPermille = 0;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  const uint32_t elapsedSinceLastEventMs = nowMs - pausedTouch_.lastMs;
  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const uint32_t pressDurationMs = nowMs - pausedTouch_.startMs;
  const bool ended = event.phase == TouchPhase::End;
  const bool tapLike = touchgesture::isTap(absDeltaX, absDeltaY, gestureConfig_);
  const bool previewBrowseMode = contextViewVisible_ && !scrollModeEnabled();

  if (state_ == AppState::Playing) {
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      if (tapLike) {
        if (handleBatteryBadgeTap(event.x, event.y, nowMs)) {
          return;
        }
        if (handleFooterMetricTap(event.x, event.y, nowMs)) {
          return;
        }
        if (handlePreviousSentenceTap(event.x, event.y, nowMs)) {
          return;
        }
        if (playLocked_ || pauseAtSentenceEndRequested_) {
          resetReaderTapTracking();
          requestReaderPauseAtSentenceEnd(nowMs);
        } else {
          handleReaderTap(event.x, event.y, nowMs);
        }
      } else {
        resetReaderTapTracking();
      }
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::None &&
      touchgesture::shouldEngagePlayHold(pressDurationMs, tapLike, previewBrowseMode, ended,
                                         gestureConfig_)) {
    resetReaderTapTracking();
    touchPlayHeld_ = true;
    pausedTouchIntent_ = TouchIntent::PlayHold;
    wpmFeedbackVisible_ = false;
    setState(AppState::Playing, nowMs);
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::None) {
    switch (touchgesture::classifyReaderDrag(absDeltaX, absDeltaY, pressDurationMs,
                                             previewBrowseMode, ended, gestureConfig_)) {
      case touchgesture::ReaderIntent::Scrub:
        resetReaderTapTracking();
        pausedTouchIntent_ = TouchIntent::Scrub;
        break;
      case touchgesture::ReaderIntent::BrowseScroll:
        resetReaderTapTracking();
        pausedTouchIntent_ = TouchIntent::BrowseScroll;
        break;
      case touchgesture::ReaderIntent::Wpm:
        resetReaderTapTracking();
        pausedTouchIntent_ = TouchIntent::Wpm;
        break;
      case touchgesture::ReaderIntent::None:
        break;
    }
  }

  if (pausedTouchIntent_ == TouchIntent::Scrub) {
    applyScrubTarget(touchgesture::scrubStepsForDrag(deltaX, gestureConfig_), nowMs);
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      saveReadingPosition(true);
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::BrowseScroll) {
    applyBrowseHoldScroll(event.y, elapsedSinceLastEventMs, nowMs);
    if (ended) {
      pausedTouch_.active = false;
      pausedTouchIntent_ = TouchIntent::None;
      saveReadingPosition(true);
    }
    return;
  }

  if (pausedTouchIntent_ == TouchIntent::Wpm) {
    if (!ended) {
      return;
    }

    const int wpmDelta = touchgesture::wpmDeltaForDrag(deltaY);
    reader_.adjustWpm(wpmDelta);
    preferences_.putUShort(kPrefWpm, reader_.wpm());
    renderWpmFeedback(nowMs);
    Serial.printf("[app] WPM=%u interval=%lu ms\n", reader_.wpm(),
                  static_cast<unsigned long>(reader_.wordIntervalMs()));
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    return;
  }

  if (ended) {
    pausedTouch_.active = false;
    pausedTouchIntent_ = TouchIntent::None;
    if (tapLike && handleBatteryBadgeTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && handleFooterMetricTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && handlePreviousSentenceTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && !previewBrowseMode && handleStarSentenceTap(event.x, event.y, nowMs)) {
      return;
    }
    if (tapLike && previewBrowseMode) {
      resetReaderTapTracking();
      contextViewVisible_ = false;
      renderActiveReader(nowMs);
    } else if (tapLike) {
      handleReaderTap(event.x, event.y, nowMs);
    } else {
      resetReaderTapTracking();
    }
  }
}

void App::applyScrubTarget(int targetSteps, uint32_t nowMs) {
  if (targetSteps == pausedTouch_.gestureStepsApplied) {
    return;
  }

  reader_.seekRelative(pausedTouch_.startWordIndex, targetSteps);
  pausedTouch_.gestureStepsApplied = targetSteps;
  if (!scrollModeEnabled()) {
    contextViewVisible_ = true;
  }
  wpmFeedbackVisible_ = false;
  renderActiveReader(nowMs);
  Serial.printf("[app] scrub target=%d word=%s\n", targetSteps, reader_.currentWord().c_str());
}

void App::renderContextBrowsePreview(size_t currentIndex, uint16_t scrollProgressPermille) {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  if (currentIndex >= wordCount) {
    currentIndex = wordCount - 1;
    scrollProgressPermille = 0;
  }

  updateContextPreviewWindow(currentIndex);
  contextViewVisible_ = true;
  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), "",
                            readerFooterStatusLabel(), chrome);
}

void App::applyBrowseHoldScroll(uint16_t y, uint32_t elapsedMs, uint32_t nowMs) {
  if (elapsedMs == 0) {
    return;
  }

  const int ratePermille = touchgesture::browseScrollRatePermille(y, BoardConfig::DISPLAY_HEIGHT);
  pausedTouch_.browseOffsetPermille +=
      (static_cast<int32_t>(ratePermille) * static_cast<int32_t>(elapsedMs)) / 1000;

  int targetWords = pausedTouch_.browseOffsetPermille / 1000;
  int32_t remainderPermille = pausedTouch_.browseOffsetPermille % 1000;
  if (remainderPermille < 0) {
    remainderPermille += 1000;
    --targetWords;
  }

  reader_.seekRelative(pausedTouch_.startWordIndex, targetWords);
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }
  pausedTouch_.gestureStepsApplied = targetWords;
  contextViewVisible_ = true;
  wpmFeedbackVisible_ = false;
  renderContextBrowsePreview(reader_.currentIndex(),
                             static_cast<uint16_t>(remainderPermille));
  Serial.printf("[app] browse hold target=%d progress=%ld word=%s\n", targetWords,
                static_cast<long>(remainderPermille), reader_.currentWord().c_str());
}

void App::applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouchIntent_ = TouchIntent::None;
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  if (event.phase != TouchPhase::End) {
    return;
  }

  pausedTouch_.active = false;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);

  if (menuScreen_ == MenuScreen::TextEntry) {
    if (touchgesture::isTap(absDeltaX, absDeltaY, gestureConfig_)) {
      handleTextEntryTap(event.x, event.y, nowMs);
    }
    return;
  }

  if (menuScreen_ == MenuScreen::TypographyTuning &&
      touchgesture::isHorizontalSwipe(absDeltaX, absDeltaY, gestureConfig_)) {
    cycleTypographyPreviewSample(deltaX < 0 ? 1 : -1);
    return;
  }

  if (touchgesture::isVerticalSwipe(absDeltaX, absDeltaY, gestureConfig_)) {
    moveMenuSelection(deltaY < 0 ? -1 : 1);
    return;
  }

  if (touchgesture::isTap(absDeltaX, absDeltaY, gestureConfig_)) {
    selectMenuItem(nowMs);
  }
}

void App::applyFocusTimerTouch(const TouchEvent &event, uint32_t nowMs) {
  if (event.phase == TouchPhase::Start) {
    pausedTouch_.active = true;
    pausedTouch_.startX = event.x;
    pausedTouch_.startY = event.y;
    pausedTouch_.lastX = event.x;
    pausedTouch_.lastY = event.y;
    pausedTouch_.startMs = nowMs;
    pausedTouch_.lastMs = nowMs;
    focusTimerCancelHoldTriggered_ = false;
    return;
  }

  if (!pausedTouch_.active) {
    return;
  }

  pausedTouch_.lastX = event.x;
  pausedTouch_.lastY = event.y;
  pausedTouch_.lastMs = nowMs;

  const int deltaX = static_cast<int>(pausedTouch_.lastX) - static_cast<int>(pausedTouch_.startX);
  const int deltaY = static_cast<int>(pausedTouch_.lastY) - static_cast<int>(pausedTouch_.startY);
  const int absDeltaX = abs(deltaX);
  const int absDeltaY = abs(deltaY);
  const bool tapLike = touchgesture::isTap(absDeltaX, absDeltaY);

  if (focusTimer_.isActiveTimerRunning() && !focusTimerCancelHoldTriggered_ &&
      event.phase != TouchPhase::End &&
      absDeltaX <= static_cast<int>(kFocusTimerCancelHoldMaxDriftPx) &&
      absDeltaY <= static_cast<int>(kFocusTimerCancelHoldMaxDriftPx) &&
      nowMs - pausedTouch_.startMs >= kFocusTimerCancelHoldMs) {
    focusTimer_.cancelActiveTimer(nowMs);
    pausedTouch_.active = false;
    focusTimerCancelHoldTriggered_ = true;
    renderFocusTimerSession();
    return;
  }

  if (event.phase != TouchPhase::End) {
    return;
  }

  pausedTouch_.active = false;

  if (focusTimerCancelHoldTriggered_) {
    focusTimerCancelHoldTriggered_ = false;
    return;
  }

  (void)tapLike;
}

void App::openFocusTimer() {
  focusTimer_.open();
  rebuildFocusTimerGenreMenuItems();
  focusTimerGenreSelectedIndex_ =
      focusTimerGenreMenuItems_.size() > 1 ? kFocusTimerGenreFirstIndex : kFocusTimerGenreBackIndex;
  focusTimerCancelHoldTriggered_ = false;
  menuScreen_ = (focusTimer_.state() == FocusTimer::State::GenreSelect)
                    ? MenuScreen::FocusTimerGenres
                    : MenuScreen::FocusTimerSession;
  renderMenu();
}

void App::updateFocusTimer(uint32_t nowMs) {
  if (state_ != AppState::Menu || menuScreen_ != MenuScreen::FocusTimerSession) {
    return;
  }

  focusTimer_.update(nowMs);
  if (focusTimer_.consumeCompletionCue()) {
    playFocusTimerCompletionCue();
  }
  if (focusTimer_.state() == FocusTimer::State::GenreSelect) {
    menuScreen_ = MenuScreen::FocusTimerGenres;
    rebuildFocusTimerGenreMenuItems();
    renderFocusTimerGenres();
    return;
  }

  renderFocusTimerSession();
}

void App::resetFocusTimer() {
  focusTimer_.abandon();
  focusTimerCancelHoldTriggered_ = false;
  pausedTouch_.active = false;
  focusTimerGenreSelectedIndex_ = kFocusTimerGenreBackIndex;
}

void App::rebuildFocusTimerGenreMenuItems() {
  focusTimerGenreMenuItems_.clear();
  focusTimerGenreMenuItems_.push_back(uiText(UiText::Back));
  focusTimerGenreMenuItems_.push_back("Chores");
  focusTimerGenreMenuItems_.push_back("Work");
  focusTimerGenreMenuItems_.push_back("Fitness");
  focusTimerGenreMenuItems_.push_back("Self Care");
  focusTimerGenreMenuItems_.push_back("Other");

  if (focusTimerGenreSelectedIndex_ >= focusTimerGenreMenuItems_.size()) {
    focusTimerGenreSelectedIndex_ =
        focusTimerGenreMenuItems_.size() > 1 ? kFocusTimerGenreFirstIndex : kFocusTimerGenreBackIndex;
  }
}

void App::selectFocusTimerGenre(uint32_t nowMs) {
  if (focusTimerGenreMenuItems_.empty()) {
    rebuildFocusTimerGenreMenuItems();
  }

  if (focusTimerGenreSelectedIndex_ == kFocusTimerGenreBackIndex) {
    resetFocusTimer();
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  FocusTimer::Genre genre = FocusTimer::Genre::None;
  switch (focusTimerGenreSelectedIndex_) {
    case 1:
      genre = FocusTimer::Genre::Chores;
      break;
    case 2:
      genre = FocusTimer::Genre::RsvpNano;
      break;
    case 3:
      genre = FocusTimer::Genre::StrengthLabs;
      break;
    case 4:
      genre = FocusTimer::Genre::SelfCare;
      break;
    case 5:
      genre = FocusTimer::Genre::Other;
      break;
    default:
      break;
  }

  if (genre == FocusTimer::Genre::None) {
    return;
  }

  focusTimer_.chooseGenre(genre, nowMs);
  focusTimerCancelHoldTriggered_ = false;
  menuScreen_ = MenuScreen::FocusTimerSession;
  renderFocusTimerSession();
}

void App::moveMenuSelection(int direction) {
  if (direction == 0 || menuScreen_ == MenuScreen::TextEntry) {
    return;
  }

  size_t *selectedIndex = &menuSelectedIndex_;
  size_t itemCount = MenuItemCount;
  if (menu::isSettingsScreen(menuScreen_)) {
    selectedIndex = &settingsSelectedIndex_;
    itemCount = settingsMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    selectedIndex = &wifiNetworkSelectedIndex_;
    itemCount = wifiNetworkMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    selectedIndex = &typographyTuningSelectedIndex_;
    itemCount = TypographyTuningItemCount;
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    selectedIndex = &bookPickerSelectedIndex_;
    itemCount = bookMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    selectedIndex = &chapterPickerSelectedIndex_;
    itemCount = chapterMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::BookmarkPicker) {
    selectedIndex = &bookmarkPickerSelectedIndex_;
    itemCount = bookmarkMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::StarredPicker) {
    selectedIndex = &starredPickerSelectedIndex_;
    itemCount = starredMenuItems_.size();
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    selectedIndex = &restartConfirmSelectedIndex_;
    itemCount = RestartConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    selectedIndex = &sdCardRepairConfirmSelectedIndex_;
    itemCount = SdCardRepairConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    selectedIndex = &updateConfirmSelectedIndex_;
    itemCount = UpdateConfirmItemCount;
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectedIndex = &focusTimerGenreSelectedIndex_;
    itemCount = focusTimerGenreMenuItems_.size();
  }

  if (itemCount == 0) {
    return;
  }

  *selectedIndex = menu::wrapSelection(*selectedIndex, direction, itemCount);

  renderMenu();
  if (menu::isSettingsScreen(menuScreen_)) {
    Serial.printf("[settings] selected=%s\n", settingsMenuItems_[settingsSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    Serial.printf("[wifi] selected=%s\n", wifiNetworkMenuItems_[wifiNetworkSelectedIndex_].title.c_str());
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    Serial.printf("[typography] selected=%s\n", typographyTuningLabel().c_str());
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    Serial.printf("[book-picker] selected=%s\n",
                  bookMenuItems_[bookPickerSelectedIndex_].title.c_str());
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    Serial.printf("[chapter-picker] selected=%s\n",
                  chapterMenuItems_[chapterPickerSelectedIndex_].c_str());
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    String selectedLabel = uiText(UiText::AreYouSure);
    switch (restartConfirmSelectedIndex_) {
      case RestartConfirmNo:
        selectedLabel = uiText(UiText::NoKeepPlace);
        break;
      case RestartConfirmYes:
        selectedLabel = uiText(UiText::YesRestart);
        break;
      default:
        break;
    }
    Serial.printf("[restart] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    const String selectedLabel =
        sdCardRepairConfirmSelectedIndex_ == SdCardRepairConfirmYes ? "Create folders" : "Not now";
    Serial.printf("[sd-check] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    const String selectedLabel =
        updateConfirmSelectedIndex_ == UpdateConfirmUpdate ? "Update" : "Skip for now";
    Serial.printf("[ota] selected=%s\n", selectedLabel.c_str());
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    Serial.printf("[timer] selected genre=%s\n",
                  focusTimerGenreMenuItems_[focusTimerGenreSelectedIndex_].c_str());
  } else {
    String selectedLabel = uiText(UiText::Resume);
    switch (menuSelectedIndex_) {
      case MenuResume:
        selectedLabel = uiText(UiText::Resume);
        break;
      case MenuChapters:
        selectedLabel = uiText(UiText::Chapters);
        break;
      case MenuBooks:
        selectedLabel = "Books";
        break;
      case MenuArticles:
        selectedLabel = "Articles";
        break;
      case MenuFocusTimer:
        selectedLabel = "Focus Timer";
        break;
      case MenuSettings:
        selectedLabel = uiText(UiText::Settings);
        break;
      case MenuSdCardCheck:
        selectedLabel = "SD card check";
        break;
      case MenuRssFeeds:
        selectedLabel = "RSS feeds";
        break;
      case MenuCompanionSync:
        selectedLabel = "Companion sync";
        break;
#if RSVP_USB_TRANSFER_ENABLED
      case MenuUsbTransfer:
        selectedLabel = uiText(UiText::UsbTransfer);
        break;
#endif
      case MenuPowerOff:
        selectedLabel = uiText(UiText::PowerOff);
        break;
      default:
        break;
    }
    Serial.printf("[menu] selected=%s\n", selectedLabel.c_str());
  }
}

void App::selectMenuItem(uint32_t nowMs) {
  if (menu::isSettingsScreen(menuScreen_)) {
    selectSettingsItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::WifiNetworks) {
    selectWifiNetworkItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::TypographyTuning) {
    selectTypographyTuningItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::BookPicker) {
    selectBookPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::ChapterPicker) {
    selectChapterPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::BookmarkPicker) {
    selectBookmarkPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::StarredPicker) {
    selectStarredPickerItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::RestartConfirm) {
    selectRestartConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    selectSdCardRepairConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::UpdateConfirm) {
    selectUpdateConfirmItem(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    selectFocusTimerGenre(nowMs);
    return;
  }
  if (menuScreen_ == MenuScreen::FocusTimerSession) {
    return;
  }

  switch (menuSelectedIndex_) {
    case MenuResume:
      setState(AppState::Paused, nowMs);
      return;
    case MenuPowerOff:
      enterPowerOff(nowMs);
      return;
    case MenuCompanionSync:
      enterCompanionSync(nowMs);
      return;
    case MenuSdCardCheck:
      runSdCardCheck(nowMs);
      return;
    case MenuRssFeeds:
      runRssFeedCheck(nowMs);
      return;
#if RSVP_USB_TRANSFER_ENABLED
    case MenuUsbTransfer:
      enterUsbTransfer(nowMs);
      return;
#endif
    case MenuChapters:
      openChapterPicker();
      return;
    case MenuMarkFinished:
      toggleCurrentBookFinished(nowMs);
      return;
    case MenuBookmarks:
      openBookmarkPicker();
      return;
    case MenuStarred:
      openStarredPicker();
      return;
    case MenuStats:
      openStatsScreen();
      return;
    case MenuBooks:
      openBookPicker(false);
      return;
    case MenuArticles:
      openBookPicker(true);
      return;
    case MenuFocusTimer:
      openFocusTimer();
      return;
    case MenuSettings:
      openSettings();
      return;
    default:
      return;
  }
}

void App::openSettings() {
  settingsSelectedIndex_ = kSettingsHomeDisplayIndex;
  menuScreen_ = MenuScreen::SettingsHome;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectSettingsItem(uint32_t nowMs) {
  if (settingsMenuItems_.empty()) {
    if (menuScreen_ == MenuScreen::WifiSettings) {
      openWifiSettings();
    } else {
      openSettings();
    }
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsHome) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        menuScreen_ = MenuScreen::Main;
        renderMainMenu();
        return;
      case kSettingsHomeDisplayIndex:
        settingsSelectedIndex_ = kSettingsDisplayThemeIndex;
        menuScreen_ = MenuScreen::SettingsDisplay;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeTypographyIndex:
        openTypographyTuning();
        return;
      case kSettingsHomePacingIndex:
        settingsSelectedIndex_ = kSettingsPacingReadingModeIndex;
        menuScreen_ = MenuScreen::SettingsPacing;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsHomeWifiIndex:
        openWifiSettings();
        return;
      case kSettingsHomeUpdateIndex: {
        runFirmwareUpdate(preferredOtaConfig(), false, nowMs);
        return;
      }
      default:
        return;
    }
  }

  if (menuScreen_ == MenuScreen::WifiSettings) {
    selectWifiSettingsItem(nowMs);
    return;
  }

  if (menuScreen_ == MenuScreen::SettingsDisplay) {
    switch (settingsSelectedIndex_) {
      case kSettingsBackIndex:
        settingsSelectedIndex_ = kSettingsHomeDisplayIndex;
        menuScreen_ = MenuScreen::SettingsHome;
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayThemeIndex:
        cycleThemeMode(nowMs);
        return;
      case kSettingsDisplayBrightnessIndex:
        cycleBrightness();
        return;
      case kSettingsDisplayHandednessIndex:
        cycleHandednessMode(nowMs);
        return;
      case kSettingsDisplayGestureIndex:
        cycleGestureSensitivity();
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayFooterIndex:
        switch (footerMetricMode_) {
          case FooterMetricMode::Percentage:
            footerMetricMode_ = FooterMetricMode::ChapterTime;
            break;
          case FooterMetricMode::ChapterTime:
            footerMetricMode_ = FooterMetricMode::BookTime;
            break;
          case FooterMetricMode::BookTime:
            footerMetricMode_ = FooterMetricMode::Percentage;
            break;
        }
        preferences_.putUChar(kPrefFooterMetricMode, static_cast<uint8_t>(footerMetricMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayBatteryIndex:
        switch (batteryLabelMode_) {
          case BatteryLabelMode::Percent:
            batteryLabelMode_ = BatteryLabelMode::TimeRemaining;
            break;
          case BatteryLabelMode::TimeRemaining:
            batteryLabelMode_ = BatteryLabelMode::Voltage;
            break;
          case BatteryLabelMode::Voltage:
          default:
            batteryLabelMode_ = BatteryLabelMode::Percent;
            break;
        }
        preferences_.putUChar(kPrefBatteryLabelMode, static_cast<uint8_t>(batteryLabelMode_));
        batteryLabel_ = currentBatteryLabel();
        display_.setBatteryLabel(batteryLabel_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayScreensaverIndex:
        switch (screensaverMode_) {
          case ScreensaverMode::Life:
            screensaverMode_ = ScreensaverMode::Maze;
            break;
          case ScreensaverMode::Maze:
            screensaverMode_ = ScreensaverMode::Voronoi;
            break;
          case ScreensaverMode::Voronoi:
            screensaverMode_ = ScreensaverMode::ScreenOff;
            break;
          case ScreensaverMode::ScreenOff:
          default:
            screensaverMode_ = ScreensaverMode::Life;
            break;
        }
        preferences_.putUChar(kPrefScreensaverMode, static_cast<uint8_t>(screensaverMode_));
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayIdleStandbyIndex:
        cycleIdleStandbyTimeout();
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayMuteIndex:
        toggleAudioMute();
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayVolumeIndex:
        cycleAudioVolume();
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderBatteryIndex:
        readerBatteryVisibleWhilePlaying_ = !readerBatteryVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderBatteryVisible, readerBatteryVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderChapterIndex:
        readerChapterVisibleWhilePlaying_ = !readerChapterVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderChapterVisible, readerChapterVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayReaderProgressIndex:
        readerProgressVisibleWhilePlaying_ = !readerProgressVisibleWhilePlaying_;
        preferences_.putBool(kPrefReaderProgressVisible, readerProgressVisibleWhilePlaying_);
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      case kSettingsDisplayLanguageIndex:
        cycleUiLanguage(nowMs);
        return;
      case kSettingsDisplayMotionPauseIndex:
        if (imuShortcutSensorReady_) {
          imuShortcutsEnabled_ = !imuShortcutsEnabled_;
          preferences_.putBool(kPrefImuShortcuts, imuShortcutsEnabled_);
        }
        rebuildSettingsMenuItems();
        renderSettings();
        return;
      default:
        return;
    }
  }

  if (menuScreen_ != MenuScreen::SettingsPacing) {
    return;
  }

  bool pacingConfigChanged = false;
  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      flushPendingTimeEstimateRebuild();
      settingsSelectedIndex_ = kSettingsHomePacingIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingReadingModeIndex:
      cycleReaderMode(nowMs);
      return;
    case kSettingsPacingPauseModeIndex:
      pauseMode_ =
          pauseMode_ == PauseMode::SentenceEnd ? PauseMode::Instant : PauseMode::SentenceEnd;
      preferences_.putUChar(kPrefPauseMode, static_cast<uint8_t>(pauseMode_));
      Serial.printf("[settings] pause mode=%s\n", pauseModeLabel().c_str());
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kSettingsPacingWpmIndex:
      reader_.setWpm(nextReaderWpmSetting(reader_.wpm()));
      preferences_.putUShort(kPrefWpm, reader_.wpm());
      Serial.printf("[settings] WPM=%u interval=%lu ms\n", reader_.wpm(),
                    static_cast<unsigned long>(reader_.wordIntervalMs()));
      break;
    case kSettingsPacingLongWordsIndex:
      pacingLongWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingLongWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingComplexityIndex:
      pacingComplexWordDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingComplexWordDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingPunctuationIndex:
      pacingPunctuationDelayMs_ = static_cast<uint16_t>(nextCyclicSetting(
          pacingPunctuationDelayMs_, kPacingDelayMinMs, kPacingDelayMaxMs, kPacingDelayStepMs));
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      pacingConfigChanged = true;
      break;
    case kSettingsPacingResetIndex:
      pacingLongWordDelayMs_ = kDefaultPacingDelayMs;
      pacingComplexWordDelayMs_ = kDefaultPacingDelayMs;
      pacingPunctuationDelayMs_ = kDefaultPacingDelayMs;
      preferences_.putUShort(kPrefPacingLongMs, pacingLongWordDelayMs_);
      preferences_.putUShort(kPrefPacingComplexMs, pacingComplexWordDelayMs_);
      preferences_.putUShort(kPrefPacingPunctuationMs, pacingPunctuationDelayMs_);
      pacingConfigChanged = true;
      break;
    default:
      return;
  }

  if (pacingConfigChanged) {
    applyPacingSettings();
  }
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::openWifiSettings() {
  settingsSelectedIndex_ = configuredWifiSsid().isEmpty() ? kWifiSettingsChooseIndex
                                                          : kWifiSettingsAutoUpdateIndex;
  menuScreen_ = MenuScreen::WifiSettings;
  rebuildSettingsMenuItems();
  renderSettings();
}

void App::selectWifiSettingsItem(uint32_t nowMs) {
  switch (settingsSelectedIndex_) {
    case kSettingsBackIndex:
      settingsSelectedIndex_ = kSettingsHomeWifiIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsNetworkIndex:
    case kWifiSettingsChooseIndex:
      scanWifiNetworks();
      return;
    case kWifiSettingsAutoUpdateIndex:
      preferences_.putBool(kPrefOtaAuto, !otaAutoCheckEnabled());
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsForgetIndex:
      preferences_.remove(kPrefWifiSsid);
      preferences_.remove(kPrefWifiPass);
      display_.renderStatus("Wi-Fi", "Credentials cleared", "");
      delay(900);
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsOtaOwnerIndex:
      openTextEntry(TextEntryPurpose::OtaOwner, "OTA Source", "GitHub owner", "",
                    preferences_.getString(kPrefOtaOwner, ""), "", false, 39,
                    MenuScreen::WifiSettings);
      return;
    case kWifiSettingsFirmwareVersionIndex:
    case kWifiSettingsLastResultIndex:
      // Display-only rows; re-render so the selection highlight settles.
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case kWifiSettingsCheckNowIndex:
      runFirmwareCheckOnly(nowMs);
      return;
    default:
      return;
  }
}

void App::scanWifiNetworks() {
  if (blockNetworkActionForOtaCheck("Wi-Fi", millis())) {
    return;
  }

  display_.renderProgress("Wi-Fi", "Scanning networks", "", 5);

  WiFi.persistent(false);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();

  const int networkCount = WiFi.scanNetworks(false, true);
  wifiNetworks_.clear();
  wifiNetworkMenuItems_.clear();
  wifiNetworkMenuItems_.push_back({uiText(UiText::Back), ""});

  if (networkCount > 0) {
    for (int i = 0; i < networkCount; ++i) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) {
        continue;
      }

      WifiNetworkInfo network;
      network.ssid = ssid;
      network.rssi = WiFi.RSSI(i);
      network.authMode = static_cast<uint8_t>(WiFi.encryptionType(i));
      wifiNetworks_.push_back(network);
    }
  }

  WiFi.scanDelete();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);

  if (wifiNetworks_.empty()) {
    display_.renderStatus("Wi-Fi", "No networks found", "");
    delay(1200);
    openWifiSettings();
    return;
  }

  const String savedSsid = configuredWifiSsid();
  std::stable_sort(wifiNetworks_.begin(), wifiNetworks_.end(),
                   [&savedSsid](const WifiNetworkInfo &left, const WifiNetworkInfo &right) {
                     const bool leftSaved = !savedSsid.isEmpty() && left.ssid == savedSsid;
                     const bool rightSaved = !savedSsid.isEmpty() && right.ssid == savedSsid;
                     if (leftSaved != rightSaved) {
                       return leftSaved;
                     }
                     if (left.rssi != right.rssi) {
                       return left.rssi > right.rssi;
                     }
                     return left.ssid < right.ssid;
                   });

  wifiNetworkMenuItems_.reserve(wifiNetworks_.size() + 1);
  for (const WifiNetworkInfo &network : wifiNetworks_) {
    wifiNetworkMenuItems_.push_back(
        {network.ssid, wifiSecurityLabel(network.authMode) + "  " + String(network.rssi) + " dBm"});
  }

  wifiNetworkSelectedIndex_ =
      wifiNetworkMenuItems_.size() > 1 ? kWifiNetworksFirstItemIndex : kWifiNetworksBackIndex;
  menuScreen_ = MenuScreen::WifiNetworks;
  renderWifiNetworks();
}

void App::renderWifiNetworks() {
  if (wifiNetworkMenuItems_.empty()) {
    display_.renderStatus("Wi-Fi", "No networks found", "");
    return;
  }

  display_.renderLibrary(wifiNetworkMenuItems_, wifiNetworkSelectedIndex_);
}

void App::selectWifiNetworkItem(uint32_t nowMs) {
  (void)nowMs;

  if (wifiNetworkSelectedIndex_ == kWifiNetworksBackIndex || wifiNetworkMenuItems_.size() <= 1) {
    openWifiSettings();
    return;
  }

  const size_t networkIndex = wifiNetworkSelectedIndex_ - kWifiNetworksFirstItemIndex;
  if (networkIndex >= wifiNetworks_.size()) {
    openWifiSettings();
    return;
  }

  const WifiNetworkInfo &network = wifiNetworks_[networkIndex];
  if (wifiNetworkRequiresPassword(network.authMode)) {
    String initialValue;
    if (configuredWifiSsid() == network.ssid) {
      initialValue = preferredOtaConfig().wifiPassword;
    }
    openTextEntry(TextEntryPurpose::WifiPassword, network.ssid, "Password", "",
                  initialValue, network.ssid, true, kWifiPasswordMaxLength,
                  MenuScreen::WifiNetworks);
    return;
  }

  preferences_.putString(kPrefWifiSsid, network.ssid);
  preferences_.putString(kPrefWifiPass, "");
  display_.renderStatus("Wi-Fi", "Network saved", network.ssid);
  delay(900);
  openWifiSettings();
}

void App::openTextEntry(TextEntryPurpose purpose, const String &title, const String &prompt,
                        const String &helperText, const String &initialValue,
                        const String &contextValue, bool masked, size_t maxLength,
                        MenuScreen returnScreen) {
  textEntrySession_ = TextEntrySession();
  textEntrySession_.active = true;
  textEntrySession_.purpose = purpose;
  textEntrySession_.mode = KeyboardMode::Lower;
  textEntrySession_.returnScreen = returnScreen;
  textEntrySession_.title = title;
  textEntrySession_.prompt = prompt;
  textEntrySession_.helperText = helperText;
  textEntrySession_.value = initialValue;
  textEntrySession_.contextValue = contextValue;
  textEntrySession_.maxLength = maxLength;
  textEntrySession_.masked = masked;
  textEntrySession_.revealValue = false;
  menuScreen_ = MenuScreen::TextEntry;
  rebuildTextEntryButtons();
  renderTextEntry();
}

void App::rebuildTextEntryButtons() {
  textEntryButtons_.clear();
  if (!textEntrySession_.active) {
    return;
  }

  const uint16_t rowPitch = kKeyboardRowHeight + kKeyboardRowGap;
  for (size_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
    const String rowChars = keyboardRowText(static_cast<uint8_t>(textEntrySession_.mode), rowIndex);
    const size_t keyCount = rowChars.length();
    if (keyCount == 0) {
      continue;
    }

    const int availableWidth =
        BoardConfig::DISPLAY_WIDTH - (2 * kKeyboardMarginX) -
        static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    const int keyWidth = std::max(28, availableWidth / static_cast<int>(keyCount));
    const int totalWidth =
        keyWidth * static_cast<int>(keyCount) + static_cast<int>((keyCount - 1) * kKeyboardRowGap);
    int x = std::max(0, (BoardConfig::DISPLAY_WIDTH - totalWidth) / 2);
    const int y = kKeyboardTopY + static_cast<int>(rowIndex * rowPitch);

    for (size_t charIndex = 0; charIndex < keyCount; ++charIndex) {
      TextEntryButton button;
      button.view.label = String(rowChars[charIndex]);
      button.view.x = static_cast<uint16_t>(x);
      button.view.y = static_cast<uint16_t>(y);
      button.view.width = static_cast<uint16_t>(keyWidth);
      button.view.height = kKeyboardRowHeight;
      button.action = TextEntryAction::Insert;
      button.payload = String(rowChars[charIndex]);
      textEntryButtons_.push_back(button);
      x += keyWidth + kKeyboardRowGap;
    }
  }

  struct ControlButtonDef {
    String label;
    TextEntryAction action;
    uint16_t units;
    bool accent;
    bool active;
  };

  const bool revealActive = textEntrySession_.masked && textEntrySession_.revealValue;
  const ControlButtonDef controls[] = {
      {"abc", TextEntryAction::SetLower, 11, false,
       textEntrySession_.mode == KeyboardMode::Lower},
      {"ABC", TextEntryAction::SetUpper, 11, false,
       textEntrySession_.mode == KeyboardMode::Upper},
      {"123", TextEntryAction::SetSymbols, 11, false,
       textEntrySession_.mode == KeyboardMode::Symbols},
      {"space", TextEntryAction::Space, 24, false, false},
      {"back", TextEntryAction::Backspace, 13, false, false},
      {textEntrySession_.masked ? (revealActive ? "hide" : "show") : "clear",
       textEntrySession_.masked ? TextEntryAction::ToggleMask : TextEntryAction::Clear, 13, false,
       revealActive},
      {"save", TextEntryAction::Save, 12, true, false},
      {"cancel", TextEntryAction::Cancel, 14, false, false},
  };

  uint16_t totalUnits = 0;
  for (const ControlButtonDef &control : controls) {
    totalUnits += control.units;
  }

  const size_t controlCount = sizeof(controls) / sizeof(controls[0]);
  const int totalGapWidth = static_cast<int>((controlCount - 1) * kKeyboardRowGap);
  const int availableWidth = BoardConfig::DISPLAY_WIDTH - (2 * kKeyboardMarginX) - totalGapWidth;
  int remainingWidth = availableWidth;
  uint16_t x = kKeyboardMarginX;
  const uint16_t y = kKeyboardTopY + static_cast<uint16_t>(3 * rowPitch);

  for (size_t i = 0; i < controlCount; ++i) {
    const ControlButtonDef &control = controls[i];
    int width = remainingWidth;
    if (i + 1 < controlCount) {
      width = (availableWidth * control.units) / totalUnits;
      remainingWidth -= width;
    }

    TextEntryButton button;
    button.view.label = control.label;
    button.view.x = x;
    button.view.y = y;
    button.view.width = static_cast<uint16_t>(std::max(28, width));
    button.view.height = kKeyboardRowHeight;
    button.view.accent = control.accent;
    button.view.active = control.active;
    button.action = control.action;
    textEntryButtons_.push_back(button);

    x = static_cast<uint16_t>(x + button.view.width + kKeyboardRowGap);
  }
}

void App::renderTextEntry() {
  if (!textEntrySession_.active) {
    return;
  }

  const String visibleValue =
      (textEntrySession_.masked && !textEntrySession_.revealValue)
          ? maskedValue(textEntrySession_.value)
          : textEntrySession_.value;

  std::vector<DisplayManager::Button> buttons;
  buttons.reserve(textEntryButtons_.size());
  for (const TextEntryButton &button : textEntryButtons_) {
    buttons.push_back(button.view);
  }

  display_.renderTextEntry(textEntrySession_.title, textEntrySession_.prompt, visibleValue,
                           textEntrySession_.helperText, buttons);
}

bool App::handleTextEntryTap(uint16_t x, uint16_t y, uint32_t nowMs) {
  if (!textEntrySession_.active) {
    return false;
  }

  for (size_t i = 0; i < textEntryButtons_.size(); ++i) {
    const DisplayManager::Button &button = textEntryButtons_[i].view;
    const uint16_t maxX = button.x + button.width;
    const uint16_t maxY = button.y + button.height;
    if (x < button.x || x > maxX || y < button.y || y > maxY) {
      continue;
    }

    activateTextEntryButton(i, nowMs);
    return true;
  }

  return false;
}

void App::activateTextEntryButton(size_t buttonIndex, uint32_t nowMs) {
  if (buttonIndex >= textEntryButtons_.size()) {
    return;
  }

  TextEntryButton &button = textEntryButtons_[buttonIndex];
  switch (button.action) {
    case TextEntryAction::Insert:
      if (textEntrySession_.value.length() < textEntrySession_.maxLength) {
        textEntrySession_.value += button.payload;
      }
      break;
    case TextEntryAction::SetLower:
      textEntrySession_.mode = KeyboardMode::Lower;
      break;
    case TextEntryAction::SetUpper:
      textEntrySession_.mode = KeyboardMode::Upper;
      break;
    case TextEntryAction::SetSymbols:
      textEntrySession_.mode = KeyboardMode::Symbols;
      break;
    case TextEntryAction::Space:
      if (textEntrySession_.value.length() < textEntrySession_.maxLength) {
        textEntrySession_.value += ' ';
      }
      break;
    case TextEntryAction::Backspace:
      if (!textEntrySession_.value.isEmpty()) {
        textEntrySession_.value.remove(textEntrySession_.value.length() - 1);
      }
      break;
    case TextEntryAction::Clear:
      textEntrySession_.value = "";
      break;
    case TextEntryAction::ToggleMask:
      if (textEntrySession_.masked) {
        textEntrySession_.revealValue = !textEntrySession_.revealValue;
      }
      break;
    case TextEntryAction::Save:
      commitTextEntry(nowMs);
      return;
    case TextEntryAction::Cancel:
      menuScreen_ = textEntrySession_.returnScreen;
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      renderMenu();
      return;
  }

  rebuildTextEntryButtons();
  renderTextEntry();
}

void App::commitTextEntry(uint32_t nowMs) {
  (void)nowMs;

  switch (textEntrySession_.purpose) {
    case TextEntryPurpose::WifiPassword: {
      if (textEntrySession_.value.isEmpty()) {
        display_.renderStatus("Wi-Fi", "Password required", textEntrySession_.contextValue);
        delay(1000);
        renderTextEntry();
        return;
      }

      const String ssid = textEntrySession_.contextValue;
      preferences_.putString(kPrefWifiSsid, ssid);
      preferences_.putString(kPrefWifiPass, textEntrySession_.value);
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      display_.renderStatus("Wi-Fi", "Network saved", ssid);
      delay(900);
      openWifiSettings();
      return;
    }
    case TextEntryPurpose::OtaOwner: {
      const String owner = textEntrySession_.value;
      if (owner.isEmpty()) {
        preferences_.remove(kPrefOtaOwner);
        display_.renderStatus("OTA", "Reset to default", "");
      } else {
        preferences_.putString(kPrefOtaOwner, owner);
        display_.renderStatus("OTA", "Owner saved", owner);
      }
      delay(900);
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      openWifiSettings();
      return;
    }
    case TextEntryPurpose::None:
    default:
      menuScreen_ = textEntrySession_.returnScreen;
      textEntrySession_ = TextEntrySession();
      textEntryButtons_.clear();
      renderMenu();
      return;
  }
}

void App::openTypographyTuning() {
  if (typographyTuningSelectedIndex_ >= TypographyTuningItemCount) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }
  if (typographyTuningSelectedIndex_ == TypographyTuningBack) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }
  menuScreen_ = MenuScreen::TypographyTuning;
  renderTypographyTuning();
}

void App::selectTypographyTuningItem(uint32_t nowMs) {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      settingsSelectedIndex_ = kSettingsHomeTypographyIndex;
      menuScreen_ = MenuScreen::SettingsHome;
      rebuildSettingsMenuItems();
      renderSettings();
      return;
    case TypographyTuningFontSize:
      cycleReaderFontSize(nowMs);
      return;
    case TypographyTuningTypeface:
      typographyConfig_.typeface = nextReaderTypeface(typographyConfig_.typeface);
      preferences_.putUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface));
      break;
    case TypographyTuningPhantomWords:
      togglePhantomWords(nowMs);
      return;
    case TypographyTuningFocusHighlight:
      typographyConfig_.focusHighlight = !typographyConfig_.focusHighlight;
      preferences_.putBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
      break;
    case TypographyTuningTracking:
      typographyConfig_.trackingPx = static_cast<int8_t>(
          nextCyclicSetting(typographyConfig_.trackingPx, kTypographyTrackingMin,
                            kTypographyTrackingMax));
      preferences_.putChar(kPrefTypographyTracking, typographyConfig_.trackingPx);
      break;
    case TypographyTuningAnchor: {
      const uint8_t anchorMin =
          (handednessMode_ == HandednessMode::Left) ? kLeftHandAnchorMin : kTypographyAnchorMin;
      const uint8_t anchorMax =
          (handednessMode_ == HandednessMode::Left) ? kLeftHandAnchorMax : kTypographyAnchorMax;
      const uint8_t nextAnchorPercent = static_cast<uint8_t>(
          nextCyclicSetting(effectiveAnchorPercent(), anchorMin, anchorMax));
      typographyConfig_.anchorPercent = (handednessMode_ == HandednessMode::Left)
                                            ? static_cast<uint8_t>(nextAnchorPercent -
                                                                   kLeftHandAnchorOffset)
                                            : nextAnchorPercent;
      preferences_.putUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent);
      break;
    }
    case TypographyTuningGuideWidth:
      typographyConfig_.guideHalfWidth = static_cast<uint8_t>(nextCyclicSetting(
          typographyConfig_.guideHalfWidth, kTypographyGuideWidthMin,
          kTypographyGuideWidthMax, kTypographyGuideWidthStep));
      preferences_.putUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth);
      break;
    case TypographyTuningGuideGap:
      typographyConfig_.guideGap = static_cast<uint8_t>(nextCyclicSetting(
          typographyConfig_.guideGap, kTypographyGuideGapMin, kTypographyGuideGapMax));
      preferences_.putUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap);
      break;
    case TypographyTuningReset:
      typographyConfig_ = defaultTypographyConfig();
      preferences_.putUChar(kPrefReaderTypeface, static_cast<uint8_t>(typographyConfig_.typeface));
      preferences_.putBool(kPrefTypographyFocusHighlight, typographyConfig_.focusHighlight);
      preferences_.putChar(kPrefTypographyTracking, typographyConfig_.trackingPx);
      preferences_.putUChar(kPrefTypographyAnchor, typographyConfig_.anchorPercent);
      preferences_.putUChar(kPrefTypographyGuideWidth, typographyConfig_.guideHalfWidth);
      preferences_.putUChar(kPrefTypographyGuideGap, typographyConfig_.guideGap);
      break;
    default:
      return;
  }

  applyTypographySettings(nowMs);
}

void App::cycleTypographyPreviewSample(int direction) {
  if (kTypographyPreviewWordCount == 0 || direction == 0) {
    return;
  }

  const int current = static_cast<int>(typographyPreviewSampleIndex_);
  int next = current + direction;
  if (next < 0) {
    next = static_cast<int>(kTypographyPreviewWordCount) - 1;
  } else if (next >= static_cast<int>(kTypographyPreviewWordCount)) {
    next = 0;
  }
  typographyPreviewSampleIndex_ = static_cast<size_t>(next);
  renderTypographyTuning();
}

void App::rebuildSettingsMenuItems() {
  settingsMenuItems_.clear();
  settingsMenuItems_.reserve(SettingsItemCount);
  if (menuScreen_ == MenuScreen::SettingsHome) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back(uiText(UiText::WordPacing));
    settingsMenuItems_.push_back(uiText(UiText::Display));
    settingsMenuItems_.push_back(uiText(UiText::TypographyTune));
    settingsMenuItems_.push_back("Wi-Fi");
    settingsMenuItems_.push_back(firmwareUpdateMenuLabel());
  } else if (menuScreen_ == MenuScreen::SettingsDisplay) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Display mode: " + themeModeLabel());
    settingsMenuItems_.push_back(uiText(UiText::Brightness) + ": " +
                                 String(currentBrightnessPercent()) + "%");
    settingsMenuItems_.push_back("Reader hand: " + handednessLabel());
    settingsMenuItems_.push_back("Gesture sensitivity: " + gestureSensitivityLabel());
    settingsMenuItems_.push_back("Footer label: " + footerMetricModeLabel());
    settingsMenuItems_.push_back("Battery label: " + batteryLabelModeLabel());
    settingsMenuItems_.push_back("Screensaver: " + screensaverModeLabel());
    settingsMenuItems_.push_back("Idle standby: " + idleStandbyLabel());
    settingsMenuItems_.push_back("Mute audio: " + audioMuteLabel());
    settingsMenuItems_.push_back("Volume: " + audioVolumeLabel());
    settingsMenuItems_.push_back("Reading battery: " +
                                 onOffLabel(readerBatteryVisibleWhilePlaying_));
    settingsMenuItems_.push_back("Reading chapter: " +
                                 onOffLabel(readerChapterVisibleWhilePlaying_));
    settingsMenuItems_.push_back("Reading percent: " +
                                 onOffLabel(readerProgressVisibleWhilePlaying_));
    settingsMenuItems_.push_back(uiText(UiText::Language) + ": " + uiLanguageLabel());
    settingsMenuItems_.push_back(
        String("Motion gestures: ") +
        (imuShortcutSensorReady_ ? onOffLabel(imuShortcutsEnabled_) : String("No sensor")));
  } else if (menuScreen_ == MenuScreen::SettingsPacing) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Reading mode: " + readerModeLabel());
    settingsMenuItems_.push_back("Pause behaviour: " + pauseModeLabel());
    settingsMenuItems_.push_back("Base speed: " + String(reader_.wpm()) + " WPM");
    settingsMenuItems_.push_back(uiText(UiText::LongWords) + ": " +
                                 pacingDelayLabel(pacingLongWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Complexity) + ": " +
                                 pacingDelayLabel(pacingComplexWordDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::Punctuation) + ": " +
                                 pacingDelayLabel(pacingPunctuationDelayMs_));
    settingsMenuItems_.push_back(uiText(UiText::ResetPacing));
  } else if (menuScreen_ == MenuScreen::WifiSettings) {
    settingsMenuItems_.push_back(uiText(UiText::Back));
    settingsMenuItems_.push_back("Network: " + storedOrFallbackLabel(configuredWifiSsid(), "Not set"));
    settingsMenuItems_.push_back("Choose network");
    settingsMenuItems_.push_back("Auto OTA: " + String(otaAutoCheckEnabled() ? "On" : "Off"));
    settingsMenuItems_.push_back("Forget network");
    settingsMenuItems_.push_back("OTA Owner: " + otaOwnerLabel());
    settingsMenuItems_.push_back("Firmware: " + otaUpdater_.currentVersion());
    settingsMenuItems_.push_back("Check for updates");
    settingsMenuItems_.push_back("Last check: " + otaLastResultLabel());
  }

  if (settingsSelectedIndex_ >= settingsMenuItems_.size()) {
    settingsSelectedIndex_ = kSettingsBackIndex;
  }
}

void App::applyPacingSettings() {
  ReadingLoop::PacingConfig pacingConfig;
  pacingConfig.longWordDelayMs = pacingLongWordDelayMs_;
  pacingConfig.complexWordDelayMs = pacingComplexWordDelayMs_;
  pacingConfig.punctuationDelayMs = pacingPunctuationDelayMs_;
  reader_.setPacingConfig(pacingConfig);

  Serial.printf("[settings] pacing long=%u ms complexity=%u ms punctuation=%u ms\n",
                static_cast<unsigned int>(pacingLongWordDelayMs_),
                static_cast<unsigned int>(pacingComplexWordDelayMs_),
                static_cast<unsigned int>(pacingPunctuationDelayMs_));
  if (state_ == AppState::Menu && menuScreen_ == MenuScreen::SettingsPacing) {
    pacingCacheDirty_ = true;
  } else {
    rebuildTimeEstimateCache();
  }
}

void App::flushPendingTimeEstimateRebuild() {
  if (!pacingCacheDirty_) {
    return;
  }
  rebuildTimeEstimateCache();
}

String App::otaOwnerLabel() {
  if (preferences_.isKey(kPrefOtaOwner)) {
    return preferences_.getString(kPrefOtaOwner, "");
  }
  OtaUpdater::Config cfg;
  otaUpdater_.loadConfig(cfg);
  return cfg.githubOwner;
}

OtaUpdater::Config App::preferredOtaConfig() {
  OtaUpdater::Config otaConfig;
  otaUpdater_.loadConfig(otaConfig);

  if (preferences_.isKey(kPrefWifiSsid)) {
    otaConfig.wifiSsid = preferences_.getString(kPrefWifiSsid, "");
  }
  if (preferences_.isKey(kPrefWifiPass)) {
    otaConfig.wifiPassword = preferences_.getString(kPrefWifiPass, "");
  }
  if (preferences_.isKey(kPrefOtaAuto)) {
    otaConfig.autoCheck = preferences_.getBool(kPrefOtaAuto, otaConfig.autoCheck);
  }
  if (preferences_.isKey(kPrefOtaOwner)) {
    otaConfig.githubOwner = preferences_.getString(kPrefOtaOwner, "");
  }

  return otaConfig;
}

String App::configuredWifiSsid() {
  String ssid = preferences_.getString(kPrefWifiSsid, "");
  if (ssid.isEmpty()) {
    OtaUpdater::Config otaConfig;
    otaUpdater_.loadConfig(otaConfig);
    ssid = otaConfig.wifiSsid;
  }
  ssid.trim();
  return ssid;
}

bool App::otaAutoCheckEnabled() {
  if (preferences_.isKey(kPrefOtaAuto)) {
    return preferences_.getBool(kPrefOtaAuto, false);
  }

  OtaUpdater::Config otaConfig;
  otaUpdater_.loadConfig(otaConfig);
  return otaConfig.autoCheck;
}

void App::maybeAutoCheckForUpdates(uint32_t nowMs) {
  (void)nowMs;
  OtaUpdater::Config otaConfig = preferredOtaConfig();
  if (!otaConfig.autoCheck || !otaUpdater_.isConfigured(otaConfig)) {
    return;
  }

  Serial.println("[ota] auto-check enabled");
  startBackgroundOtaCheck(otaConfig);
}

bool App::startBackgroundOtaCheck(const OtaUpdater::Config &config) {
  if (otaCheckInProgress_) {
    Serial.println("[ota] background check already running");
    return false;
  }

  if (otaCheckQueue_ == nullptr) {
    otaCheckQueue_ = xQueueCreate(1, sizeof(OtaCheckResult));
    if (otaCheckQueue_ == nullptr) {
      Serial.println("[ota] could not create result queue");
      return false;
    }
  }
  xQueueReset(otaCheckQueue_);

  OtaCheckTaskParams *params = new OtaCheckTaskParams();
  if (params == nullptr) {
    Serial.println("[ota] could not allocate task params");
    return false;
  }
  params->config = config;
  params->resultQueue = otaCheckQueue_;

  otaCheckInProgress_ = true;
  BaseType_t created = xTaskCreatePinnedToCore(otaCheckTask, "ota_check",
                                               kOtaCheckTaskStackBytes, params, 1, nullptr, 0);
  if (created != pdPASS) {
    Serial.printf("[ota] background task create failed: %ld\n", static_cast<long>(created));
    otaCheckInProgress_ = false;
    delete params;
    return false;
  }

  Serial.println("[ota] background check started");
  return true;
}

void App::otaCheckTask(void *params) {
  OtaCheckTaskParams *taskParams = static_cast<OtaCheckTaskParams *>(params);
  if (taskParams == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  OtaCheckResult queuedResult;

  const OtaUpdater::Result result =
      OtaUpdater().checkOnly(taskParams->config, nullptr, nullptr);
  queuedResult.code = result.code;
  copyOtaLabel(queuedResult.currentVersion, sizeof(queuedResult.currentVersion),
               result.currentVersion);
  copyOtaLabel(queuedResult.latestVersion, sizeof(queuedResult.latestVersion),
               result.latestVersion);
  copyOtaLabel(queuedResult.summary, sizeof(queuedResult.summary), result.summary);
  copyOtaLabel(queuedResult.detail, sizeof(queuedResult.detail), result.detail);

  if (taskParams->resultQueue != nullptr) {
    xQueueOverwrite(taskParams->resultQueue, &queuedResult);
  }

  delete taskParams;
  vTaskDelete(nullptr);
}

void App::pollOtaCheckResult(uint32_t nowMs) {
  (void)nowMs;
  if (otaCheckQueue_ == nullptr) {
    return;
  }

  OtaCheckResult result;
  while (xQueueReceive(otaCheckQueue_, &result, 0) == pdTRUE) {
    otaCheckInProgress_ = false;
    Serial.printf("[ota] background result code=%u current=%s latest=%s summary=%s detail=%s\n",
                  static_cast<unsigned int>(result.code), result.currentVersion,
                  result.latestVersion, result.summary, result.detail);

    if (result.code == OtaUpdater::ResultCode::UpdateAvailable) {
      pendingUpdateCurrentVersion_ = String(result.currentVersion);
      pendingUpdateNewVersion_ = String(result.latestVersion);
      otaUpdatePromptPending_ = true;
    }
  }
}

bool App::updateConfirmCanOpen() const {
  return otaUpdatePromptPending_ && !pendingBootBookLoad_ && state_ == AppState::Paused;
}

void App::maybeOpenUpdateConfirm(uint32_t nowMs) {
  if (!updateConfirmCanOpen()) {
    return;
  }

  otaUpdatePromptPending_ = false;
  setState(AppState::Menu, nowMs);
  openUpdateConfirm();
}

bool App::blockNetworkActionForOtaCheck(const String &title, uint32_t nowMs) {
  pollOtaCheckResult(nowMs);
  if (!otaCheckInProgress_) {
    return false;
  }

  display_.renderStatus(title, "OTA check running", "Try again soon");
  delay(1200);
  renderMenu();
  return true;
}

void App::runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs) {
  if (blockNetworkActionForOtaCheck("OTA", nowMs)) {
    return;
  }

  if (!automatic) {
    otaUpdatePromptPending_ = false;
  }

  if (!otaUpdater_.isConfigured(config)) {
    if (!automatic) {
      display_.renderStatus("OTA", "Wi-Fi not set", "Settings -> Wi-Fi");
      delay(1600);
      if (state_ == AppState::Menu &&
          menu::isSettingsScreen(menuScreen_)) {
        rebuildSettingsMenuItems();
        renderSettings();
      } else {
        menuScreen_ = MenuScreen::Main;
        setState(AppState::Paused, nowMs);
      }
    }
    return;
  }

  saveReadingPosition(true);
  const OtaUpdater::Result result =
      otaUpdater_.checkAndInstall(config, &App::handleStorageStatus, this);

  Serial.printf("[ota] code=%u current=%s latest=%s summary=%s detail=%s\n",
                static_cast<unsigned int>(result.code), result.currentVersion.c_str(),
                result.latestVersion.c_str(), result.summary.c_str(), result.detail.c_str());

  if (result.rebootRequired) {
    display_.renderStatus("OTA", "Restarting", result.latestVersion);
    delay(300);
    ESP.restart();
    return;
  }

  if (automatic) {
    return;
  }

  const String line2 = result.detail.isEmpty() ? result.currentVersion : result.detail;
  display_.renderStatus("OTA", result.summary, line2);
  delay(1600);
  if (state_ == AppState::Menu &&
      menu::isSettingsScreen(menuScreen_)) {
    rebuildSettingsMenuItems();
    renderSettings();
  } else {
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
  }
}

String App::otaLastResultLabel() {
  // Decision 1: no guaranteed RTC, so store only the last result string (boot-
  // relative "this session"); don't fabricate a wall-clock timestamp.
  const String stored = preferences_.getString(kPrefOtaLastResult, "");
  return stored.isEmpty() ? "Not this session" : stored;
}

void App::runFirmwareCheckOnly(uint32_t nowMs) {
  if (blockNetworkActionForOtaCheck("OTA", nowMs)) {
    return;
  }

  const OtaUpdater::Config config = preferredOtaConfig();
  if (!otaUpdater_.isConfigured(config)) {
    display_.renderStatus("OTA", "Wi-Fi not set", "Settings -> Wi-Fi");
    delay(1600);
    rebuildSettingsMenuItems();
    renderSettings();
    return;
  }

  const OtaUpdater::Result result =
      otaUpdater_.checkOnly(config, &App::handleStorageStatus, this);

  Serial.printf("[ota] check-only code=%u current=%s latest=%s summary=%s\n",
                static_cast<unsigned int>(result.code), result.currentVersion.c_str(),
                result.latestVersion.c_str(), result.summary.c_str());

  String resultLabel = result.summary;
  if (resultLabel.isEmpty()) {
    resultLabel = result.code == OtaUpdater::ResultCode::NoUpdate ? "Up to date" : "Check failed";
  }
  preferences_.putString(kPrefOtaLastResult, resultLabel);

  const String line2 = result.detail.isEmpty() ? result.latestVersion : result.detail;
  display_.renderStatus("OTA", resultLabel, line2);
  delay(1600);

  // If an update is available, offer to install it via the existing prompt path.
  if (result.code == OtaUpdater::ResultCode::UpdateAvailable) {
    runFirmwareUpdate(config, false, nowMs);
    return;
  }

  rebuildSettingsMenuItems();
  renderSettings();
}

void App::runRssFeedCheck(uint32_t nowMs) {
  (void)nowMs;
  if (blockNetworkActionForOtaCheck("RSS", nowMs)) {
    return;
  }

  saveReadingPosition(true);

  display_.renderStatus("RSS", "Checking feeds", "Please wait");
  const RssFeedManager::Result result =
      rssFeedManager_.checkFeeds(preferredOtaConfig(), preferences_, &App::handleStorageStatus, this);

  Serial.printf("[rss] feeds=%u saved=%u skipped=%u summary=%s detail=%s\n",
                static_cast<unsigned int>(result.feedsChecked),
                static_cast<unsigned int>(result.articlesSaved),
                static_cast<unsigned int>(result.articlesSkipped), result.summary.c_str(),
                result.detail.c_str());

  storage_.refreshBooks(false);
  display_.renderStatus("RSS", result.summary, result.detail);
  delay(1800);
  renderMainMenu();
}

String App::pacingDelayLabel(uint16_t delayMs) const { return String(delayMs) + " ms"; }

String App::firmwareUpdateMenuLabel() const { return "Firmware update"; }

String App::uiText(UiText key) const { return Localization::text(uiLanguage_, key); }

String App::themeModeLabel() const {
  if (nightMode_) {
    return uiText(UiText::Night);
  }
  return darkMode_ ? uiText(UiText::Dark) : uiText(UiText::Light);
}

String App::phantomWordsLabel() const {
  return phantomWordsEnabled_ ? uiText(UiText::On) : uiText(UiText::Off);
}

String App::focusHighlightLabel() const {
  return typographyConfig_.focusHighlight ? uiText(UiText::On) : uiText(UiText::Off);
}

String App::uiLanguageLabel() const { return Localization::languageName(uiLanguage_); }

String App::readerModeLabel() const {
  switch (readerMode_) {
    case ReaderMode::Scroll:
      return uiText(UiText::ScrollMode);
    case ReaderMode::Rsvp:
    default:
      return uiText(UiText::RsvpMode);
  }
}

String App::pauseModeLabel() const {
  return pauseMode_ == PauseMode::Instant ? "Instant" : "Sentence";
}

String App::handednessLabel() const {
  return handednessMode_ == HandednessMode::Left ? "Left" : "Right";
}

String App::readerFontSizeLabel() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }

  switch (levelIndex) {
    case 0:
      return uiText(UiText::Large);
    case 1:
      return uiText(UiText::Medium);
    case 2:
    default:
      return uiText(UiText::Small);
  }
}

String App::readerTypefaceLabel() const {
  switch (typographyConfig_.typeface) {
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return "Atkinson";
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return "OpenDyslexic";
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return uiText(UiText::Standard);
  }
}

String App::typographyTuningLabel() const {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      return uiText(UiText::Back);
    case TypographyTuningFontSize:
      return uiText(UiText::FontSize);
    case TypographyTuningTypeface:
      return uiText(UiText::Typeface);
    case TypographyTuningPhantomWords:
      return uiText(UiText::PhantomWords);
    case TypographyTuningFocusHighlight:
      return uiText(UiText::RedHighlight);
    case TypographyTuningTracking:
      return uiText(UiText::Tracking);
    case TypographyTuningAnchor:
      return uiText(UiText::Anchor);
    case TypographyTuningGuideWidth:
      return uiText(UiText::GuideWidth);
    case TypographyTuningGuideGap:
      return uiText(UiText::GuideGap);
    case TypographyTuningReset:
      return uiText(UiText::Reset);
    default:
      return uiText(UiText::Typography);
  }
}

String App::typographyTuningValueLabel() const {
  switch (typographyTuningSelectedIndex_) {
    case TypographyTuningBack:
      return uiText(UiText::TapToExit);
    case TypographyTuningFontSize:
      return readerFontSizeLabel();
    case TypographyTuningTypeface:
      return readerTypefaceLabel();
    case TypographyTuningPhantomWords:
      return phantomWordsLabel();
    case TypographyTuningFocusHighlight:
      return focusHighlightLabel();
    case TypographyTuningTracking:
      return String(typographyConfig_.trackingPx >= 0 ? "+" : "") +
             String(static_cast<int>(typographyConfig_.trackingPx)) + " px";
    case TypographyTuningAnchor:
      return String(static_cast<unsigned int>(effectiveAnchorPercent())) + "%";
    case TypographyTuningGuideWidth:
      return String(static_cast<unsigned int>(typographyConfig_.guideHalfWidth)) + " px";
    case TypographyTuningGuideGap:
      return String(static_cast<unsigned int>(typographyConfig_.guideGap)) + " px";
    case TypographyTuningReset:
      return uiText(UiText::TapToReset);
    default:
      return "";
  }
}

void App::openBookPicker(bool articlesOnly) {
  storage_.refreshBooks();
  bookMenuItems_.clear();
  bookPickerBookIndices_.clear();
  bookMenuItems_.push_back({uiText(UiText::Back), ""});

  const size_t count = storage_.bookCount();
  std::vector<size_t> sortedBookIndices;
  sortedBookIndices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    if (storage_.bookIsArticle(i) != articlesOnly) {
      continue;
    }
    sortedBookIndices.push_back(i);
  }

  std::stable_sort(sortedBookIndices.begin(), sortedBookIndices.end(),
                   [this](size_t leftIndex, size_t rightIndex) {
                     const bool leftCurrent =
                         usingStorageBook_ && leftIndex == currentBookIndex_;
                     const bool rightCurrent =
                         usingStorageBook_ && rightIndex == currentBookIndex_;
                     if (leftCurrent != rightCurrent) {
                       return leftCurrent;
                     }

                     const uint32_t leftRecent =
                         bookProgress_.recentSequence(storage_.bookPath(leftIndex));
                     const uint32_t rightRecent =
                         bookProgress_.recentSequence(storage_.bookPath(rightIndex));
                     const bool leftHasRecent = leftRecent > 0;
                     const bool rightHasRecent = rightRecent > 0;
                     if (leftHasRecent != rightHasRecent) {
                       return leftHasRecent;
                     }
                     if (leftRecent != rightRecent) {
                       return leftRecent > rightRecent;
                     }

                     return false;
                   });

  for (size_t bookIndex : sortedBookIndices) {
    bookPickerBookIndices_.push_back(bookIndex);
    bookMenuItems_.push_back(libraryItemForBook(bookIndex));
  }

  if (sortedBookIndices.empty()) {
    Serial.printf("[book-picker] No SD %s available\n", articlesOnly ? "articles" : "books");
  }

  menuScreen_ = MenuScreen::BookPicker;
  bookPickerSelectedIndex_ = kBookPickerBackIndex;
  if (usingStorageBook_) {
    for (size_t row = 0; row < bookPickerBookIndices_.size(); ++row) {
      if (bookPickerBookIndices_[row] == currentBookIndex_) {
        bookPickerSelectedIndex_ = row + 1;
        break;
      }
    }
  }
  renderBookPicker();
}

void App::selectBookPickerItem(uint32_t nowMs) {
  if (bookPickerSelectedIndex_ == kBookPickerBackIndex || bookMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  const size_t rowIndex = bookPickerSelectedIndex_ - 1;
  if (rowIndex >= bookPickerBookIndices_.size()) {
    renderBookPicker();
    return;
  }

  const size_t bookIndex = bookPickerBookIndices_[rowIndex];
  saveReadingPosition(true);
  if (!loadBookAtIndex(bookIndex, nowMs)) {
    Serial.println("[book-picker] Failed to load selected book");
    display_.renderStatus("Book open failed", storage_.bookDisplayName(bookIndex),
                          "Check serial log");
    delay(1800);
    renderBookPicker();
    return;
  }

  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
}

void App::toggleCurrentBookFinished(uint32_t nowMs) {
  if (!usingStorageBook_ || currentBookPath_.isEmpty()) {
    display_.renderStatus("Mark finished", "No book open", "");
    delay(1200);
    renderMainMenu();
    return;
  }

  const bool nowFinished = !bookProgress_.isFinished(currentBookPath_);
  // Finished is orthogonal to the saved position -- we never reset position here,
  // so re-reads still resume where the reader left off.
  bookProgress_.setFinished(currentBookPath_, nowFinished);
  Serial.printf("[app] book %s marked %s\n", currentBookPath_.c_str(),
                nowFinished ? "finished" : "unread");
  display_.renderStatus("Mark finished", currentBookTitle_.c_str(),
                        nowFinished ? "Marked finished" : "Marked unread");
  delay(1200);
  renderMainMenu();
}

void App::openChapterPicker() {
  chapterMenuItems_.clear();
  chapterMenuItems_.push_back(uiText(UiText::Back));

  if (chapterMarkers_.empty()) {
    chapterMenuItems_.push_back(uiText(UiText::StartOfBook));
    chapterPickerSelectedIndex_ = kChapterPickerFallbackIndex;
    Serial.println("[chapter-picker] No chapter markers found; showing start fallback");
  } else {
    for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
      chapterMenuItems_.push_back(chapterMenuLabel(i));
    }

    size_t selectedChapter = 0;
    const size_t currentWordIndex = reader_.currentIndex();
    for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
      if (chapterMarkers_[i].wordIndex <= currentWordIndex) {
        selectedChapter = i;
      }
    }
    chapterPickerSelectedIndex_ = selectedChapter + 1;
  }

  chapterMenuItems_.push_back(uiText(UiText::RestartBook));

  menuScreen_ = MenuScreen::ChapterPicker;
  renderChapterPicker();
}

void App::selectChapterPickerItem(uint32_t nowMs) {
  if (chapterPickerSelectedIndex_ == kChapterPickerBackIndex || chapterMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  const size_t restartIndex = chapterMenuItems_.size() - 1;
  if (chapterPickerSelectedIndex_ == restartIndex) {
    openRestartConfirm();
    return;
  }

  if (chapterMarkers_.empty()) {
    reader_.seekTo(0);
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
    saveReadingPosition(true);
    Serial.println("[chapter-picker] jumped to start of book");
    return;
  }

  const size_t chapterIndex = chapterPickerSelectedIndex_ - 1;
  if (chapterIndex >= chapterMarkers_.size()) {
    renderChapterPicker();
    return;
  }

  reader_.seekTo(chapterMarkers_[chapterIndex].wordIndex);
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.printf("[chapter-picker] jumped to %s at word %u\n",
                chapterMarkers_[chapterIndex].title.c_str(),
                static_cast<unsigned int>(chapterMarkers_[chapterIndex].wordIndex));
}

void App::openBookmarkPicker() {
  bookmarkMenuItems_.clear();
  bookmarkMenuWordIndices_.clear();
  bookmarkMenuItems_.push_back(uiText(UiText::Back));

  if (!usingStorageBook_ || currentBookPath_.isEmpty()) {
    display_.renderStatus("Bookmarks", "No book open", "");
    delay(1200);
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  // First action always drops a mark at the current word.
  bookmarkMenuItems_.push_back("+ Drop mark here");

  const std::vector<uint32_t> marks = bookProgress_.bookmarks(currentBookPath_);
  const uint32_t wordCount = static_cast<uint32_t>(reader_.wordCount());
  for (uint32_t wordIndex : marks) {
    bookmarkMenuWordIndices_.push_back(wordIndex);
    String label = "@ " + String(static_cast<unsigned int>(wordIndex + 1));
    uint8_t percent = 0;
    if (bookprogress::progressPercent(wordIndex, wordCount, percent)) {
      label += " - " + String(percent) + "%";
    }
    bookmarkMenuItems_.push_back(label);
  }

  bookmarkPickerSelectedIndex_ = kBookmarkPickerDropIndex;
  menuScreen_ = MenuScreen::BookmarkPicker;
  Serial.printf("[bookmark-picker] %u marks for %s\n",
                static_cast<unsigned int>(marks.size()), currentBookPath_.c_str());
  renderBookmarkPicker();
}

void App::selectBookmarkPickerItem(uint32_t nowMs) {
  if (bookmarkPickerSelectedIndex_ == kBookmarkPickerBackIndex ||
      bookmarkMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  if (bookmarkPickerSelectedIndex_ == kBookmarkPickerDropIndex) {
    const uint32_t wordIndex = static_cast<uint32_t>(reader_.currentIndex());
    const bool added = bookProgress_.addBookmark(currentBookPath_, wordIndex);
    Serial.printf("[bookmark-picker] drop at word=%u %s\n",
                  static_cast<unsigned int>(wordIndex), added ? "added" : "exists");
    display_.renderStatus("Bookmarks",
                          added ? "Mark dropped" : "Already marked",
                          "");
    delay(900);
    openBookmarkPicker();
    return;
  }

  // Remaining rows map to saved marks (offset past Back + Drop).
  const size_t markRow = bookmarkPickerSelectedIndex_ - kBookmarkPickerFirstMarkIndex;
  if (markRow >= bookmarkMenuWordIndices_.size()) {
    renderBookmarkPicker();
    return;
  }

  const uint32_t wordIndex = bookmarkMenuWordIndices_[markRow];
  reader_.seekTo(wordIndex);
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.printf("[bookmark-picker] jumped to word=%u\n", static_cast<unsigned int>(wordIndex));
}

void App::renderBookmarkPicker() {
  display_.renderMenu(bookmarkMenuItems_, bookmarkPickerSelectedIndex_);
}

namespace {
// Starred picker row layout: Back, then the remove-mode toggle, then one row per
// saved quote.
constexpr size_t kStarredPickerBackIndex = 0;
constexpr size_t kStarredPickerModeIndex = 1;
constexpr size_t kStarredPickerFirstQuoteIndex = 2;
}  // namespace

void App::openStarredPicker() {
  starredQuotes_ = quoteStore_.loadAll();
  starredMenuItems_.clear();
  starredMenuItems_.push_back(uiText(UiText::Back));
  starredMenuItems_.push_back(starredRemoveMode_ ? "Remove: tap to delete"
                                                 : "Remove: off (tap to jump)");

  for (const quotes::Quote &quote : starredQuotes_) {
    starredMenuItems_.push_back(quotes::quoteListLabel(quote));
  }

  if (starredQuotes_.empty()) {
    starredMenuItems_.push_back("(No starred sentences)");
  }

  if (starredPickerSelectedIndex_ >= starredMenuItems_.size()) {
    starredPickerSelectedIndex_ = kStarredPickerModeIndex;
  }
  menuScreen_ = MenuScreen::StarredPicker;
  Serial.printf("[starred-picker] %u quotes\n",
                static_cast<unsigned int>(starredQuotes_.size()));
  renderStarredPicker();
}

void App::selectStarredPickerItem(uint32_t nowMs) {
  if (starredPickerSelectedIndex_ == kStarredPickerBackIndex ||
      starredMenuItems_.size() <= 1) {
    menuScreen_ = MenuScreen::Main;
    renderMainMenu();
    return;
  }

  if (starredPickerSelectedIndex_ == kStarredPickerModeIndex) {
    starredRemoveMode_ = !starredRemoveMode_;
    openStarredPicker();
    return;
  }

  const size_t quoteRow = starredPickerSelectedIndex_ - kStarredPickerFirstQuoteIndex;
  if (quoteRow >= starredQuotes_.size()) {
    renderStarredPicker();  // The empty-state row or a stale index: just redraw.
    return;
  }

  const quotes::Quote quote = starredQuotes_[quoteRow];

  if (starredRemoveMode_) {
    const bool removed = quoteStore_.remove(quote.bookPath, quote.wordIndex);
    Serial.printf("[starred-picker] remove word=%u %s\n",
                  static_cast<unsigned int>(quote.wordIndex),
                  removed ? "removed" : "absent");
    display_.renderStatus("Starred", removed ? "Removed" : "Already gone", "");
    delay(700);
    if (starredPickerSelectedIndex_ > kStarredPickerFirstQuoteIndex) {
      --starredPickerSelectedIndex_;
    }
    openStarredPicker();
    return;
  }

  if (!jumpToQuote(quote, nowMs)) {
    display_.renderStatus("Starred", "Open failed", quote.bookTitle);
    delay(1200);
    openStarredPicker();
  }
}

bool App::jumpToQuote(const quotes::Quote &quote, uint32_t nowMs) {
  // Load the quote's book if it is not already the open one, then seek to the
  // sentence start -- the same path the bookmark picker uses, plus a cross-book
  // load like the book picker.
  if (!usingStorageBook_ || currentBookPath_ != quote.bookPath) {
    const int bookIndex = findBookIndexByPath(quote.bookPath);
    if (bookIndex < 0) {
      Serial.printf("[starred-picker] book not found: %s\n", quote.bookPath.c_str());
      return false;
    }
    saveReadingPosition(true);
    if (!loadBookAtIndex(static_cast<size_t>(bookIndex), nowMs)) {
      return false;
    }
  }

  reader_.seekTo(quote.wordIndex);
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.printf("[starred-picker] jumped to word=%u in %s\n",
                static_cast<unsigned int>(quote.wordIndex), quote.bookPath.c_str());
  return true;
}

void App::renderStarredPicker() {
  display_.renderMenu(starredMenuItems_, starredPickerSelectedIndex_);
}

namespace {
constexpr const char *kStatsDir = "/rsvp/.stats";
constexpr const char *kStatsFile = "/rsvp/.stats/stats.json";

// Extract an unsigned integer that follows "\"key\":" in a small JSON blob.
// Our own file, so a tolerant manual scan beats pulling in a JSON parser.
uint64_t jsonUint(const String &json, const char *key) {
  String needle = String("\"") + key + "\":";
  const int at = json.indexOf(needle);
  if (at < 0) {
    return 0;
  }
  int i = at + static_cast<int>(needle.length());
  while (i < static_cast<int>(json.length()) && (json[i] == ' ' || json[i] == '\t')) {
    ++i;
  }
  uint64_t value = 0;
  while (i < static_cast<int>(json.length()) && json[i] >= '0' && json[i] <= '9') {
    value = value * 10ULL + static_cast<uint64_t>(json[i] - '0');
    ++i;
  }
  return value;
}
}  // namespace

void App::loadReadingStats() {
  // Each power-on is a fresh "day" bucket: with no RTC/NTP on this device we
  // cannot tell calendar days apart, so we bucket per session. (The pure module
  // takes a day key, so a real date key can replace this later untouched.)
  statsSessionDayKey_ = static_cast<uint32_t>(bootStartedMs_ ^ 0x9E3779B9UL);
  if (statsSessionDayKey_ == 0) {
    statsSessionDayKey_ = 1;
  }

  stats::Snapshot snapshot;
  bool loaded = false;
  if (storageReady_) {
    File file = SD_MMC.open(kStatsFile, FILE_READ);
    if (file && !file.isDirectory()) {
      String json;
      json.reserve(static_cast<unsigned int>(file.size()) + 1);
      while (file.available()) {
        json += static_cast<char>(file.read());
      }
      file.close();
      snapshot.totalWords = jsonUint(json, "totalWords");
      snapshot.totalMs = jsonUint(json, "totalMs");
      snapshot.dayWords = jsonUint(json, "dayWords");
      snapshot.dayMs = jsonUint(json, "dayMs");
      snapshot.dayKey = static_cast<uint32_t>(jsonUint(json, "dayKey"));
      loaded = snapshot.totalWords > 0 || snapshot.totalMs > 0;
    } else if (file) {
      file.close();
    }
  }

  if (!loaded) {
    // Fall back to the NVS mirror so the screen shows something pre-card-read.
    snapshot.totalWords = preferences_.getULong64(kPrefStatsWords, 0);
    snapshot.totalMs = preferences_.getULong64(kPrefStatsMs, 0);
  }

  readingStats_ = stats::ReadingStats(snapshot);
  Serial.printf("[stats] loaded words=%lu ms=%lu (sd=%u)\n",
                static_cast<unsigned long>(snapshot.totalWords),
                static_cast<unsigned long>(snapshot.totalMs), loaded ? 1 : 0);
}

void App::flushReadingStats() {
  const stats::Snapshot &snapshot = readingStats_.snapshot();

  // Mirror the all-time totals to NVS for instant boot display.
  preferences_.putULong64(kPrefStatsWords, snapshot.totalWords);
  preferences_.putULong64(kPrefStatsMs, snapshot.totalMs);

  if (!storageReady_) {
    return;
  }

  SD_MMC.mkdir("/rsvp");
  SD_MMC.mkdir(kStatsDir);
  const String tmpPath = String(kStatsFile) + ".tmp";
  SD_MMC.remove(tmpPath);
  File file = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!file) {
    Serial.printf("[stats] could not write %s\n", tmpPath.c_str());
    return;
  }
  file.print("{\"totalWords\":");
  file.print(static_cast<unsigned long>(snapshot.totalWords));
  file.print(",\"totalMs\":");
  file.print(static_cast<unsigned long>(snapshot.totalMs));
  file.print(",\"dayKey\":");
  file.print(static_cast<unsigned long>(snapshot.dayKey));
  file.print(",\"dayWords\":");
  file.print(static_cast<unsigned long>(snapshot.dayWords));
  file.print(",\"dayMs\":");
  file.print(static_cast<unsigned long>(snapshot.dayMs));
  file.println("}");
  file.close();

  SD_MMC.remove(kStatsFile);
  if (!SD_MMC.rename(tmpPath, kStatsFile)) {
    Serial.println("[stats] rename of stats file failed");
  }
}

void App::beginStatsSession(uint32_t nowMs) {
  if (!usingStorageBook_) {
    statsPlayStartWordIndex_ = static_cast<size_t>(-1);
    return;
  }
  statsPlayStartMs_ = nowMs;
  statsPlayStartWordIndex_ = reader_.currentIndex();
}

void App::endStatsSession(uint32_t nowMs) {
  if (statsPlayStartWordIndex_ == static_cast<size_t>(-1)) {
    return;
  }

  const size_t endIndex = reader_.currentIndex();
  // Decrement-safe: scrub-back during Playing must never subtract from totals.
  const size_t words =
      endIndex > statsPlayStartWordIndex_ ? endIndex - statsPlayStartWordIndex_ : 0;
  const uint32_t elapsedMs = nowMs >= statsPlayStartMs_ ? nowMs - statsPlayStartMs_ : 0;
  statsPlayStartWordIndex_ = static_cast<size_t>(-1);

  if (words == 0 && elapsedMs == 0) {
    return;
  }

  readingStats_.recordSession(statsSessionDayKey_, static_cast<uint32_t>(words), elapsedMs);
  flushReadingStats();
  Serial.printf("[stats] session words=%u ms=%u total=%lu\n", static_cast<unsigned int>(words),
                static_cast<unsigned int>(elapsedMs),
                static_cast<unsigned long>(readingStats_.snapshot().totalWords));
}

size_t App::countFinishedBooks() {
  size_t finished = 0;
  for (size_t i = 0; i < storage_.bookCount(); ++i) {
    if (bookProgress_.isFinished(storage_.bookPath(i))) {
      ++finished;
    }
  }
  return finished;
}

void App::openStatsScreen() {
  const stats::Snapshot &snapshot = readingStats_.snapshot();
  const unsigned long minutes = static_cast<unsigned long>(snapshot.totalMs / 60000ULL);
  const unsigned long avgWpm = static_cast<unsigned long>(readingStats_.averageWpm());
  const size_t finished = countFinishedBooks();

  const String line1 = String(static_cast<unsigned long>(snapshot.totalWords)) + " words  " +
                       String(minutes) + " min";
  const String line2 = String(avgWpm) + " wpm avg  " + String(static_cast<unsigned int>(finished)) +
                       " finished";
  display_.renderStatus("Reading stats", line1, line2);
  // No RTC on this device, so totals are per-session-bucketed actuals, not
  // calendar days. Hold the screen, then return to the menu.
  delay(2500);
  renderMainMenu();
}

void App::openRestartConfirm() {
  restartConfirmReturnScreen_ = menuScreen_;
  restartConfirmSelectedIndex_ = RestartConfirmNo;
  menuScreen_ = MenuScreen::RestartConfirm;
  renderRestartConfirm();
}

void App::selectRestartConfirmItem(uint32_t nowMs) {
  if (restartConfirmSelectedIndex_ != RestartConfirmYes) {
    menuScreen_ = restartConfirmReturnScreen_;
    renderMenu();
    return;
  }

  reader_.begin(nowMs);
  restartConfirmReturnScreen_ = MenuScreen::Main;
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
  saveReadingPosition(true);
  Serial.println("[restart] book restarted from beginning");
}

void App::openSdCardRepairConfirm() {
  sdCardRepairConfirmSelectedIndex_ = SdCardRepairConfirmNo;
  menuScreen_ = MenuScreen::SdCardRepairConfirm;
  renderSdCardRepairConfirm();
}

void App::selectSdCardRepairConfirmItem(uint32_t nowMs) {
  if (sdCardRepairConfirmSelectedIndex_ != SdCardRepairConfirmYes) {
    Serial.println("[sd-check] folder repair declined");
    menuScreen_ = MenuScreen::Main;
    renderMenu();
    return;
  }

  runSdCardRepair(nowMs);
}

void App::openUpdateConfirm() {
  updateConfirmSelectedIndex_ = UpdateConfirmSkip;
  menuScreen_ = MenuScreen::UpdateConfirm;
  renderUpdateConfirm();
}

void App::selectUpdateConfirmItem(uint32_t nowMs) {
  if (updateConfirmSelectedIndex_ != UpdateConfirmUpdate) {
    Serial.println("[ota] update skipped by user");
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Paused, nowMs);
    return;
  }

  Serial.println("[ota] update confirmed by user");
  runFirmwareUpdate(preferredOtaConfig(), false, nowMs);
}

void App::enterCompanionSync(uint32_t nowMs) {
  if (blockNetworkActionForOtaCheck("Sync", nowMs)) {
    return;
  }

  Serial.println("[app] entering companion sync mode");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  display_.renderStatus("Sync", "Starting Wi-Fi", "");

  OtaUpdater::Config wifiConfig = preferredOtaConfig();
  CompanionSyncManager::Config syncConfig;
  syncConfig.wifiSsid = wifiConfig.wifiSsid;
  syncConfig.wifiPassword = wifiConfig.wifiPassword;

  if (!companionSync_.begin(syncConfig)) {
    Serial.println("[app] companion sync failed");
    display_.renderStatus("Sync", "Could not start", "Returning");
    delay(1400);
    menuScreen_ = MenuScreen::Main;
    setState(AppState::Menu, nowMs);
    return;
  }

  lastCompanionSyncRenderMs_ = 0;
  setState(AppState::CompanionSync, nowMs);
}

void App::updateCompanionSync(uint32_t nowMs) {
  companionSync_.update();

  if (powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs) {
    powerButtonLongPressHandled_ = true;
    exitCompanionSync(nowMs);
    return;
  }

  if (nowMs - lastCompanionSyncRenderMs_ >= 1000) {
    lastCompanionSyncRenderMs_ = nowMs;
    display_.renderStatus("Sync", companionSync_.statusLine1(), companionSync_.statusLine2());
  }
}

void App::exitCompanionSync(uint32_t nowMs) {
  Serial.println("[app] leaving companion sync mode");
  display_.renderStatus("Sync", "Stopping", "");
  companionSync_.end();
  preferences_.end();
  preferences_.begin(kPrefsNamespace, false);
  reloadRuntimePreferences(nowMs, false);
  storage_.refreshBooks();
  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
}

void App::runSdCardCheck(uint32_t nowMs) {
  (void)nowMs;
  Serial.println("[app] running SD card check");
  display_.renderStatus("SD check", "Starting", "");
  const StorageManager::DiagnosticResult result = storage_.diagnoseSdCard();

  if (sdCardFolderRepairNeeded(result)) {
    display_.renderStatus("SD check", "Folders missing", "Confirm repair");
    delay(900);
    openSdCardRepairConfirm();
    return;
  }

  String detail = result.detail;
  if (detail.isEmpty() && result.mounted) {
    detail = String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  }
  display_.renderStatus("SD check", result.summary, detail);
  delay(2600);

  menuScreen_ = MenuScreen::Main;
  renderMenu();
}

void App::runSdCardRepair(uint32_t nowMs) {
  (void)nowMs;
  Serial.println("[app] repairing SD card folder layout");
  display_.renderStatus("SD check", "Repairing folders", "Please wait");
  const bool repaired = storage_.repairSdCardFolders();
  if (!repaired) {
    display_.renderStatus("SD check", "Folder repair failed", "Format FAT32 MBR");
    delay(2600);
    menuScreen_ = MenuScreen::Main;
    renderMenu();
    return;
  }

  display_.renderStatus("SD check", "Folders repaired", "Checking card");
  delay(900);

  const StorageManager::DiagnosticResult result = storage_.diagnoseSdCard();
  String detail = result.detail;
  if (detail.isEmpty() && result.mounted) {
    detail = String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  }
  display_.renderStatus("SD check", result.summary, detail);
  delay(2600);

  menuScreen_ = MenuScreen::Main;
  renderMenu();
}

void App::enterUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] entering USB transfer mode");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  const size_t resumeIndex = reader_.currentIndex();
  setState(AppState::UsbTransfer, nowMs);

  activeBookStore_.close();
  storage_.end();
  if (!usbTransfer_.begin(true)) {
    Serial.printf("[app] USB transfer failed: %s\n", usbTransfer_.statusMessage());
    display_.renderStatus("USB", "SD not ready", "Returning");
    storageReady_ = storage_.begin();
    if (storageReady_ && usingStorageBook_ && !currentBookPath_.isEmpty()) {
      const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
      if (refreshedBookIndex >= 0 &&
          loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs, false, false, false,
                          false)) {
        reader_.seekTo(resumeIndex);
      }
    }
    setState(AppState::Paused, nowMs);
    return;
  }

  const uint64_t sizeMb = usbTransfer_.cardSizeBytes() / (1024ULL * 1024ULL);
  Serial.printf("[app] USB transfer active (%llu MB). Eject from computer when finished.\n",
                sizeMb);
  display_.renderStatus("USB", "Copy books now", "Eject then hold PWR");
}

void App::updateUsbTransfer(uint32_t nowMs) {
  if (!usbTransfer_.active()) {
    return;
  }

  const bool powerExitRequested =
      powerButton_.isHeld() && nowMs - powerButton_.lastEdgeMs() >= kUsbTransferExitHoldMs;
  if (!usbTransfer_.ejected() && !powerExitRequested) {
    return;
  }

  if (powerExitRequested && !usbTransfer_.ejected()) {
    Serial.println("[app] leaving USB transfer by PWR hold; make sure host was ejected first");
  }

  if (powerExitRequested) {
    powerButtonLongPressHandled_ = true;
  }

  exitUsbTransfer(nowMs);
}

void App::exitUsbTransfer(uint32_t nowMs) {
  Serial.println("[app] USB transfer ejected; remounting SD");
  display_.renderStatus("USB", "Remounting SD", "");
  usbTransfer_.end();

  storageReady_ = storage_.begin();
  if (storageReady_) {
    const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
    if (refreshedBookIndex >= 0) {
      const size_t resumeIndex = reader_.currentIndex();
      if (loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs, false, false, false,
                          false)) {
        reader_.seekTo(resumeIndex);
      } else {
        Serial.println("[app] current indexed book unavailable after USB transfer");
        usingStorageBook_ = false;
        currentBookPath_ = "";
        currentBookTitle_ = "Demo";
        reader_.clearLoadedBook(nowMs);
        reader_.begin(nowMs);
      }
    } else if (storage_.bookCount() > 0) {
      loadBookAtIndex(0, nowMs);
    }
  } else {
    Serial.println("[app] SD remount failed after USB transfer");
  }

  menuScreen_ = MenuScreen::Main;
  setState(AppState::Paused, nowMs);
}

void App::enterStandby(uint32_t nowMs) {
  if (state_ == AppState::UsbTransfer || state_ == AppState::CompanionSync ||
      state_ == AppState::Sleeping || powerOffStarted_) {
    return;
  }

  standbyReturnState_ = state_ == AppState::Playing ? AppState::Paused : state_;
  if (standbyReturnState_ == AppState::Booting || standbyReturnState_ == AppState::Standby) {
    standbyReturnState_ = AppState::Paused;
  }

  if (state_ == AppState::Playing) {
    saveReadingPosition(true);
  }

  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  batteryWarningOverlayVisible_ = false;
  standbyButtonsReleased_ = false;
  standbyWakeTouchActive_ = false;
  lastStandbyFrameMs_ = 0;
  // Anchor the wake grace; a no-op when this entry was the decider's own
  // verdict (it already flipped its mode).
  standbyDecider_.noteStandbyEntered(nowMs);
  setState(AppState::Standby, nowMs);
  Serial.println("[app] standby screensaver started");
}

void App::exitStandby(uint32_t nowMs) {
  if (state_ != AppState::Standby) {
    return;
  }

  // Report the wake (a no-op after the decider's own lift verdict): counts as
  // activity so the idle timer doesn't immediately bounce back to standby, and
  // disarms set-down until the device leaves flat.
  standbyDecider_.noteWoke(nowMs);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  playLocked_ = false;
  pauseAtSentenceEndRequested_ = false;
  batteryWarningOverlayVisible_ = false;
  standbyButtonsReleased_ = false;

  AppState nextState = standbyReturnState_;
  if (nextState == AppState::Booting || nextState == AppState::Playing ||
      nextState == AppState::CompanionSync || nextState == AppState::UsbTransfer ||
      nextState == AppState::Standby || nextState == AppState::Sleeping) {
    nextState = AppState::Paused;
  }

  Serial.println("[app] leaving standby");
  if (standbyScreenOffActive_) {
    display_.wakeFromSleep();
    standbyScreenOffActive_ = false;
  }
  standbyScreenOffForced_ = false;
  setState(nextState, nowMs);
}

void App::handleStandbyTouchWake(uint32_t nowMs) {
  if (!touchInitialized_) {
    return;
  }

  TouchEvent ev;
  if (!touch_.poll(ev)) {
    return;
  }

  // Ignore any touch inside the wake-grace window so the entering button combo
  // (or a stray contact at sleep) does not instantly wake the device. The
  // decider owns the one grace gate every wake path consults.
  const bool pastGrace = standbyDecider_.canWakeNow(nowMs);
  if (!pastGrace || button_.isHeld() || powerButton_.isHeld()) {
    standbyWakeTouchActive_ = false;
    return;
  }

  if (ev.phase == TouchPhase::Start) {
    standbyWakeTouchActive_ = true;
    standbyWakeStartX_ = ev.x;
    standbyWakeStartY_ = ev.y;
    return;
  }

  if (!standbyWakeTouchActive_) {
    return;
  }

  if (ev.phase == TouchPhase::End) {
    const int absDeltaX = abs(static_cast<int>(ev.x) - static_cast<int>(standbyWakeStartX_));
    const int absDeltaY = abs(static_cast<int>(ev.y) - static_cast<int>(standbyWakeStartY_));
    standbyWakeTouchActive_ = false;
    // Decision 1: any tap (within the slop box) after the grace window wakes.
    // (exitStandby reports the wake to the decider, which counts it as
    // activity for the idle timer.)
    if (touchgesture::isTap(absDeltaX, absDeltaY)) {
      Serial.println("[app] tap wake from standby");
      exitStandby(nowMs);
    }
  }
}

void App::noteUserInput(uint32_t nowMs) { standbyDecider_.noteActivity(nowMs); }

void App::cycleIdleStandbyTimeout() {
  // Off -> 1 -> 2 -> 5 -> 10 -> 15 -> Off (minutes).
  switch (idleStandbyMinutes_) {
    case 0:
      idleStandbyMinutes_ = 1;
      break;
    case 1:
      idleStandbyMinutes_ = 2;
      break;
    case 2:
      idleStandbyMinutes_ = 5;
      break;
    case 5:
      idleStandbyMinutes_ = 10;
      break;
    case 10:
      idleStandbyMinutes_ = 15;
      break;
    default:
      idleStandbyMinutes_ = 0;
      break;
  }
  preferences_.putUChar(kPrefIdleStandbyMin, idleStandbyMinutes_);
  standbyDecider_.setIdleTimeoutMs(static_cast<uint32_t>(idleStandbyMinutes_) * 60000UL);
  standbyDecider_.noteActivity(millis());
}

String App::idleStandbyLabel() const {
  if (idleStandbyMinutes_ == 0) {
    return "Off";
  }
  return String(idleStandbyMinutes_) + " min";
}

namespace {

// On-device gesture sensitivity presets (decision 1). "Medium" reproduces the
// historical defaults exactly. Higher sensitivity = smaller thresholds / shorter
// hold (easier to trigger); lower = larger (more deliberate).
struct GesturePreset {
  const char *label;
  uint16_t swipePx;
  uint16_t tapPx;
  uint16_t scrubPx;
  uint32_t holdMs;
};

constexpr GesturePreset kGesturePresets[] = {
    {"Low", 56, 34, 30, 560},     // less sensitive
    {"Medium", 40, 26, 22, 420},  // = baked-in defaults
    {"High", 28, 18, 16, 300},    // more sensitive
};
constexpr size_t kGesturePresetCount = sizeof(kGesturePresets) / sizeof(kGesturePresets[0]);
constexpr size_t kGesturePresetMediumIndex = 1;

}  // namespace

void App::loadGestureConfig() {
  const GesturePreset &def = kGesturePresets[kGesturePresetMediumIndex];
  touchgesture::GestureConfig cfg;
  cfg.swipeThresholdPx = preferences_.getUShort(kPrefGestureSwipePx, def.swipePx);
  cfg.tapSlopPx = preferences_.getUShort(kPrefGestureTapPx, def.tapPx);
  cfg.scrubStepPx = preferences_.getUShort(kPrefGestureScrubPx, def.scrubPx);
  cfg.playHoldMs = preferences_.getUInt(kPrefGestureHoldMs, def.holdMs);
  gestureConfig_ = touchgesture::clampGestureConfig(cfg);
}

void App::cycleGestureSensitivity() {
  // Find the current preset (or treat a companion-set custom config as Medium's
  // neighbour), then advance to the next preset and persist its raw values.
  size_t current = kGesturePresetMediumIndex;
  for (size_t i = 0; i < kGesturePresetCount; ++i) {
    if (gestureConfig_.swipeThresholdPx == kGesturePresets[i].swipePx &&
        gestureConfig_.tapSlopPx == kGesturePresets[i].tapPx &&
        gestureConfig_.scrubStepPx == kGesturePresets[i].scrubPx &&
        gestureConfig_.playHoldMs == kGesturePresets[i].holdMs) {
      current = i;
      break;
    }
  }
  const size_t next = (current + 1) % kGesturePresetCount;
  const GesturePreset &preset = kGesturePresets[next];
  preferences_.putUShort(kPrefGestureSwipePx, preset.swipePx);
  preferences_.putUShort(kPrefGestureTapPx, preset.tapPx);
  preferences_.putUShort(kPrefGestureScrubPx, preset.scrubPx);
  preferences_.putUInt(kPrefGestureHoldMs, preset.holdMs);
  loadGestureConfig();
}

String App::gestureSensitivityLabel() const {
  for (size_t i = 0; i < kGesturePresetCount; ++i) {
    if (gestureConfig_.swipeThresholdPx == kGesturePresets[i].swipePx &&
        gestureConfig_.tapSlopPx == kGesturePresets[i].tapPx &&
        gestureConfig_.scrubStepPx == kGesturePresets[i].scrubPx &&
        gestureConfig_.playHoldMs == kGesturePresets[i].holdMs) {
      return kGesturePresets[i].label;
    }
  }
  return "Custom";  // raw values set over the companion
}

void App::applyAudioSettings() {
  audio_.setMuted(audioMuted_);
  audio_.setVolume(audioVolumePercent_);
}

void App::toggleAudioMute() {
  audioMuted_ = !audioMuted_;
  preferences_.putBool(kPrefAudioMuted, audioMuted_);
  applyAudioSettings();
}

void App::cycleAudioVolume() {
  // 25 -> 50 -> 75 -> 100 -> 25 (percent). Mute is the separate row for silence.
  if (audioVolumePercent_ < 50) {
    audioVolumePercent_ = 50;
  } else if (audioVolumePercent_ < 75) {
    audioVolumePercent_ = 75;
  } else if (audioVolumePercent_ < 100) {
    audioVolumePercent_ = 100;
  } else {
    audioVolumePercent_ = 25;
  }
  preferences_.putUChar(kPrefAudioVolume, audioVolumePercent_);
  applyAudioSettings();
}

String App::audioMuteLabel() const { return audioMuted_ ? "On" : "Off"; }

String App::audioVolumeLabel() const { return String(audioVolumePercent_) + "%"; }

uint32_t App::standbyRngSeed(uint32_t nowMs) const {
  return nowMs ^ micros() ^ (static_cast<uint32_t>(reader_.currentIndex() + 1) * 2654435761UL) ^
         (static_cast<uint32_t>(batteryMonitor_.displayedPercent()) << 24);
}

void App::seedStandbyScreensaver(uint32_t nowMs) {
  // Screen-off either by preference or forced for this entry (set-down
  // screen-down goes dark regardless of the screensaver choice).
  const bool screenOff =
      screensaverMode_ == ScreensaverMode::ScreenOff || standbyScreenOffForced_;
  if (!screenOff && standbyScreenOffActive_) {
    display_.wakeFromSleep();
    standbyScreenOffActive_ = false;
  }

  if (screenOff) {
    screensaver_.reset();
    seedStandbyScreenOff(nowMs);
    return;
  }

  standby::Kind kind = standby::Kind::Life;
  switch (screensaverMode_) {
    case ScreensaverMode::Maze:
      kind = standby::Kind::Maze;
      break;
    case ScreensaverMode::Voronoi:
      kind = standby::Kind::Voronoi;
      break;
    case ScreensaverMode::Life:
    default:
      kind = standby::Kind::Life;
      break;
  }
  screensaver_ = standby::makeScreensaver(kind, kStandbyLifeColumns, kStandbyLifeRows);
  screensaver_->seed(standbyRngSeed(nowMs));
}

void App::stepStandbyScreensaver(uint32_t nowMs) {
  (void)nowMs;
  if (screensaver_) {
    screensaver_->step();
  }
}

void App::seedStandbyScreenOff(uint32_t nowMs) {
  (void)nowMs;
  screensaver_.reset();
  standbyScreenOffActive_ = true;
  display_.prepareForSleep();
}

void App::updateStandbyScreensaver(uint32_t nowMs, bool force) {
  if (state_ != AppState::Standby) {
    return;
  }

  if (screensaverMode_ == ScreensaverMode::ScreenOff || standbyScreenOffForced_) {
    if (!standbyScreenOffActive_) {
      seedStandbyScreenOff(nowMs);
    }
    lastStandbyFrameMs_ = nowMs;
    return;
  }

  if (!force && nowMs - lastStandbyFrameMs_ < kStandbyFrameMs) {
    return;
  }

  if (!force) {
    stepStandbyScreensaver(nowMs);
  } else if (!screensaver_) {
    seedStandbyScreensaver(nowMs);
  }

  lastStandbyFrameMs_ = nowMs;
  if (screensaver_) {
    const standby::Frame frame = screensaver_->frame();
    display_.renderLifeScreensaver(*frame.cells, kStandbyLifeColumns, kStandbyLifeRows,
                                   frame.generation, frame.dimCells);
  }
}

void App::enterPowerOff(uint32_t nowMs) {
  if (powerOffStarted_) {
    return;
  }

  powerOffStarted_ = true;
  Serial.println("[app] powering off; hold PWR to start again");
  saveReadingPosition(true);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  touchPlayHeld_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  state_ = AppState::Sleeping;

  display_.renderStatus("OFF", "Release PWR", "Hold PWR to start");
  delay(300);
  display_.prepareForSleep();

  activeBookStore_.close();
  storage_.end();
  touch_.end();
  touchInitialized_ = false;
  Serial.flush();

  BoardConfig::holdBacklightOffForDeepSleep();
  BoardConfig::releaseBatteryPowerHold();

  const uint32_t waitStartMs = millis();
  while (powerButton_.isHeld() && millis() - waitStartMs < kPowerOffReleaseWaitMs) {
    powerButton_.update(millis());
    delay(10);
  }

  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BoardConfig::PIN_PWR_BUTTON), 0);
  esp_deep_sleep_start();
}

void App::enterSleep(uint32_t nowMs) {
  Serial.println("[app] entering light sleep; press BOOT to wake");
  saveReadingPosition(true);
  setState(AppState::Sleeping, nowMs);
  Serial.flush();
  delay(200);

  display_.prepareForSleep();
  activeBookStore_.close();
  storage_.end();
  touch_.end();
  touchInitialized_ = false;

  BoardConfig::lightSleepUntilBootButton();
  wakeFromSleep();
}

void App::wakeFromSleep() {
  const uint32_t nowMs = millis();
  Serial.println("[app] woke from light sleep");

  BoardConfig::begin();
  button_.begin();
  powerButton_.begin();
  bootButtonReleasedSinceBoot_ = !button_.isHeld();
  bootButtonLongPressHandled_ = false;
  powerButtonReleasedSinceBoot_ = !powerButton_.isHeld();
  powerButtonLongPressHandled_ = false;
  powerOffStarted_ = false;
  updateBatteryStatus(nowMs, true);
  storage_.setStatusCallback(&App::handleStorageStatus, this);
  pausedTouch_.active = false;
  pausedTouchIntent_ = TouchIntent::None;
  wpmFeedbackVisible_ = false;
  menuScreen_ = MenuScreen::Main;
  lastStateLogMs_ = nowMs;
  // Waking is activity: never inherit a pre-sleep idle countdown.
  standbyDecider_.noteActivity(nowMs);
  state_ = AppState::Paused;

  const bool displayReady = display_.wakeFromSleep();
  touchInitialized_ = touch_.begin();
  storageReady_ = storage_.begin();

  if (storageReady_ && usingStorageBook_ && !currentBookPath_.isEmpty()) {
    const size_t resumeIndex = reader_.currentIndex();
    const int refreshedBookIndex = findBookIndexByPath(currentBookPath_);
    if (refreshedBookIndex >= 0 &&
        loadBookAtIndex(static_cast<size_t>(refreshedBookIndex), nowMs, false, false, false,
                        false)) {
      reader_.seekTo(resumeIndex);
    } else {
      Serial.println("[app] current indexed book unavailable after wake");
      usingStorageBook_ = false;
      currentBookPath_ = "";
      currentBookTitle_ = "Demo";
      reader_.clearLoadedBook(nowMs);
      reader_.begin(nowMs);
    }
  }

  if (displayReady) {
    renderActiveReader(nowMs);
  }
}

bool App::restoreSavedBook(uint32_t nowMs) {
  const String savedPath = preferences_.getString(kPrefBookPath, "");
  if (savedPath.isEmpty()) {
    return false;
  }

  const int bookIndex = findBookIndexByPath(savedPath);
  if (bookIndex < 0) {
    Serial.printf("[app] saved book not found: %s\n", savedPath.c_str());
    return false;
  }

  if (!loadBookAtIndex(static_cast<size_t>(bookIndex), nowMs, true, false, false, false)) {
    return false;
  }

  Serial.printf("[app] restored %s at word %u\n", savedPath.c_str(),
                static_cast<unsigned int>(reader_.currentIndex()));
  return true;
}

bool App::prepareBootBookLoad() {
  pendingBootBookIndex_ = 0;
  pendingBootBookLegacyFallback_ = false;

  if (!storageReady_ || storage_.bookCount() == 0) {
    return false;
  }

  const String savedPath = preferences_.getString(kPrefBookPath, "");
  if (!savedPath.isEmpty()) {
    const int savedBookIndex = findBookIndexByPath(savedPath);
    if (savedBookIndex >= 0) {
      pendingBootBookIndex_ = static_cast<size_t>(savedBookIndex);
      pendingBootBookLegacyFallback_ = true;
      Serial.printf("[app] deferred saved book load: %s\n", savedPath.c_str());
      return true;
    }

    Serial.printf("[app] saved book not found: %s\n", savedPath.c_str());
  }

  pendingBootBookIndex_ = 0;
  pendingBootBookLegacyFallback_ = false;
  Serial.println("[app] deferred first book load");
  return true;
}

void App::loadPendingBootBook(uint32_t nowMs) {
  if (!pendingBootBookLoad_ || state_ != AppState::Paused) {
    return;
  }

  pendingBootBookLoad_ = false;
  display_.renderStatus("Loading book", currentBookTitle_, "Please wait");
  const uint32_t startedMs = millis();
  const bool allowIndexBuild = pendingBootBookLegacyFallback_;
  const bool loaded = loadBookAtIndex(pendingBootBookIndex_, nowMs,
                                      pendingBootBookLegacyFallback_, allowIndexBuild, false,
                                      false);
  const uint32_t elapsedMs = millis() - startedMs;
  Serial.printf("[app] deferred book load %s in %lu ms\n", loaded ? "ok" : "failed",
                static_cast<unsigned long>(elapsedMs));

  if (loaded) {
    usingStorageBook_ = true;
    renderActiveReader(millis());
    return;
  }

  usingStorageBook_ = false;
  chapterMarkers_.clear();
  paragraphStarts_.clear();
  currentBookPath_ = "";
  currentBookTitle_ = "Demo";
  reader_.begin(millis());
  invalidateContextPreviewWindow();
  Serial.println("[app] using built-in demo text");
  renderActiveReader(millis());
}

void App::saveReadingPosition(bool force) {
  if (!usingStorageBook_ || currentBookPath_.isEmpty()) {
    return;
  }

  const size_t wordIndex = reader_.currentIndex();
  if (!force && wordIndex == lastSavedWordIndex_) {
    return;
  }

  preferences_.putString(kPrefBookPath, currentBookPath_);
  bookProgress_.savePosition(currentBookPath_, static_cast<uint32_t>(wordIndex),
                             static_cast<uint32_t>(reader_.wordCount()));
  preferences_.putUShort(kPrefWpm, reader_.wpm());
  bookProgress_.markRecent(currentBookPath_);
  // Optional auto-finish: when the reader reaches the end of the book, set the
  // finished flag. This never clears it on scrub-back -- explicit unmark only.
  if (reader_.atEnd() && !bookProgress_.isFinished(currentBookPath_)) {
    bookProgress_.setFinished(currentBookPath_, true);
    Serial.printf("[app] auto-marked finished book=%s\n", currentBookPath_.c_str());
  }
  lastSavedWordIndex_ = wordIndex;
  Serial.printf("[app] saved position word=%u book=%s\n", static_cast<unsigned int>(wordIndex),
                currentBookPath_.c_str());
}

bool App::loadBookAtIndex(size_t index, uint32_t nowMs, bool allowLegacyPositionFallback,
                          bool allowIndexBuild, bool allowEpubConversion,
                          bool rebuildTimeEstimate) {
  BookMetadata book;
  String loadedPath;
  size_t loadedIndex = index;
  const String initialLabel = storage_.bookDisplayName(index);
  renderStorageStatus("Opening book", initialLabel.c_str(),
                      allowIndexBuild ? "Checking index" : "Checking saved index", 5);
  if (!storage_.loadIndexedBook(index, activeBookStore_, book, &loadedPath, &loadedIndex,
                                allowIndexBuild, allowEpubConversion)) {
    return false;
  }

  const String loadedTitle = book.title.isEmpty() ? displayNameForPath(loadedPath) : book.title;
  renderStorageStatus("Opening book", loadedTitle.c_str(), "Loading word cache", 70);

  const bool keepingExistingTimeCache =
      !rebuildTimeEstimate && timeEstimateCacheValid_ && currentBookPath_ == loadedPath;
  reader_.setWordSource(&activeBookStore_, nowMs);
  if (reader_.wordCount() == 0 || reader_.currentWord().isEmpty()) {
    Serial.printf("[app] failed to read first indexed word from %s\n", loadedPath.c_str());
    activeBookStore_.close();
    reader_.clearLoadedBook(nowMs);
    renderStorageStatus("Book open failed", loadedTitle.c_str(), "Word cache unreadable", 100);
    return false;
  }

  chapterMarkers_ = std::move(book.chapters);
  paragraphStarts_ = std::move(book.paragraphStarts);
  invalidateContextPreviewWindow();
  currentBookIndex_ = loadedIndex;
  currentBookPath_ = loadedPath;
  currentBookTitle_ = loadedTitle;
  lastSavedWordIndex_ = static_cast<size_t>(-1);
  usingStorageBook_ = true;
  preferences_.putString(kPrefBookPath, currentBookPath_);
  bookProgress_.saveWordCount(currentBookPath_, static_cast<uint32_t>(reader_.wordCount()));
  bookProgress_.markRecent(currentBookPath_);

  const uint32_t savedWordIndex =
      bookProgress_.readPosition(currentBookPath_, allowLegacyPositionFallback);
  if (savedWordIndex != bookprogress::kNoSavedWordIndex) {
    renderStorageStatus("Opening book", currentBookTitle_.c_str(), "Restoring position", 78);
    reader_.seekTo(savedWordIndex);
    lastSavedWordIndex_ = reader_.currentIndex();
    Serial.printf("[app] restored book position word=%u key=%s\n",
                  static_cast<unsigned int>(reader_.currentIndex()),
                  bookprogress::positionKey(currentBookPath_).c_str());
  }

  if (rebuildTimeEstimate) {
    rebuildTimeEstimateCache();
  } else if (!keepingExistingTimeCache) {
    invalidateTimeEstimateCache();
  } else {
    renderStorageStatus("Opening book", currentBookTitle_.c_str(), "Using cached estimate", 92);
  }

  lastProgressSaveMs_ = nowMs;
  Serial.printf("[app] loaded SD book[%u/%u]: %s (%u chapters, %u paragraphs)\n",
                static_cast<unsigned int>(loadedIndex + 1),
                static_cast<unsigned int>(storage_.bookCount()), loadedPath.c_str(),
                static_cast<unsigned int>(chapterMarkers_.size()),
                static_cast<unsigned int>(paragraphStarts_.size()));
  return true;
}

bool App::bookProgressPercent(size_t bookIndex, uint8_t &percent) {
  if (usingStorageBook_ && bookIndex == currentBookIndex_) {
    return bookprogress::progressPercent(static_cast<uint32_t>(reader_.currentIndex()),
                                         static_cast<uint32_t>(reader_.wordCount()), percent);
  }
  return bookProgress_.progressPercent(storage_.bookPath(bookIndex), percent);
}

int App::findBookIndexByPath(const String &path) const {
  for (size_t i = 0; i < storage_.bookCount(); ++i) {
    if (storage_.bookPath(i) == path) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void App::renderMenu() {
  if (!isFocusTimerMenuScreen(menuScreen_)) {
    applyReaderUiOrientation();
  }

  if (menu::isSettingsScreen(menuScreen_)) {
    renderSettings();
  } else if (menuScreen_ == MenuScreen::WifiNetworks) {
    renderWifiNetworks();
  } else if (menuScreen_ == MenuScreen::TextEntry) {
    renderTextEntry();
  } else if (menuScreen_ == MenuScreen::TypographyTuning) {
    renderTypographyTuning();
  } else if (menuScreen_ == MenuScreen::BookPicker) {
    renderBookPicker();
  } else if (menuScreen_ == MenuScreen::ChapterPicker) {
    renderChapterPicker();
  } else if (menuScreen_ == MenuScreen::BookmarkPicker) {
    renderBookmarkPicker();
  } else if (menuScreen_ == MenuScreen::StarredPicker) {
    renderStarredPicker();
  } else if (menuScreen_ == MenuScreen::RestartConfirm) {
    renderRestartConfirm();
  } else if (menuScreen_ == MenuScreen::SdCardRepairConfirm) {
    renderSdCardRepairConfirm();
  } else if (menuScreen_ == MenuScreen::UpdateConfirm) {
    renderUpdateConfirm();
  } else if (menuScreen_ == MenuScreen::FocusTimerGenres) {
    renderFocusTimerGenres();
  } else if (menuScreen_ == MenuScreen::FocusTimerSession) {
    renderFocusTimerSession();
  } else {
    renderMainMenu();
  }
}

void App::renderMainMenu() {
  std::vector<String> items;
  items.reserve(MenuItemCount);
  items.push_back(uiText(UiText::Resume));
  items.push_back(uiText(UiText::Chapters));
  const bool finished =
      usingStorageBook_ && !currentBookPath_.isEmpty() && bookProgress_.isFinished(currentBookPath_);
  items.push_back(finished ? "Mark unread" : "Mark finished");
  items.push_back("Bookmarks");
  items.push_back("Starred");
  items.push_back("Reading stats");
  items.push_back("Books");
  items.push_back("Articles");
  items.push_back("Focus Timer");
  items.push_back(uiText(UiText::Settings));
  items.push_back("SD card check");
  items.push_back("RSS feeds");
  items.push_back("Companion sync");
#if RSVP_USB_TRANSFER_ENABLED
  items.push_back(uiText(UiText::UsbTransfer));
#endif
  items.push_back(uiText(UiText::PowerOff));
  display_.renderMenu(items, menuSelectedIndex_);
}

void App::renderSettings() {
  if (settingsMenuItems_.empty()) {
    rebuildSettingsMenuItems();
  }
  display_.renderMenu(settingsMenuItems_, settingsSelectedIndex_);
}

void App::renderTypographyTuning() {
  if (kTypographyPreviewWordCount == 0) {
    display_.renderStatus(uiText(UiText::Typography), uiText(UiText::NoSamples), "");
    return;
  }

  if (typographyPreviewSampleIndex_ >= kTypographyPreviewWordCount) {
    typographyPreviewSampleIndex_ = 0;
  }
  if (typographyTuningSelectedIndex_ >= TypographyTuningItemCount) {
    typographyTuningSelectedIndex_ = TypographyTuningFontSize;
  }

  const size_t index = typographyPreviewSampleIndex_;
  const size_t beforeIndex =
      index == 0 ? kTypographyPreviewWordCount - 1 : index - 1;
  const size_t afterIndex =
      (index + 1 >= kTypographyPreviewWordCount) ? 0 : index + 1;
  const String beforeText = phantomWordsEnabled_ ? kTypographyPreviewWords[beforeIndex] : "";
  const String afterText = phantomWordsEnabled_ ? kTypographyPreviewWords[afterIndex] : "";
  const String line1 = typographyTuningLabel() + ": " + typographyTuningValueLabel();
  const String title =
      uiText(UiText::Typography) + " " + String(static_cast<unsigned int>(index + 1)) + "/" +
      String(static_cast<unsigned int>(kTypographyPreviewWordCount));
  String line2 = uiText(UiText::TapChangeSample);
  if (typographyTuningSelectedIndex_ == TypographyTuningBack) {
    line2 = uiText(UiText::TapExitSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningPhantomWords ||
             typographyTuningSelectedIndex_ == TypographyTuningFocusHighlight) {
    line2 = uiText(UiText::TapToggleSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningFontSize ||
             typographyTuningSelectedIndex_ == TypographyTuningTypeface) {
    line2 = uiText(UiText::TapCycleSample);
  } else if (typographyTuningSelectedIndex_ == TypographyTuningReset) {
    line2 = uiText(UiText::TapToReset);
  }

  display_.renderTypographyPreview(beforeText,
                                   kTypographyPreviewWords[index],
                                   afterText,
                                   readerFontSizeIndex_, title, line1, line2);
}

void App::renderBookPicker() {
  display_.renderLibrary(bookMenuItems_, bookPickerSelectedIndex_);
}

void App::renderChapterPicker() {
  display_.renderMenu(chapterMenuItems_, chapterPickerSelectedIndex_);
}

void App::renderRestartConfirm() {
  std::vector<String> items;
  items.reserve(RestartConfirmItemCount);
  items.push_back(uiText(UiText::AreYouSure));
  items.push_back(uiText(UiText::NoKeepPlace));
  items.push_back(uiText(UiText::YesRestart));

  display_.renderMenu(items, restartConfirmSelectedIndex_ + kRestartConfirmHeaderRows);
}

void App::renderSdCardRepairConfirm() {
  std::vector<String> items;
  items.reserve(SdCardRepairConfirmItemCount + kSdCardRepairConfirmHeaderRows);
  items.push_back("Repair folders?");
  items.push_back("Not now");
  items.push_back("Create folders");

  display_.renderMenu(items, sdCardRepairConfirmSelectedIndex_ + kSdCardRepairConfirmHeaderRows);
}

void App::renderUpdateConfirm() {
  std::vector<String> items;
  items.reserve(UpdateConfirmItemCount + kUpdateConfirmHeaderRows);
  items.push_back("Update available");
  items.push_back(pendingUpdateCurrentVersion_ + " -> " + pendingUpdateNewVersion_);
  items.push_back("Skip for now");
  items.push_back("Update");

  display_.renderMenu(items, updateConfirmSelectedIndex_ + kUpdateConfirmHeaderRows);
}

void App::renderFocusTimerGenres() {
  applyReaderUiOrientation();
  if (focusTimerGenreMenuItems_.empty()) {
    rebuildFocusTimerGenreMenuItems();
  }
  display_.renderMenu(focusTimerGenreMenuItems_, focusTimerGenreSelectedIndex_);
}

void App::renderFocusTimerSession() {
  applyUiOrientation(focusTimer_.uiOrientation());
  const String remainingLabel = formatFocusTimerRemaining(millis());

  switch (focusTimer_.state()) {
    case FocusTimer::State::Unavailable:
      display_.renderFocusTimerScreen("TIMER", "", "", "IMU unavailable");
      return;
    case FocusTimer::State::GenreSelect:
      renderFocusTimerGenres();
      return;
    case FocusTimer::State::WaitForTouchStart:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Place on short side");
      return;
    case FocusTimer::State::TouchRunning:
      display_.renderFocusTimerScreen("BEGIN", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()));
      return;
    case FocusTimer::State::WaitAfterTouch:
      display_.renderFocusTimerScreen("WORK", "", "", "Flip to continue");
      return;
    case FocusTimer::State::WorkRunning:
      display_.renderFocusTimerScreen("WORK", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()));
      return;
    case FocusTimer::State::BreakRunning:
      display_.renderFocusTimerScreen("BREAK", "", remainingLabel, "",
                                      "", focusTimer_.progressPercent(millis()), true);
      return;
    case FocusTimer::State::WaitAfterWork:
      display_.renderFocusTimerScreen("BREAK", "", "", "Turn for break", "",
                                      -1, true);
      return;
    case FocusTimer::State::WaitAfterBreak:
      display_.renderFocusTimerScreen("WORK", "", "", "Flip to begin");
      return;
    case FocusTimer::State::Cancelled:
      display_.renderFocusTimerScreen("BEGIN", "", "", "Place to begin again");
      return;
    case FocusTimer::State::Complete:
      display_.renderFocusTimerScreen("DONE", "", "", "Session complete");
      return;
  }
}

bool App::updateChapterTransition(uint32_t nowMs) {
  if (!chapterTransitionVisible_) {
    return false;
  }

  if (nowMs < chapterTransitionUntilMs_) {
    return true;
  }

  chapterTransitionVisible_ = false;
  reader_.start(nowMs);
  renderActiveReader(nowMs);
  return true;
}

bool App::maybeStartChapterTransition(size_t previousWordIndex, size_t currentWordIndex,
                                      uint32_t nowMs) {
  if (chapterMarkers_.empty() || currentWordIndex <= previousWordIndex) {
    return false;
  }

  for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
    const size_t chapterWordIndex = chapterMarkers_[i].wordIndex;
    if (chapterWordIndex == 0 || chapterWordIndex <= previousWordIndex ||
        chapterWordIndex > currentWordIndex) {
      continue;
    }

    chapterTransitionIndex_ = i;
    chapterTransitionVisible_ = true;
    chapterTransitionUntilMs_ = nowMs + kChapterTransitionMs;
    contextViewVisible_ = false;
    wpmFeedbackVisible_ = false;
    reader_.seekTo(chapterWordIndex);
    renderChapterTransition();
    Serial.printf("[chapter] transition %u/%u word=%u title=%s\n",
                  static_cast<unsigned int>(i + 1),
                  static_cast<unsigned int>(chapterMarkers_.size()),
                  static_cast<unsigned int>(chapterWordIndex),
                  chapterMarkers_[i].title.c_str());
    return true;
  }

  return false;
}

void App::renderChapterTransition() {
  if (!chapterTransitionVisible_ || chapterTransitionIndex_ >= chapterMarkers_.size()) {
    return;
  }

  applyReaderUiOrientation();
  const String title = String("CHAPTER ") + String(chapterTransitionIndex_ + 1);
  String subtitle = chapterMarkers_[chapterTransitionIndex_].title;
  if (subtitle.length() > 42) {
    subtitle = subtitle.substring(0, 42) + "...";
  }
  display_.renderStatus(title, subtitle, "");
}

DisplayManager::LibraryItem App::libraryItemForBook(size_t bookIndex) {
  DisplayManager::LibraryItem item;
  item.title = storage_.bookDisplayName(bookIndex);
  item.subtitle = storage_.bookAuthorName(bookIndex);
  item.finished = bookProgress_.isFinished(storage_.bookPath(bookIndex));

  uint8_t percent = 0;
  const bool hasProgress = bookProgressPercent(bookIndex, percent);
  if (hasProgress) {
    if (!item.subtitle.isEmpty()) {
      item.subtitle += " - ";
    }
    item.subtitle += String(percent) + "%";
  }

  if (item.subtitle.isEmpty() && usingStorageBook_ && bookIndex == currentBookIndex_) {
    item.subtitle = uiText(UiText::CurrentBook);
  }

  return item;
}

String App::chapterMenuLabel(size_t chapterIndex) const {
  if (chapterIndex >= chapterMarkers_.size()) {
    return "";
  }

  String label = String(chapterIndex + 1) + " " + chapterMarkers_[chapterIndex].title;
  if (label.length() > 36) {
    label = label.substring(0, 36) + "...";
  }

  const size_t currentIndex = reader_.currentIndex();
  const size_t startIndex = chapterMarkers_[chapterIndex].wordIndex;
  const size_t endIndex = (chapterIndex + 1 < chapterMarkers_.size())
                              ? chapterMarkers_[chapterIndex + 1].wordIndex
                              : reader_.wordCount();
  if (currentIndex >= startIndex && currentIndex < endIndex) {
    label += " *";
  }
  return label;
}

size_t App::currentChapterIndex() const {
  if (chapterMarkers_.empty()) {
    return static_cast<size_t>(-1);
  }

  size_t currentChapter = 0;
  const size_t currentIndex = reader_.currentIndex();
  for (size_t i = 0; i < chapterMarkers_.size(); ++i) {
    if (chapterMarkers_[i].wordIndex <= currentIndex) {
      currentChapter = i;
    }
  }

  return currentChapter;
}

String App::currentChapterLabel() const {
  const size_t chapterIndex = currentChapterIndex();
  if (chapterIndex >= chapterMarkers_.size()) {
    return currentBookTitle_.isEmpty() ? uiText(UiText::Start) : currentBookTitle_;
  }

  return chapterMarkers_[chapterIndex].title;
}

String App::currentFooterMetricLabel() const {
  if (footerMetricMode_ == FooterMetricMode::Percentage) {
    return String(readingProgressPercent()) + "%";
  }

  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "0%";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  size_t endIndex = wordCount;
  const bool generatingEstimate = accurateTimeEstimateEnabled_ && timeEstimateBuildInProgress_ &&
                                  timeEstimateBuildMatchesCurrentBook();
  const int generatingPercent =
      generatingEstimate
          ? static_cast<int>((timeEstimateBuildNextBlock_ * 100UL) /
                             std::max<size_t>(1, timeEstimateBuildBlockCount_))
          : 0;

  if (footerMetricMode_ == FooterMetricMode::ChapterTime) {
    const size_t chapterIndex = currentChapterIndex();
    if (chapterIndex < chapterMarkers_.size() && chapterIndex + 1 < chapterMarkers_.size()) {
      endIndex = chapterMarkers_[chapterIndex + 1].wordIndex;
    }
    if (generatingEstimate) {
      return String("CH ") + String(generatingPercent) + "% gen";
    }
    return String("CH ") +
           formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
  }

  if (generatingEstimate) {
    return String("BOOK ") + String(generatingPercent) + "% gen";
  }
  return String("BOOK ") +
         formatReadingTimeRemaining(estimatedReadingTimeRemainingMs(currentIndex, endIndex));
}

String App::currentBatteryLabel() const {
  if (!batteryMonitor_.present() || !batteryMonitor_.initialized()) {
    return "";
  }

  if (batteryLabelMode_ == BatteryLabelMode::TimeRemaining) {
    return batteryTimeRemainingLabel();
  }

  if (batteryLabelMode_ == BatteryLabelMode::Voltage) {
    return batteryVoltageLabel();
  }

  return String(static_cast<unsigned int>(batteryMonitor_.displayedPercent())) + "%";
}

String App::footerMetricModeLabel() const {
  switch (footerMetricMode_) {
    case FooterMetricMode::ChapterTime:
      return "Chapter time";
    case FooterMetricMode::BookTime:
      return "Book time";
    case FooterMetricMode::Percentage:
    default:
      return "Percent read";
  }
}

String App::batteryLabelModeLabel() const {
  switch (batteryLabelMode_) {
    case BatteryLabelMode::TimeRemaining:
      return "Time remaining";
    case BatteryLabelMode::Voltage:
      return "Voltage";
    case BatteryLabelMode::Percent:
    default:
      return "Percentage";
  }
}

String App::screensaverModeLabel() const {
  switch (screensaverMode_) {
    case ScreensaverMode::Maze:
      return "Maze";
    case ScreensaverMode::Voronoi:
      return "Voronoi";
    case ScreensaverMode::ScreenOff:
      return "Screen off";
    case ScreensaverMode::Life:
    default:
      return "Life";
  }
}

String App::batteryTimeRemainingLabel() const {
  if (batteryMonitor_.runtimeEstimateReady()) {
    return formatBatteryTimeRemaining(batteryMonitor_.runtimeMinutesRemaining());
  }

  const uint32_t estimatedMinutes =
      (static_cast<uint32_t>(batteryMonitor_.displayedPercent()) * kNominalBatteryRuntimeMinutes) /
      100UL;
  return formatBatteryTimeRemaining(estimatedMinutes);
}

String App::batteryVoltageLabel() const {
  return String(batteryMonitor_.filteredVoltage(), 2) + "V";
}

String App::formatBatteryTimeRemaining(uint32_t minutes) const {
  if (minutes < 1) {
    return "0m";
  }

  if (minutes < 60) {
    return String(minutes) + "m";
  }

  const uint32_t hours = minutes / 60;
  const uint32_t remainder = minutes % 60;
  if (hours >= 10 || remainder < 10) {
    return String(hours) + "h";
  }

  return String(hours) + "h" + String(remainder / 10) + "0";
}

uint32_t App::estimatedReadingTimeRemainingMs(size_t startIndex, size_t endIndex) const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0 || reader_.wpm() == 0) {
    return 0;
  }

  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  const uint32_t baseMs = static_cast<uint32_t>(
      (static_cast<uint64_t>(endIndex - startIndex) * 60000ULL) /
      static_cast<uint64_t>(reader_.wpm()));

  if (!accurateTimeEstimateEnabled_ || !timeEstimateCacheValid_) {
    return baseMs;
  }

  return baseMs + estimatedPacingBonusMs(startIndex, endIndex);
}

uint32_t App::estimatedPacingBonusMs(size_t startIndex, size_t endIndex) const {
  if (!timeEstimateCacheValid_ || wordBonusBlockPrefixSumMs_.empty() ||
      endIndex <= startIndex) {
    return 0;
  }

  const size_t wordCount = reader_.wordCount();
  startIndex = std::min(startIndex, wordCount);
  endIndex = std::min(endIndex, wordCount);
  if (endIndex <= startIndex) {
    return 0;
  }

  const size_t firstFullBlock = (startIndex + kTimeEstimateBlockWords - 1) /
                                kTimeEstimateBlockWords;
  const size_t lastFullBlockEnd = endIndex / kTimeEstimateBlockWords;
  uint32_t bonusMs = 0;

  if (firstFullBlock < lastFullBlockEnd &&
      lastFullBlockEnd < wordBonusBlockPrefixSumMs_.size()) {
    const size_t startPartialEnd =
        std::min(endIndex, firstFullBlock * kTimeEstimateBlockWords);
    for (size_t i = startIndex; i < startPartialEnd; ++i) {
      bonusMs += reader_.wordPacingBonusMsAt(i);
    }

    bonusMs += wordBonusBlockPrefixSumMs_[lastFullBlockEnd] -
               wordBonusBlockPrefixSumMs_[firstFullBlock];

    const size_t endPartialStart = lastFullBlockEnd * kTimeEstimateBlockWords;
    for (size_t i = endPartialStart; i < endIndex; ++i) {
      bonusMs += reader_.wordPacingBonusMsAt(i);
    }
    return bonusMs;
  }

  for (size_t i = startIndex; i < endIndex; ++i) {
    bonusMs += reader_.wordPacingBonusMsAt(i);
  }
  return bonusMs;
}

void App::invalidateTimeEstimateCache() {
  cancelTimeEstimateBuild();
  timeEstimateCacheValid_ = false;
  std::vector<uint32_t>().swap(wordBonusBlockPrefixSumMs_);
}

void App::rebuildTimeEstimateCache() {
  invalidateTimeEstimateCache();
  pacingCacheDirty_ = false;
  if (!accurateTimeEstimateEnabled_) {
    if (!currentBookTitle_.isEmpty()) {
      renderStorageStatus("Reading time", currentBookTitle_.c_str(), "Fast estimate enabled",
                          100);
    }
    return;
  }

  const size_t n = reader_.wordCount();
  if (n == 0) {
    return;
  }

  const String label = currentBookTitle_.isEmpty() ? String("Current book") : currentBookTitle_;
  timeEstimateBuildWordCount_ = n;
  timeEstimateBuildBlockCount_ =
      (timeEstimateBuildWordCount_ + kTimeEstimateBlockWords - 1) / kTimeEstimateBlockWords;
  if (timeEstimateBuildBlockCount_ == 0) {
    return;
  }

  wordBonusBlockPrefixSumMs_.assign(timeEstimateBuildBlockCount_ + 1, 0);
  timeEstimateBuildBookPath_ = currentBookPath_;
  timeEstimateBuildNextBlock_ = 0;
  timeEstimateBuildRunningMs_ = 0;
  timeEstimateBuildStartedMs_ = millis();
  timeEstimateBuildLastLogMs_ = timeEstimateBuildStartedMs_;
  timeEstimateBuildInProgress_ = true;

  const String detail = String(static_cast<unsigned int>(n)) + " words in background";
  renderStorageStatus("Reading time", label.c_str(), detail.c_str(), 0);
  Serial.printf("[time-est] background build started words=%u blocks=%u book=%s\n",
                static_cast<unsigned int>(timeEstimateBuildWordCount_),
                static_cast<unsigned int>(timeEstimateBuildBlockCount_),
                currentBookPath_.c_str());
}

void App::cancelTimeEstimateBuild() {
  timeEstimateBuildInProgress_ = false;
  timeEstimateBuildBookPath_ = "";
  timeEstimateBuildWordCount_ = 0;
  timeEstimateBuildBlockCount_ = 0;
  timeEstimateBuildNextBlock_ = 0;
  timeEstimateBuildRunningMs_ = 0;
  timeEstimateBuildStartedMs_ = 0;
  timeEstimateBuildLastLogMs_ = 0;
}

bool App::timeEstimateBuildMatchesCurrentBook() const {
  return timeEstimateBuildInProgress_ && timeEstimateBuildBookPath_ == currentBookPath_ &&
         timeEstimateBuildWordCount_ == reader_.wordCount();
}

void App::updateTimeEstimateBuild(uint32_t nowMs) {
  if (!timeEstimateBuildInProgress_) {
    return;
  }

  if (!accurateTimeEstimateEnabled_ || !timeEstimateBuildMatchesCurrentBook()) {
    Serial.println("[time-est] background build cancelled");
    invalidateTimeEstimateCache();
    return;
  }

  if (state_ == AppState::Playing || state_ == AppState::CompanionSync ||
      state_ == AppState::UsbTransfer || state_ == AppState::Standby ||
      state_ == AppState::Sleeping) {
    return;
  }

  size_t processedBlocks = 0;
  while (timeEstimateBuildNextBlock_ < timeEstimateBuildBlockCount_ &&
         processedBlocks < kTimeEstimateBlocksPerUpdate) {
    const size_t block = timeEstimateBuildNextBlock_;
    wordBonusBlockPrefixSumMs_[block] = timeEstimateBuildRunningMs_;
    const size_t blockStart = block * kTimeEstimateBlockWords;
    const size_t blockEnd =
        std::min(timeEstimateBuildWordCount_, blockStart + kTimeEstimateBlockWords);
    for (size_t i = blockStart; i < blockEnd; ++i) {
      timeEstimateBuildRunningMs_ += reader_.wordPacingBonusMsAt(i);
    }
    ++timeEstimateBuildNextBlock_;
    ++processedBlocks;
    delay(0);
  }

  if (timeEstimateBuildNextBlock_ >= timeEstimateBuildBlockCount_) {
    wordBonusBlockPrefixSumMs_[timeEstimateBuildBlockCount_] = timeEstimateBuildRunningMs_;
    timeEstimateCacheValid_ = true;
    const uint32_t elapsedMs = millis() - timeEstimateBuildStartedMs_;
    Serial.printf("[time-est] background cached %u words in %u blocks bonus=%lums took=%lums\n",
                  static_cast<unsigned int>(timeEstimateBuildWordCount_),
                  static_cast<unsigned int>(timeEstimateBuildBlockCount_),
                  static_cast<unsigned long>(timeEstimateBuildRunningMs_),
                  static_cast<unsigned long>(elapsedMs));
    cancelTimeEstimateBuild();
    if (state_ == AppState::Paused || state_ == AppState::Playing) {
      renderActiveReader(nowMs);
    } else if (state_ == AppState::Menu) {
      renderMenu();
    }
    return;
  }

  if (nowMs - timeEstimateBuildLastLogMs_ >= kTimeEstimateProgressLogMs) {
    const int progress =
        static_cast<int>((timeEstimateBuildNextBlock_ * 100UL) /
                         std::max<size_t>(1, timeEstimateBuildBlockCount_));
    Serial.printf("[time-est] background progress %u/%u blocks (%d%%)\n",
                  static_cast<unsigned int>(timeEstimateBuildNextBlock_),
                  static_cast<unsigned int>(timeEstimateBuildBlockCount_), progress);
    timeEstimateBuildLastLogMs_ = nowMs;
    if (state_ == AppState::Paused) {
      renderActiveReader(nowMs);
    }
  }
}

String App::timeEstimateModeLabel() const {
  return uiText(accurateTimeEstimateEnabled_ ? UiText::TimeEstimateAccurate
                                             : UiText::TimeEstimateFast);
}

String App::formatReadingTimeRemaining(uint32_t remainingMs) const {
  const uint32_t totalSeconds = remainingMs / 1000UL;
  if (totalSeconds < 60UL) {
    return "0m";
  }

  const uint32_t totalMinutes = totalSeconds / 60UL;
  if (totalMinutes < 60UL) {
    return String(totalMinutes) + "m";
  }

  const uint32_t totalHours = totalMinutes / 60UL;
  const uint32_t minutes = totalMinutes % 60UL;
  if (totalHours < 24UL) {
    if (minutes == 0) {
      return String(totalHours) + "h";
    }
    return String(totalHours) + "h" + String(minutes) + "m";
  }

  const uint32_t days = totalHours / 24UL;
  const uint32_t hours = totalHours % 24UL;
  if (hours == 0) {
    return String(days) + "d";
  }
  return String(days) + "d" + String(hours) + "h";
}

uint8_t App::readingProgressPercent() const {
  const size_t count = reader_.wordCount();
  if (count <= 1) {
    return 0;
  }

  const size_t index = std::min(reader_.currentIndex(), count - 1);
  const size_t percent = (index * 100UL) / (count - 1);
  return static_cast<uint8_t>(std::min(static_cast<size_t>(100), percent));
}

bool App::isFocusTimerMenuScreen(MenuScreen screen) const {
  return menu::isFocusTimerScreen(screen);
}

void App::applyUiOrientation(BoardConfig::UiOrientation orientation) {
  touch_.setUiOrientation(orientation);
  display_.setUiOrientation(orientation);
}

void App::applyReaderUiOrientation() {
  applyUiOrientation(readerUiOrientation());
}

BoardConfig::UiOrientation App::readerUiOrientation() const {
  return uiRotated180() ? BoardConfig::UiOrientation::LandscapeFlipped
                        : BoardConfig::UiOrientation::Landscape;
}

String App::formatFocusTimerRemaining(uint32_t nowMs) const {
  const uint32_t remainingMs = focusTimer_.remainingMs(nowMs);
  const uint32_t totalSeconds = remainingMs / 1000UL;
  const uint32_t minutes = totalSeconds / 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
                static_cast<unsigned long>(minutes),
                static_cast<unsigned long>(seconds));
  return String(buffer);
}

String App::focusTimerCountsLabel() const {
  return "T" + String(focusTimer_.completedTouchBlocks()) + " W" +
         String(focusTimer_.completedWorkBlocks()) + " B" +
         String(focusTimer_.completedBreakBlocks());
}

void App::playFocusTimerCompletionCue() {
  if (audio_.beep()) {
    return;
  }

  for (int i = 0; i < 3; ++i) {
    digitalWrite(BoardConfig::PIN_LCD_BACKLIGHT, HIGH);
    delay(55);
    digitalWrite(BoardConfig::PIN_LCD_BACKLIGHT, LOW);
    delay(45);
  }
}

bool App::scrollModeEnabled() const { return readerMode_ == ReaderMode::Scroll; }

bool App::uiRotated180() const {
  return handednessMode_ == HandednessMode::Right ? BoardConfig::UI_ROTATED_180
                                                  : !BoardConfig::UI_ROTATED_180;
}

uint8_t App::effectiveAnchorPercent() const {
  return handednessMode_ == HandednessMode::Left
             ? static_cast<uint8_t>(typographyConfig_.anchorPercent + kLeftHandAnchorOffset)
             : typographyConfig_.anchorPercent;
}

DisplayManager::TypographyConfig App::effectiveTypographyConfig() const {
  DisplayManager::TypographyConfig config = typographyConfig_;
  config.anchorPercent = effectiveAnchorPercent();
  return config;
}

uint32_t App::currentReaderContentToken() const {
  return bookprogress::hashPath(currentBookPath_.isEmpty() ? String("__demo__") : currentBookPath_);
}

size_t App::phantomBeforeCharTarget() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }
  return kPhantomBeforeCharTargets[levelIndex];
}

size_t App::phantomAfterCharTarget() const {
  uint8_t levelIndex = readerFontSizeIndex_;
  if (levelIndex >= kReaderFontSizeCount) {
    levelIndex = 0;
  }
  return kPhantomAfterCharTargets[levelIndex];
}

String App::collectPhantomBeforeText(size_t currentIndex, size_t charTarget) const {
  if (currentIndex == 0 || charTarget == 0) {
    return "";
  }

  size_t startIndex = currentIndex;
  size_t totalChars = 0;
  while (startIndex > 0 && totalChars < charTarget) {
    --startIndex;
    const String word = reader_.wordAt(startIndex);
    totalChars += word.length();
    if (startIndex + 1 < currentIndex) {
      ++totalChars;
    }
  }

  String text;
  for (size_t index = startIndex; index < currentIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += reader_.wordAt(index);
  }
  return text;
}

String App::collectPhantomAfterText(size_t currentIndex, size_t charTarget) const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0 || currentIndex + 1 >= wordCount || charTarget == 0) {
    return "";
  }

  size_t endIndex = currentIndex + 1;
  size_t totalChars = 0;
  while (endIndex < wordCount && totalChars < charTarget) {
    const String word = reader_.wordAt(endIndex);
    totalChars += word.length();
    if (endIndex > currentIndex + 1) {
      ++totalChars;
    }
    ++endIndex;
  }

  String text;
  for (size_t index = currentIndex + 1; index < endIndex; ++index) {
    if (!text.isEmpty()) {
      text += ' ';
    }
    text += reader_.wordAt(index);
  }
  return text;
}

String App::phantomBeforeText() const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  return collectPhantomBeforeText(currentIndex, phantomBeforeCharTarget());
}

String App::phantomAfterText() const {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    return "";
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  return collectPhantomAfterText(currentIndex, phantomAfterCharTarget());
}

void App::renderActiveReader(uint32_t nowMs) {
  if (pendingBootBookLoad_) {
    display_.renderStatus("Loading book", currentBookTitle_, "Please wait");
    return;
  }

  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

  if (chapterTransitionVisible_) {
    renderChapterTransition();
    return;
  }

  applyReaderUiOrientation();
  if (scrollModeEnabled()) {
    if (wpmFeedbackVisible_) {
      renderScrollReader(nowMs, String(reader_.wpm()) + " WPM");
    } else {
      renderScrollReader(nowMs);
    }
    return;
  }

  if (contextViewVisible_) {
    renderContextPreview();
  } else if (wpmFeedbackVisible_) {
    renderWpmFeedback(nowMs);
  } else {
    renderReaderWord();
  }
}

bool App::ensureCurrentBookWordAvailable(uint32_t nowMs) {
  if (!usingStorageBook_ || reader_.wordCount() == 0 || !reader_.currentWord().isEmpty()) {
    return true;
  }

  handleCurrentBookReadFailure(nowMs, "Word cache unreadable");
  return false;
}

void App::handleCurrentBookReadFailure(uint32_t nowMs, const char *detail) {
  const String failedTitle = currentBookTitle_.isEmpty() ? String("Current book") : currentBookTitle_;
  const String failedPath = currentBookPath_;
  const bool articlesOnly =
      currentBookIndex_ < storage_.bookCount() && storage_.bookIsArticle(currentBookIndex_);

  Serial.printf("[app] active book read failed word=%u book=%s detail=%s\n",
                static_cast<unsigned int>(reader_.currentIndex()), failedPath.c_str(),
                detail == nullptr ? "" : detail);

  saveReadingPosition(true);
  activeBookStore_.close();
  reader_.clearLoadedBook(nowMs);
  chapterMarkers_.clear();
  paragraphStarts_.clear();
  currentBookPath_ = "";
  currentBookTitle_ = "Demo";
  usingStorageBook_ = false;
  contextViewVisible_ = false;
  wpmFeedbackVisible_ = false;
  invalidateContextPreviewWindow();
  invalidateTimeEstimateCache();

  setState(AppState::Menu, nowMs);
  display_.renderStatus("Book read failed", failedTitle,
                        detail == nullptr ? "Reopen from library" : detail);
  delay(1800);
  openBookPicker(articlesOnly);
}

void App::renderReaderWord() {
  applyReaderUiOrientation();
  contextViewVisible_ = false;
  const String beforeText = phantomWordsEnabled_ ? phantomBeforeText() : "";
  const String afterText = phantomWordsEnabled_ ? phantomAfterText() : "";
  const DisplayManager::ReaderChrome chrome = readerChrome();
  const bool showReaderFooter = readerFooterVisible();
  const String footerMetricLabel = readerFooterStatusLabel();
  display_.renderPhantomRsvpWord(beforeText, reader_.currentWord(), afterText,
                                 readerFontSizeIndex_, currentChapterLabel(),
                                 readingProgressPercent(), showReaderFooter, footerMetricLabel,
                                 chrome);
}

bool App::isParagraphStart(size_t wordIndex) const {
  if (wordIndex == 0) {
    return true;
  }

  return std::binary_search(paragraphStarts_.begin(), paragraphStarts_.end(), wordIndex);
}

size_t App::paragraphStartAtOrBefore(size_t wordIndex) const {
  if (wordIndex == 0 || paragraphStarts_.empty()) {
    return 0;
  }

  const auto it = std::upper_bound(paragraphStarts_.begin(), paragraphStarts_.end(), wordIndex);
  if (it == paragraphStarts_.begin()) {
    return 0;
  }

  return *std::prev(it);
}

size_t App::contextPreviewAnchorIndex(size_t currentIndex) const {
  if (currentIndex <= kContextPreviewAnchorLeadWords) {
    return 0;
  }

  const size_t anchorTarget = currentIndex - kContextPreviewAnchorLeadWords;
  const size_t paragraphStart = paragraphStartAtOrBefore(anchorTarget);
  if (anchorTarget - paragraphStart <= kContextPreviewMaxParagraphSnapWords) {
    return paragraphStart;
  }

  return anchorTarget;
}

void App::updateContextPreviewWindow(size_t currentIndex) {
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    contextPreviewWords_.clear();
    contextPreviewWindowValid_ = false;
    contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
    return;
  }

  size_t startIndex = contextPreviewStartIndex_;
  size_t endIndex = 0;
  bool rebuildWindow = !contextPreviewWindowValid_ || contextPreviewWords_.empty();
  if (!rebuildWindow) {
    endIndex = std::min(wordCount, startIndex + kContextPreviewWindowWords);
    rebuildWindow = currentIndex < startIndex || currentIndex >= endIndex ||
                    (currentIndex + 1 >= endIndex && endIndex < wordCount);
  }

  if (rebuildWindow) {
    startIndex = contextPreviewAnchorIndex(currentIndex);
    endIndex = std::min(wordCount, startIndex + kContextPreviewWindowWords);
    contextPreviewStartIndex_ = startIndex;
    contextPreviewWindowValid_ = true;
    contextPreviewWords_.clear();
    contextPreviewWords_.reserve(endIndex - startIndex);
    for (size_t index = startIndex; index < endIndex; ++index) {
      DisplayManager::ContextWord word;
      word.text = reader_.wordAt(index);
      word.paragraphStart = isParagraphStart(index);
      word.current = index == currentIndex;
      contextPreviewWords_.push_back(word);
    }
    contextPreviewCurrentLocalIndex_ =
        currentIndex >= startIndex ? currentIndex - startIndex : static_cast<size_t>(-1);
    return;
  }

  const size_t nextLocalIndex = currentIndex - startIndex;
  if (contextPreviewCurrentLocalIndex_ < contextPreviewWords_.size()) {
    contextPreviewWords_[contextPreviewCurrentLocalIndex_].current = false;
  }
  if (nextLocalIndex < contextPreviewWords_.size()) {
    contextPreviewWords_[nextLocalIndex].current = true;
    contextPreviewCurrentLocalIndex_ = nextLocalIndex;
  } else {
    contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
  }
}

void App::invalidateContextPreviewWindow() {
  contextPreviewWindowValid_ = false;
  contextPreviewWords_.clear();
  contextPreviewCurrentLocalIndex_ = static_cast<size_t>(-1);
}

void App::renderContextPreview() {
  applyReaderUiOrientation();
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  updateContextPreviewWindow(currentIndex);

  contextViewVisible_ = true;
  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, 0,
                            currentChapterLabel(), readingProgressPercent(), "",
                            readerFooterStatusLabel(), chrome);
}

void App::renderScrollReader(uint32_t nowMs, const String &overlayText) {
  applyReaderUiOrientation();
  contextViewVisible_ = false;
  const size_t wordCount = reader_.wordCount();
  if (wordCount == 0) {
    renderReaderWord();
    return;
  }

  const size_t currentIndex = std::min(reader_.currentIndex(), wordCount - 1);
  updateContextPreviewWindow(currentIndex);

  uint16_t scrollProgressPermille = 0;
  if (state_ == AppState::Playing && currentIndex + 1 < wordCount) {
    const uint32_t durationMs = reader_.currentWordDurationMs();
    if (durationMs > 0) {
      const uint32_t elapsedMs = reader_.elapsedInCurrentWordMs(nowMs);
      scrollProgressPermille = static_cast<uint16_t>(
          std::min<uint32_t>(1000UL, (elapsedMs * 1000UL) / durationMs));
    }
  }

  const DisplayManager::ReaderChrome chrome = readerChrome();
  display_.renderScrollView(contextPreviewWords_, currentReaderContentToken(),
                            contextPreviewStartIndex_, currentIndex, scrollProgressPermille,
                            currentChapterLabel(), readingProgressPercent(), overlayText,
                            readerFooterStatusLabel(), chrome);
}

void App::renderWpmFeedback(uint32_t nowMs) {
  if (!ensureCurrentBookWordAvailable(nowMs)) {
    return;
  }

  applyReaderUiOrientation();
  wpmFeedbackVisible_ = true;
  wpmFeedbackUntilMs_ = nowMs + kWpmFeedbackMs;
  if (scrollModeEnabled()) {
    renderScrollReader(nowMs, String(reader_.wpm()) + " WPM");
    return;
  }

  contextViewVisible_ = false;
  const String beforeText = phantomWordsEnabled_ ? phantomBeforeText() : "";
  const String afterText = phantomWordsEnabled_ ? phantomAfterText() : "";
  const DisplayManager::ReaderChrome chrome = readerChrome();
  const String footerMetricLabel = readerFooterStatusLabel();
  display_.renderPhantomRsvpWordWithWpm(beforeText, reader_.currentWord(), afterText,
                                        readerFontSizeIndex_, reader_.wpm(),
                                        currentChapterLabel(), readingProgressPercent(),
                                        readerFooterVisible(), footerMetricLabel, chrome);
}

void App::renderStorageStatus(const char *title, const char *line1, const char *line2,
                              int progressPercent) {
  applyReaderUiOrientation();
  display_.renderProgress(title == nullptr ? "SD" : title, line1 == nullptr ? "" : line1,
                          line2 == nullptr ? "" : line2, progressPercent);
}

void App::handleStorageStatus(void *context, const char *title, const char *line1,
                              const char *line2, int progressPercent) {
  if (context == nullptr) {
    return;
  }

  static_cast<App *>(context)->renderStorageStatus(title, line1, line2, progressPercent);
  delay(0);
}
