#pragma once

// Canonical registry of device preference keys and the NVS namespace they live
// in. App (the device UI) and CompanionSyncManager (the web companion) both
// read and write the same stored settings; before this header each file kept
// its own copy of these key strings, which could silently drift apart. Keys are
// short by necessity -- the ESP32 Preferences/NVS key length limit is 15 chars.
namespace settings {

constexpr const char *kPrefsNamespace = "rsvp";

// Library / reading position.
constexpr const char *kPrefBookPath = "book";
constexpr const char *kPrefLegacyWordIndex = "word";
constexpr const char *kPrefRecentSeq = "seq";

// Reader + display.
constexpr const char *kPrefWpm = "wpm";
constexpr const char *kPrefBrightness = "bright";
constexpr const char *kPrefDarkMode = "dark";
constexpr const char *kPrefNightMode = "night";
constexpr const char *kPrefUiLanguage = "ui_lang";
constexpr const char *kPrefReaderMode = "read_mode";
constexpr const char *kPrefHandedness = "handed";
constexpr const char *kPrefPhantomWords = "phantom_on";
constexpr const char *kPrefFooterMetricMode = "prog_md";
constexpr const char *kPrefBatteryLabelMode = "bat_md";
constexpr const char *kPrefScreensaverMode = "scrn_sv";
constexpr const char *kPrefIdleStandbyMin = "idle_sby";
constexpr const char *kPrefReaderBatteryVisible = "read_bat";
constexpr const char *kPrefReaderChapterVisible = "read_ch";
constexpr const char *kPrefReaderProgressVisible = "read_pct";
constexpr const char *kPrefReaderFontSize = "font_size";
constexpr const char *kPrefReaderTypeface = "typeface";

// Typography.
constexpr const char *kPrefTypographyFocusHighlight = "type_hlt";
constexpr const char *kPrefTypographyTracking = "type_trk";
constexpr const char *kPrefTypographyAnchor = "type_anc";
constexpr const char *kPrefTypographyGuideWidth = "type_wid";
constexpr const char *kPrefTypographyGuideGap = "type_gap";

// Word pacing.
constexpr const char *kPrefLegacyPacingLong = "pace_len";
constexpr const char *kPrefLegacyPacingComplex = "pace_cpx";
constexpr const char *kPrefLegacyPacingPunctuation = "pace_pnc";
constexpr const char *kPrefPacingLongMs = "pace_lms";
constexpr const char *kPrefPacingComplexMs = "pace_cms";
constexpr const char *kPrefPacingPunctuationMs = "pace_pms";
constexpr const char *kPrefPauseMode = "pause_md";
constexpr const char *kPrefAccurateTime = "time_est_a";

// Reading statistics. The authoritative store is an SD JSON file (see
// App stats persistence); these mirror the all-time totals so the stats screen
// can show something instantly on boot before the card is read. Keys <= 15 chars.
constexpr const char *kPrefStatsWords = "st_words";
constexpr const char *kPrefStatsMs = "st_ms";

// Hands-free IMU shortcut: face-down pauses the reader while playing.
constexpr const char *kPrefImuShortcuts = "imu_short";

// Gesture sensitivity (raw px / ms; presets on-device, raw via companion).
constexpr const char *kPrefGestureSwipePx = "gest_swp";
constexpr const char *kPrefGestureTapPx = "gest_tap";
constexpr const char *kPrefGestureScrubPx = "gest_scr";
constexpr const char *kPrefGestureHoldMs = "gest_hold";

// Audio.
constexpr const char *kPrefAudioMuted = "aud_mute";
constexpr const char *kPrefAudioVolume = "aud_vol";

// Wi-Fi + OTA.
// Legacy single-network pair. Still read for one-time migration into slot 0 of
// the multi-network store (net::WifiCredentialStore), then cleared. New code
// should go through the slot keys below, not these.
constexpr const char *kPrefWifiSsid = "wifi_ssid";
constexpr const char *kPrefWifiPass = "wifi_pass";
// Up to 5 saved home networks. Slots are addressed by net::WifiCredentialStore
// using "wifi_ss<N>" / "wifi_pw<N>" / "wifi_rc<N>" (recency) for N in 0..4, plus
// "wifi_rseq" as the recency sequence source. All keys are <= 15 chars.
constexpr const char *kPrefOtaAuto = "ota_auto";
constexpr const char *kPrefOtaOwner = "ota_owner";
constexpr const char *kPrefOtaLastResult = "ota_res";

}  // namespace settings
