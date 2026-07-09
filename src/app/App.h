#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include <string>
#include <vector>

#include "app/AppState.h"
#include "app/Localization.h"
#include "app/Menu.h"
#include "app/TouchGesture.h"
#include "audio/AudioManager.h"
#include "board/BatteryManager.h"
#include "board/ImuDriver.h"
#include "display/DisplayManager.h"
#include "input/ButtonHandler.h"
#include "input/TouchHandler.h"
#include "motion/FlickDetector.h"
#include "motion/StandbyDecider.h"
#include "net/WifiCredentialStore.h"
#include "quotes/Quote.h"
#include "quotes/QuoteStore.h"
#include "reader/ContextPreview.h"
#include "reader/ReadingLoop.h"
#include "reader/TimeEstimate.h"
#include "rss/RssFeedManager.h"
#include "standby/Screensaver.h"
#include "stats/Achievements.h"
#include "stats/ReadingStats.h"
#include "stats/StatsHistory.h"
#include "storage/BookProgress.h"
#include "time/DeviceClock.h"
#include "storage/StorageManager.h"
#include "sync/CompanionSyncManager.h"
#include "timer/FocusTimer.h"
#include "timer/SprintAccount.h"
#include "update/OtaUpdater.h"
#include "usb/UsbMassStorageManager.h"

class App {
 public:
  enum class ReaderMode : uint8_t {
    Rsvp = 0,
    Scroll = 1,
  };

  enum class HandednessMode : uint8_t {
    Right = 0,
    Left = 1,
  };

  App();

  void begin();
  void update(uint32_t nowMs);

 private:
  static constexpr size_t kOtaVersionLabelMax = 32;
  static constexpr size_t kOtaSummaryLabelMax = 40;
  static constexpr size_t kOtaDetailLabelMax = 96;

  struct OtaCheckResult {
    OtaUpdater::ResultCode code = OtaUpdater::ResultCode::MetadataFailed;
    char currentVersion[kOtaVersionLabelMax] = {};
    char latestVersion[kOtaVersionLabelMax] = {};
    char summary[kOtaSummaryLabelMax] = {};
    char detail[kOtaDetailLabelMax] = {};
  };

  struct OtaCheckTaskParams {
    OtaUpdater::Config config;
    QueueHandle_t resultQueue = nullptr;
  };

  struct PausedTouchSession {
    bool active = false;
    uint16_t startX = 0;
    uint16_t startY = 0;
    uint16_t lastX = 0;
    uint16_t lastY = 0;
    uint32_t startMs = 0;
    uint32_t lastMs = 0;
    size_t startWordIndex = 0;
    int gestureStepsApplied = 0;
    int32_t browseOffsetPermille = 0;
  };

  enum class TouchIntent {
    None,
    PlayHold,
    Scrub,
    BrowseScroll,
    Wpm,
  };

  // Menu screens live in the pure `menu` module so the navigation logic is
  // host-testable; this alias keeps every existing MenuScreen:: call site
  // unchanged.
  using MenuScreen = menu::Screen;

  enum class FooterMetricMode : uint8_t {
    Percentage = 0,
    ChapterTime = 1,
    BookTime = 2,
    // Appended; persisted in NVS, never renumber. Current WPM vs all-time avg.
    PaceVsAverage = 3,
    // Wall-clock chapter finish ("BY 21:42"); needs a valid device clock.
    ChapterClock = 4,
  };

  enum class BatteryLabelMode : uint8_t {
    Percent = 0,
    TimeRemaining = 1,
    Voltage = 2,
  };

  enum class ScreensaverMode : uint8_t {
    Life = 0,
    Maze = 2,
    Voronoi = 3,
    ScreenOff = 6,
    // Appended above the existing gap; persisted in NVS, never renumber.
    WordRain = 7,
    DvdBounce = 8,
    BookCover = 9,
  };

  enum class PauseMode : uint8_t {
    SentenceEnd = 0,
    Instant = 1,
  };

  enum class TextEntryPurpose : uint8_t {
    None,
    WifiPassword,
    OtaOwner,
  };

  enum class KeyboardMode : uint8_t {
    Lower,
    Upper,
    Symbols,
  };

  enum class TextEntryAction : uint8_t {
    Insert,
    SetLower,
    SetUpper,
    SetSymbols,
    Space,
    Backspace,
    Clear,
    ToggleMask,
    Save,
    Cancel,
  };

  struct WifiNetworkInfo {
    String ssid;
    int32_t rssi = 0;
    uint8_t authMode = 0;
  };

  // Semantic kind of each row on the (now dynamic) Wi-Fi settings screen. The
  // saved-network rows vary in count, so rows are dispatched by kind rather than
  // fixed index. SavedNetwork rows carry the slot index in WifiSettingsRow.slot.
  enum class WifiSettingsRowKind : uint8_t {
    Back,
    SavedNetwork,
    AddNetwork,
    AutoUpdate,
    OtaOwner,
    FirmwareVersion,
    CheckNow,
    LastResult,
  };

  struct WifiSettingsRow {
    WifiSettingsRowKind kind = WifiSettingsRowKind::Back;
    int slot = -1;  // valid only for SavedNetwork rows

    WifiSettingsRow() = default;
    WifiSettingsRow(WifiSettingsRowKind kindValue, int slotValue)
        : kind(kindValue), slot(slotValue) {}
  };

  struct TextEntryButton {
    DisplayManager::Button view;
    TextEntryAction action = TextEntryAction::Insert;
    String payload;
  };

  struct TextEntrySession {
    bool active = false;
    TextEntryPurpose purpose = TextEntryPurpose::None;
    KeyboardMode mode = KeyboardMode::Lower;
    MenuScreen returnScreen = MenuScreen::Main;
    String title;
    String prompt;
    String helperText;
    String value;
    String contextValue;
    size_t maxLength = 63;
    bool masked = false;
    bool revealValue = false;
  };

  void setState(AppState nextState, uint32_t nowMs);
  void updateState(uint32_t nowMs);
  void updateReader(uint32_t nowMs);
  void updateWpmFeedback(uint32_t nowMs);
  void maybeSaveReadingPosition(uint32_t nowMs);
  void handleBootButton(uint32_t nowMs);
  void handlePowerButton(uint32_t nowMs);
  bool handleStandbyCombo(uint32_t nowMs);
  void toggleMenuFromPowerButton(uint32_t nowMs);
  void openMainMenu(uint32_t nowMs);
  void cycleBrightness();
  void cycleThemeMode(uint32_t nowMs);
  void updateAutoNight(uint32_t nowMs);
  void cycleUiLanguage(uint32_t nowMs);
  void cycleReaderMode(uint32_t nowMs);
  void cycleHandednessMode(uint32_t nowMs);
  void togglePhantomWords(uint32_t nowMs);
  void cycleReaderFontSize(uint32_t nowMs);
  void applyDisplayPreferences(uint32_t nowMs, bool rerender = true);
  void applyHandednessSettings(uint32_t nowMs, bool rerender = true);
  void applyTypographySettings(uint32_t nowMs, bool rerender = true);
  uint8_t currentBrightnessPercent() const;
  bool updateBatteryStatus(uint32_t nowMs, bool force = false);
  void handleBatteryProtection(uint32_t nowMs);
  void showLowBatteryWarning(uint32_t nowMs);
  void updateBatteryWarningOverlay(uint32_t nowMs);
  void handleTouch(uint32_t nowMs);
  void applyPausedTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void handleReaderTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handleFooterMetricTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handleBatteryBadgeTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handlePreviousSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs);
  bool handleStarSentenceTap(uint16_t x, uint16_t y, uint32_t nowMs);
  void requestReaderPauseAtSentenceEnd(uint32_t nowMs);
  void finalizeReaderPause(uint32_t nowMs);
  bool shouldFinalizeReaderPause(uint32_t nowMs) const;
  void resetReaderTapTracking();
  bool isFooterMetricTap(uint16_t x, uint16_t y) const;
  bool isBatteryBadgeTap(uint16_t x, uint16_t y) const;
  bool isPreviousSentenceTap(uint16_t x, uint16_t y) const;
  bool isStarSentenceTap(uint16_t x, uint16_t y) const;
  // Star (save) the sentence containing the current word to /config/quotes.jsonl.
  void starCurrentSentence(uint32_t nowMs);
  void showStarOverlay(const String &line, uint32_t nowMs);
  void updateStarOverlay(uint32_t nowMs);
  bool isActivelyReading() const;
  bool readerFooterVisible() const;
  DisplayManager::ReaderChrome readerChrome() const;
  String readerFooterStatusLabel() const;
  String onOffLabel(bool enabled) const;
  void applyScrubTarget(int targetSteps, uint32_t nowMs);
  void applyBrowseHoldScroll(uint16_t y, uint32_t elapsedMs, uint32_t nowMs);
  void renderContextBrowsePreview(size_t currentIndex, uint16_t scrollProgressPermille);
  void applyMenuTouchGesture(const TouchEvent &event, uint32_t nowMs);
  void applyFocusTimerTouch(const TouchEvent &event, uint32_t nowMs);
  void moveMenuSelection(int direction);
  void selectMenuItem(uint32_t nowMs);
  void openFocusTimer();
  void updateFocusTimer(uint32_t nowMs);
  void resetFocusTimer();
  void rebuildFocusTimerGenreMenuItems();
  void selectFocusTimerGenre(uint32_t nowMs);
  void openSettings();
  void selectSettingsItem(uint32_t nowMs);
  void openWifiSettings();
  void selectWifiSettingsItem(uint32_t nowMs);
  void openTypographyTuning();
  void selectTypographyTuningItem(uint32_t nowMs);
  void cycleTypographyPreviewSample(int direction);
  // Refresh the typography preview samples from the loaded book (real sentences
  // near the saved position), falling back to the canned samples when no book is
  // loaded or extraction fails.
  void refreshTypographyPreviewSamples();
  size_t typographyPreviewSampleCount() const;
  String typographyPreviewSampleAt(size_t index) const;
  void rebuildSettingsMenuItems();
  void applyPacingSettings();
  // Persist a WPM change: always update the global kPrefWpm fallback, and when a
  // Book is loaded also store it as that book's per-book WPM override so the
  // book resumes at its own speed next time.
  void persistCurrentWpm();
  void maybeAutoCheckForUpdates(uint32_t nowMs);
  bool startBackgroundOtaCheck(const OtaUpdater::Config &config);
  static void otaCheckTask(void *params);
  void pollOtaCheckResult(uint32_t nowMs);
  void maybeOpenUpdateConfirm(uint32_t nowMs);
  bool updateConfirmCanOpen() const;
  bool blockNetworkActionForOtaCheck(const String &title, uint32_t nowMs);
  void runFirmwareUpdate(const OtaUpdater::Config &config, bool automatic, uint32_t nowMs);
  void runFirmwareCheckOnly(uint32_t nowMs);
  String otaLastResultLabel();
  void runRssFeedCheck(uint32_t nowMs);
  OtaUpdater::Config preferredOtaConfig();
  void scanWifiNetworks();
  void renderWifiNetworks();
  void selectWifiNetworkItem(uint32_t nowMs);
  // Multi-network saved-credential helpers (logic in net::WifiCredentialStore).
  net::WifiCredentialStore wifiCredentialStore();
  void migrateLegacyWifiCredential();
  void saveWifiSlot(const String &ssid, const String &password);
  // Resolve the wifi creds OTA/RSS should use right now. Without a scan, returns
  // the most-recently-used saved slot. scanFirst==true does a quick scan and
  // picks the strongest reachable saved network (net::pickConnectionOrder).
  bool resolveHomeWifi(String &ssidOut, String &passwordOut, bool scanFirst);
  // Saved-network detail screen (connect-test / forget).
  void openWifiSavedNetwork(int slot);
  void selectWifiSavedNetworkItem(uint32_t nowMs);
  void runWifiConnectTest(int slot, uint32_t nowMs);
  void openTextEntry(TextEntryPurpose purpose, const String &title, const String &prompt,
                     const String &helperText, const String &initialValue,
                     const String &contextValue, bool masked, size_t maxLength,
                     MenuScreen returnScreen);
  void rebuildTextEntryButtons();
  void renderTextEntry();
  bool handleTextEntryTap(uint16_t x, uint16_t y, uint32_t nowMs);
  void activateTextEntryButton(size_t buttonIndex, uint32_t nowMs);
  void commitTextEntry(uint32_t nowMs);
  String configuredWifiSsid();
  bool otaAutoCheckEnabled();
  String otaOwnerLabel();
  String pacingDelayLabel(uint16_t delayMs) const;
  String firmwareUpdateMenuLabel() const;
  String themeModeLabel() const;
  String phantomWordsLabel() const;
  String focusHighlightLabel() const;
  String uiLanguageLabel() const;
  String readerModeLabel() const;
  String pauseModeLabel() const;
  String handednessLabel() const;
  String readerFontSizeLabel() const;
  String readerTypefaceLabel() const;
  String typographyTuningLabel() const;
  String typographyTuningValueLabel() const;
  String uiText(UiText key) const;
  void openBookPicker(bool articlesOnly = false);
  void selectBookPickerItem(uint32_t nowMs);
  size_t bookIndexForPickerRow(size_t row) const;
  void openChapterPicker();
  void selectChapterPickerItem(uint32_t nowMs);
  void toggleCurrentBookFinished(uint32_t nowMs);
  // Library features: filter cycling, "Surprise me", per-book action sheet +
  // delete confirmation. The pure decisions live in src/library; these own the
  // Arduino/NVS/UI glue.
  void cycleBookPickerFilter();
  void rebuildBookPicker(bool resetSelection);
  void openSurpriseBook(uint32_t nowMs);
  void openBookActions(size_t bookIndex);
  void selectBookActionItem(uint32_t nowMs);
  void renderBookActions();
  void openBookDeleteConfirm();
  void selectBookDeleteConfirmItem(uint32_t nowMs);
  void renderBookDeleteConfirm();
  void openBookmarkPicker();
  void selectBookmarkPickerItem(uint32_t nowMs);
  void renderBookmarkPicker();
  // Starred sentences: on-device list of saved quotes, jump-to and remove.
  void openStarredPicker();
  void selectStarredPickerItem(uint32_t nowMs);
  void renderStarredPicker();
  bool jumpToQuote(const quotes::Quote &quote, uint32_t nowMs);
  // Reading statistics: persistence shim, Playing-session record hooks, screen.
  void loadReadingStats();
  void flushReadingStats();
  void beginStatsSession(uint32_t nowMs);
  void endStatsSession(uint32_t nowMs);
  size_t countFinishedBooks();
  void openStatsScreen();
  // Achievements: gather the current data snapshot, fold it into achievementMask_,
  // and -- when something new unlocked -- raise the overlay + ping. Evaluated at
  // natural seams (stats load, session end, book finished), never per-word.
  void loadAchievements();
  void evaluateAchievements(uint32_t nowMs, bool allowOverlay);
  void showAchievementOverlay(const String &name, uint32_t nowMs);
  void updateAchievementOverlay(uint32_t nowMs);
  // Book-finished celebration screen (LifeGrid sparkle one-shot). Begun when a
  // book is auto-marked finished; stepped each frame; dismissed by tap/button.
  void beginBookCelebration(uint32_t nowMs);
  bool updateBookCelebration(uint32_t nowMs);
  void renderBookCelebration();
  void dismissBookCelebration(uint32_t nowMs);
  // Boot greeting line shown over the loading screen: progress percent (+ time
  // left when the estimate is cheaply available). No clock/estimate build forced.
  String bootGreetingLine();
  // Day key for the current session: the local calendar day from deviceClock_
  // when valid, else the per-boot session key. Used by both ReadingStats and
  // StatsHistory so they bucket in lockstep.
  uint32_t currentStatsDayKey(uint32_t nowMs) const;
  uint32_t dailyWordGoal() const;
  void cycleDailyWordGoal(uint32_t nowMs);
  String dailyWordGoalLabel() const;

  // Device clock: opportunistic SNTP while on home Wi-Fi, NVS snapshot restore on
  // boot + periodic refresh. Companion-set time arrives via CompanionSyncManager.
  void loadDeviceClock();
  void persistDeviceClockSnapshot();
  void maybeSyncClockViaSntp(uint32_t nowMs);
  void applyConsumedCompanionTime(uint32_t nowMs);
  void openRestartConfirm();
  void selectRestartConfirmItem(uint32_t nowMs);
  void openSdCardRepairConfirm();
  void selectSdCardRepairConfirmItem(uint32_t nowMs);
  void runSdCardRepair(uint32_t nowMs);
  void runSdCardCheck(uint32_t nowMs);
  void openUpdateConfirm();
  void selectUpdateConfirmItem(uint32_t nowMs);
  void enterCompanionSync(uint32_t nowMs);
  void updateCompanionSync(uint32_t nowMs);
  void exitCompanionSync(uint32_t nowMs);
  void enterUsbTransfer(uint32_t nowMs);
  void updateUsbTransfer(uint32_t nowMs);
  void exitUsbTransfer(uint32_t nowMs);
  void enterStandby(uint32_t nowMs);
  void exitStandby(uint32_t nowMs);
  void handleStandbyTouchWake(uint32_t nowMs);
  void noteUserInput(uint32_t nowMs);
  void cycleIdleStandbyTimeout();
  String idleStandbyLabel() const;
  String standbyTouchLabel() const;
  // Words sampled around the current reading position for the word-rain saver;
  // falls back to an empty list (the saver then uses its built-in words).
  std::vector<std::string> sampleStandbyWords() const;
  void renderBookCoverStandby(uint32_t nowMs);
  void loadGestureConfig();
  void cycleGestureSensitivity();
  String gestureSensitivityLabel() const;
  void applyAudioSettings();
  void toggleAudioMute();
  void cycleAudioVolume();
  void toggleSoundChime();
  String audioMuteLabel() const;
  String audioVolumeLabel() const;
  String soundChimeLabel() const;
  // Reading-sprint background tracking. The focus-timer work block keeps
  // advancing while the reader leaves its page; these manage that lifecycle and
  // the "Block done" overlay.
  void syncSprintPlayingState(uint32_t nowMs);
  void maybeUpdateBackgroundFocusTimer(uint32_t nowMs);
  void trackFocusWorkBlock(uint32_t nowMs, bool allowOverlay);
  void finishReadingSprint(uint32_t nowMs, bool allowOverlay);
  bool updateSprintOverlay(uint32_t nowMs);
  void renderSprintOverlay();
  void seedStandbyScreensaver(uint32_t nowMs);
  void stepStandbyScreensaver(uint32_t nowMs);
  uint32_t standbyRngSeed(uint32_t nowMs) const;
  void seedStandbyScreenOff(uint32_t nowMs);
  void updateStandbyScreensaver(uint32_t nowMs, bool force = false);
  void enterPowerOff(uint32_t nowMs);
  void enterSleep(uint32_t nowMs);
  void wakeFromSleep();
  bool restoreSavedBook(uint32_t nowMs);
  bool prepareBootBookLoad();
  void loadPendingBootBook(uint32_t nowMs);
  void saveReadingPosition(bool force = false);
  bool loadBookAtIndex(size_t index, uint32_t nowMs, bool allowLegacyPositionFallback = false,
                       bool allowIndexBuild = true, bool allowEpubConversion = true,
                       bool rebuildTimeEstimate = true);
  bool bookProgressPercent(size_t bookIndex, uint8_t &percent);
  int findBookIndexByPath(const String &path) const;
  void renderMenu();
  void renderMainMenu();
  void renderSettings();
  void renderTypographyTuning();
  void renderBookPicker();
  void renderChapterPicker();
  void renderRestartConfirm();
  void renderSdCardRepairConfirm();
  void renderUpdateConfirm();
  void renderFocusTimerGenres();
  void renderFocusTimerSession();
  void renderActiveReader(uint32_t nowMs);
  void applyBurnInJitter(uint32_t nowMs);
  bool updateChapterTransition(uint32_t nowMs);
  bool maybeStartChapterTransition(size_t previousWordIndex, size_t currentWordIndex,
                                   uint32_t nowMs);
  void renderChapterTransition();
  void renderScrollReader(uint32_t nowMs, const String &overlayText = "");
  DisplayManager::LibraryItem libraryItemForBook(size_t bookIndex);
  String chapterMenuLabel(size_t chapterIndex) const;
  size_t currentChapterIndex() const;
  String currentChapterLabel() const;
  String currentFooterMetricLabel() const;
  String currentBatteryLabel() const;
  String footerMetricModeLabel() const;
  String batteryLabelModeLabel() const;
  String screensaverModeLabel() const;
  String batteryTimeRemainingLabel() const;
  String batteryVoltageLabel() const;
  String formatBatteryTimeRemaining(uint32_t minutes) const;
  uint32_t estimatedReadingTimeRemainingMs(size_t startIndex, size_t endIndex) const;
  uint32_t estimatedPacingBonusMs(size_t startIndex, size_t endIndex) const;
  void rebuildTimeEstimateCache();
  void invalidateTimeEstimateCache();
  void flushPendingTimeEstimateRebuild();
  void updateTimeEstimateBuild(uint32_t nowMs);
  bool timeEstimateBuildMatchesCurrentBook() const;
  String formatReadingTimeRemaining(uint32_t remainingMs) const;
  String timeEstimateModeLabel() const;
  uint8_t readingProgressPercent() const;
  bool ensureCurrentBookWordAvailable(uint32_t nowMs);
  void handleCurrentBookReadFailure(uint32_t nowMs, const char *detail);
  void renderReaderWord();
  // The display text for the current word, accounting for long-word splitting:
  // when the current word is being shown as two sub-frames, returns the active
  // sub-frame ("hyphen-" or the remainder); otherwise the whole word. Also
  // (re)initialises the split-frame state for the current word/duration.
  String currentReaderDisplayWord();
  // Advance the split sub-frame timer while Playing; returns true if the
  // visible sub-frame changed and the word needs re-rendering.
  bool updateWordSplitFrame(uint32_t nowMs);
  void resetWordSplitFrame();
  void renderContextPreview();
  void renderWpmFeedback(uint32_t nowMs);
  // The whole-book reading-time delta caused by a WPM change, formatted for the
  // feedback overlay (e.g. "-14 min"), or "" when the estimate cache is
  // invalid. previousWpm is the speed before the change just applied.
  String wpmChangeConsequenceLabel(uint16_t previousWpm) const;
  size_t phantomBeforeCharTarget() const;
  size_t phantomAfterCharTarget() const;
  String collectPhantomBeforeText(size_t currentIndex, size_t charTarget) const;
  String collectPhantomAfterText(size_t currentIndex, size_t charTarget) const;
  String phantomBeforeText() const;
  String phantomAfterText() const;
  void updateContextPreviewWindow(size_t currentIndex);
  void invalidateContextPreviewWindow();
  void renderStorageStatus(const char *title, const char *line1, const char *line2,
                           int progressPercent);
  static void handleStorageStatus(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);
  const char *stateName(AppState state) const;
  const char *touchPhaseName(TouchPhase phase) const;
  bool isFocusTimerMenuScreen(MenuScreen screen) const;
  bool scrollModeEnabled() const;
  void applyUiOrientation(BoardConfig::UiOrientation orientation);
  void applyReaderUiOrientation();
  void reloadRuntimePreferences(uint32_t nowMs, bool rerender);
  BoardConfig::UiOrientation readerUiOrientation() const;
  bool uiRotated180() const;
  uint8_t effectiveAnchorPercent() const;
  DisplayManager::TypographyConfig effectiveTypographyConfig() const;
  uint32_t currentReaderContentToken() const;
  String formatFocusTimerRemaining(uint32_t nowMs) const;
  String focusTimerCountsLabel() const;
  void playFocusTimerCompletionCue();

  // Standby decision + motion gestures. The decider owns the timing (set-down,
  // lift-to-wake, wake grace, idle timeout); App polls the sensor, feeds it
  // samples and context, and executes its verdicts. The flick detector shares
  // the sample stream: a sharp flick rewinds to the previous sentence.
  void updateStandbyDecision(uint32_t nowMs);
  motion::StandbyContext standbyContext() const;
  void rewindToPreviousSentence(uint32_t nowMs);

  AppState state_ = AppState::Booting;
  AppState standbyReturnState_ = AppState::Paused;
  DisplayManager display_;
  AudioManager audio_;
  FocusTimer focusTimer_;
  ImuDriver imuShortcutImu_;
  bool imuShortcutsEnabled_ = false;
  bool imuShortcutSensorReady_ = false;
  uint32_t imuShortcutLastPollMs_ = 0;
  motion::StandbyDecider standbyDecider_;
  motion::FlickDetector flickDetector_;
  ReadingLoop reader_;
  ButtonHandler button_;
  ButtonHandler powerButton_;
  TouchHandler touch_;
  StorageManager storage_;
  IndexedBookStore activeBookStore_;
  OtaUpdater otaUpdater_;
  RssFeedManager rssFeedManager_;
  CompanionSyncManager companionSync_;
  UsbMassStorageManager usbTransfer_;
  Preferences preferences_;
  BookProgress bookProgress_{preferences_};
  QuoteStore quoteStore_;
  stats::ReadingStats readingStats_;
  stats::StatsHistory statsHistory_;
  // Unlocked-achievement bitmask (stats::Achievement bits), persisted to NVS.
  // recentAchievement_ holds the most recent unlock id for the stats line and
  // the overlay; -1 == none yet this boot.
  uint32_t achievementMask_ = 0;
  int recentAchievement_ = -1;
  devclock::DeviceClock deviceClock_;
  battery::Monitor batteryMonitor_;
  PausedTouchSession pausedTouch_;
  TouchIntent pausedTouchIntent_ = TouchIntent::None;

  uint32_t bootStartedMs_ = 0;
  uint32_t lastStateLogMs_ = 0;
  uint32_t wpmFeedbackUntilMs_ = 0;
  uint32_t lastProgressSaveMs_ = 0;
  uint32_t lastBatterySampleMs_ = 0;
  uint32_t lastScrollAnimationRenderMs_ = 0;
  uint32_t lastCompanionSyncRenderMs_ = 0;
  uint32_t lastReaderTapMs_ = 0;
  uint32_t standbyComboStartedMs_ = 0;
  uint32_t lastStandbyFrameMs_ = 0;
  uint32_t chapterTransitionUntilMs_ = 0;
  uint32_t lastLowBatteryWarningMs_ = 0;
  uint32_t batteryWarningRestoreAtMs_ = 0;
  uint32_t starOverlayUntilMs_ = 0;
  bool starOverlayVisible_ = false;
  // Achievement-unlock overlay: a brief "Achievement: <name>" banner over the
  // reader, mirroring the star/WPM overlays. One-shot, dismissed by timeout.
  uint32_t achievementOverlayUntilMs_ = 0;
  bool achievementOverlayVisible_ = false;
  String achievementOverlayName_;
  // Book-finished celebration: a one-shot LifeGrid sparkle burst over a
  // congratulatory card, stepped for ~kBookCelebrationMs. Dismissible by any tap
  // or button, which returns to the book picker.
  bool bookCelebrationActive_ = false;
  uint32_t bookCelebrationUntilMs_ = 0;
  uint32_t bookCelebrationLastFrameMs_ = 0;
  uint32_t bookCelebrationGeneration_ = 0;
  String bookCelebrationTitle_;
  size_t bookCelebrationNumber_ = 0;
  uint32_t bookCelebrationAvgWpm_ = 0;
  std::vector<uint32_t> bookCelebrationCells_;
  std::vector<uint32_t> bookCelebrationNext_;
  size_t lastSavedWordIndex_ = static_cast<size_t>(-1);
  size_t currentBookIndex_ = 0;
  size_t pendingBootBookIndex_ = 0;
  size_t menuSelectedIndex_ = 0;
  size_t settingsSelectedIndex_ = 0;
  size_t wifiNetworkSelectedIndex_ = 0;
  size_t bookPickerSelectedIndex_ = 0;
  size_t chapterPickerSelectedIndex_ = 0;
  size_t bookmarkPickerSelectedIndex_ = 0;
  size_t starredPickerSelectedIndex_ = 0;
  size_t chapterTransitionIndex_ = static_cast<size_t>(-1);
  // Live Playing-session tracking for reading stats. Captured when entering
  // Playing, folded into readingStats_ when leaving it. -1 word index = no
  // active session.
  uint32_t statsPlayStartMs_ = 0;
  size_t statsPlayStartWordIndex_ = static_cast<size_t>(-1);
  uint32_t statsSessionDayKey_ = 0;
  // The most recently completed Playing session, captured for the SpeedReader
  // achievement predicate (a sustained fast read). 0/0 outside a session.
  uint32_t lastSessionWords_ = 0;
  uint32_t lastSessionAvgWpm_ = 0;
  // Reading sprint: words read while a focus-timer work block runs in the
  // background. The overlay shows the result briefly when the block completes.
  sprint::SprintAccount sprint_;
  bool sprintActive_ = false;        // a work block is being tracked
  bool sprintWasPlaying_ = false;    // last-known Playing state, for segment edges
  bool sprintOverlayVisible_ = false;
  uint32_t sprintOverlayUntilMs_ = 0;
  uint32_t sprintOverlayWords_ = 0;
  uint8_t sprintWorkBlockCountAtStart_ = 0;  // completedWorkBlocks at sprint begin
  // Per-boot fallback day key, used when the device clock is invalid (no RTC/NTP
  // and no companion time yet). Real calendar day keys from deviceClock_ replace
  // it once the clock is valid.
  uint32_t statsBootDayKey_ = 0;
  uint32_t dailyWordGoal_ = stats::kDefaultDailyGoal;
  uint32_t lastClockSnapshotMs_ = 0;
  bool clockSyncAttemptedThisLink_ = false;
  size_t restartConfirmSelectedIndex_ = 0;
  size_t sdCardRepairConfirmSelectedIndex_ = 0;
  size_t updateConfirmSelectedIndex_ = 0;
  size_t focusTimerGenreSelectedIndex_ = 0;
  uint8_t brightnessLevelIndex_ = 4;
  uint8_t readerFontSizeIndex_ = 0;
  uint8_t idleStandbyMinutes_ = 0;  // 0 = off (preserves prior behaviour)
  uint8_t audioVolumePercent_ = 100;
  bool audioMuted_ = false;
  bool soundChimeEnabled_ = false;  // chapter chime + book fanfare; default OFF
  touchgesture::GestureConfig gestureConfig_;
  uint16_t pacingLongWordDelayMs_ = 200;
  uint16_t pacingComplexWordDelayMs_ = 200;
  uint16_t pacingPunctuationDelayMs_ = 200;
  uint16_t pacingClausePauseDelayMs_ = 100;  // device default: half of punctuation
  size_t typographyTuningSelectedIndex_ = 1;
  size_t typographyPreviewSampleIndex_ = 0;
  // Book-sourced typography preview samples; empty -> use the canned samples.
  std::vector<String> typographyPreviewSamples_;
  MenuScreen menuScreen_ = MenuScreen::Main;
  MenuScreen restartConfirmReturnScreen_ = MenuScreen::Main;
  QueueHandle_t otaCheckQueue_ = nullptr;
  std::vector<String> settingsMenuItems_;
  std::vector<String> focusTimerGenreMenuItems_;
  std::vector<DisplayManager::LibraryItem> wifiNetworkMenuItems_;
  std::vector<DisplayManager::LibraryItem> bookMenuItems_;
  std::vector<size_t> bookPickerBookIndices_;
  // Library-feature state. Whether the picker is showing Articles vs Books
  // (so the filter persists to the right pref and Surprise scopes correctly),
  // the active filter, the per-picker selection inside the action sheet, and
  // the book index a held row / action sheet is acting on. uint8_t filter
  // values are persisted raw; see library::LibraryFilter.
  bool bookPickerArticles_ = false;
  uint8_t booksFilterRaw_ = 0;
  uint8_t articlesFilterRaw_ = 0;
  size_t bookActionSelectedIndex_ = 0;
  size_t bookDeleteConfirmSelectedIndex_ = 0;
  size_t bookActionBookIndex_ = static_cast<size_t>(-1);
  bool menuHoldFired_ = false;
  std::vector<String> chapterMenuItems_;
  std::vector<uint32_t> bookmarkMenuWordIndices_;
  std::vector<String> bookmarkMenuItems_;
  std::vector<quotes::Quote> starredQuotes_;
  std::vector<String> starredMenuItems_;
  bool starredRemoveMode_ = false;
  std::vector<ChapterMarker> chapterMarkers_;
  std::vector<size_t> paragraphStarts_;
  timeestimate::PrefixCache timeEstimate_;
  String timeEstimateBuildBookPath_;
  uint32_t timeEstimateBuildStartedMs_ = 0;
  uint32_t timeEstimateBuildLastLogMs_ = 0;
  bool accurateTimeEstimateEnabled_ = true;
  bool pacingCacheDirty_ = false;
  contextpreview::Window contextPreview_;
  std::vector<WifiNetworkInfo> wifiNetworks_;
  // Wi-Fi settings screen is dynamic: this maps each visible row to its action.
  std::vector<WifiSettingsRow> wifiSettingsRows_;
  // Slot whose connect/forget detail screen is open.
  int wifiSavedNetworkSlot_ = -1;
  size_t wifiSavedNetworkSelectedIndex_ = 0;
  // SSID of the last network we successfully associated with; marked in the
  // saved-network list. Empty until a connect-test or a feature connect succeeds.
  String lastConnectedWifiSsid_;
  std::vector<TextEntryButton> textEntryButtons_;
  std::unique_ptr<standby::Screensaver> screensaver_;
  String currentBookPath_;
  String currentBookTitle_;
  String pendingUpdateCurrentVersion_;
  String pendingUpdateNewVersion_;
  String batteryLabel_;
  // The whole-book reading-time delta to show under the WPM feedback overlay,
  // captured at the moment WPM changes and held while the overlay is up. Empty
  // when the time-estimate cache was invalid.
  String pendingWpmConsequenceLabel_;
  TextEntrySession textEntrySession_;
  uint16_t lastReaderTapX_ = 0;
  uint16_t lastReaderTapY_ = 0;
  bool touchInitialized_ = false;
  bool touchPlayHeld_ = false;
  bool playLocked_ = false;
  bool pauseAtSentenceEndRequested_ = false;
  bool lastReaderTapValid_ = false;
  bool bootButtonReleasedSinceBoot_ = false;
  bool bootButtonLongPressHandled_ = false;
  bool powerButtonReleasedSinceBoot_ = false;
  bool powerButtonLongPressHandled_ = false;
  bool powerOffStarted_ = false;
  bool standbyComboActive_ = false;
  bool standbyComboHandled_ = false;
  bool standbyButtonsReleased_ = false;
  bool standbyScreenOffActive_ = false;
  // Set down screen-down: this standby entry goes screen-off regardless of
  // the screensaver preference. Cleared on wake.
  bool standbyScreenOffForced_ = false;
  bool standbyWakeTouchActive_ = false;
  // Standby touch behaviour: OFF (default) = tap wakes; ON = tap stamps a glider
  // when the Life saver is active (other savers still wake on tap).
  bool standbyTouchPlay_ = false;
  uint16_t standbyWakeStartX_ = 0;
  uint16_t standbyWakeStartY_ = 0;
  // millis() at the most recent standby entry; drives the book-cover burn-in
  // drift schedule.
  uint32_t standbyEnteredMs_ = 0;
  bool chapterTransitionVisible_ = false;
  bool batteryWarningOverlayVisible_ = false;
  bool focusTimerCancelHoldTriggered_ = false;
  bool otaCheckInProgress_ = false;
  bool otaUpdatePromptPending_ = false;
  bool contextViewVisible_ = false;
  bool wpmFeedbackVisible_ = false;
  bool usingStorageBook_ = false;
  bool storageReady_ = false;
  bool pendingBootBookLoad_ = false;
  bool pendingBootBookLegacyFallback_ = false;
  bool phantomWordsEnabled_ = true;
  bool rampInEnabled_ = true;        // ease the first words after a resume up to WPM
  bool pauseContextEnabled_ = false;  // show surrounding sentence while Paused
  // Long-word splitting: the current word may render as two sequential
  // sub-frames ("hyphen-" then "ation"). The reader's word index is unchanged;
  // these track which sub-frame is showing and when to swap. -1 / inactive
  // means the current word is not split.
  bool wordSplitActive_ = false;
  bool wordSplitShowingSecond_ = false;
  size_t wordSplitWordIndex_ = static_cast<size_t>(-1);
  uint32_t wordSplitFirstDurationMs_ = 0;
  bool readerBatteryVisibleWhilePlaying_ = true;
  bool readerChapterVisibleWhilePlaying_ = false;
  bool readerProgressVisibleWhilePlaying_ = false;
  FooterMetricMode footerMetricMode_ = FooterMetricMode::Percentage;
  BatteryLabelMode batteryLabelMode_ = BatteryLabelMode::Percent;
  ScreensaverMode screensaverMode_ = ScreensaverMode::Life;
  PauseMode pauseMode_ = PauseMode::SentenceEnd;
  bool darkMode_ = true;
  bool nightMode_ = false;
  // Auto night: flip the theme on the schedule's day/night edges (manual
  // changes hold between edges). lastState -1 = not yet evaluated.
  bool autoNightEnabled_ = false;
  int8_t autoNightLastState_ = -1;
  bool autoNightPrevDark_ = true;
  uint32_t lastAutoNightCheckMs_ = 0;
  UiLanguage uiLanguage_ = UiLanguage::English;
  ReaderMode readerMode_ = ReaderMode::Rsvp;
  HandednessMode handednessMode_ = HandednessMode::Right;
  DisplayManager::TypographyConfig typographyConfig_;
};
