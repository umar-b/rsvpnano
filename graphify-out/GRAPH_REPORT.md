# Graph Report - rsvpnano  (2026-06-11)

## Corpus Check
- 123 files · ~1,150,541 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1784 nodes · 5379 edges · 23 communities detected
- Extraction: 74% EXTRACTED · 26% INFERRED · 0% AMBIGUOUS · INFERRED: 1413 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 119 edges
2. `substring()` - 43 edges
3. `reserve()` - 42 edges
4. `remove()` - 42 edges
5. `setState()` - 42 edges
6. `renderStatus()` - 42 edges
7. `startsWith()` - 41 edges
8. `begin()` - 41 edges
9. `rebuildSettingsMenuItems()` - 39 edges
10. `close()` - 39 edges

## Surprising Connections (you probably didn't know these)
- `setup()` --calls--> `begin()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/src/main.cpp → src/sync/CompanionSyncManager.cpp
- `enterUsbTransfer()` --calls--> `statusMessage()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/usb/UsbMassStorageManager.cpp
- `updateUsbTransfer()` --calls--> `ejected()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/usb/UsbMassStorageManager.cpp
- `applyAudioSettings()` --calls--> `setMuted()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/audio/AudioManager.cpp
- `applyAudioSettings()` --calls--> `setVolume()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/audio/AudioManager.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (259): displayNameForPath(), flushReadingStats(), loadReadingStats(), c_str(), endsWith(), lastIndexOf(), remove(), reserve() (+251 more)

### Community 1 - "Community 1"
Cohesion: 0.03
Nodes (268): activateTextEntryButton(), applyAudioSettings(), applyBrowseHoldScroll(), applyDisplayPreferences(), applyFocusTimerTouch(), applyHandednessSettings(), applyMenuTouchGesture(), applyPacingSettings() (+260 more)

### Community 2 - "Community 2"
Cohesion: 0.06
Nodes (144): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+136 more)

### Community 3 - "Community 3"
Cohesion: 0.03
Nodes (87): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, CaseIterable, Codable, addRssFeed(), applyWifiSettings() (+79 more)

### Community 4 - "Community 4"
Cohesion: 0.04
Nodes (100): addBookmark(), bookmarks(), bookprogress(), markRecent(), nextSequence(), progressPercent(), readPosition(), recentSequence() (+92 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (30): formatReadingTimeRemaining(), jsonUint(), ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile (+22 more)

### Community 6 - "Community 6"
Cohesion: 0.05
Nodes (92): collectPhantomBeforeText(), shouldFinalizeReaderPause(), isOpen(), loadWordWindow(), prefetchAround(), readRecords(), wordAt(), adjustWpm() (+84 more)

### Community 7 - "Community 7"
Cohesion: 0.04
Nodes (65): App, focusTimerCountsLabel(), seedStandbyScreenOff(), seedStandbyScreensaver(), standbyRngSeed(), protectionAction(), sampleIntervalMs(), update() (+57 more)

### Community 8 - "Community 8"
Cohesion: 0.08
Nodes (58): replace(), chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), cleanText(), collapseZipPath(), containerRootfile(), convertDescriptorIntoItem(), createLibraryItem() (+50 more)

### Community 9 - "Community 9"
Cohesion: 0.08
Nodes (44): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+36 more)

### Community 10 - "Community 10"
Cohesion: 0.07
Nodes (22): ArticleEditorView, ContentView, saveSettings(), text, begin(), beginSdCard(), cardSizeBytes(), configureMsc() (+14 more)

### Community 11 - "Community 11"
Cohesion: 0.11
Nodes (41): updateWithSample(), absDiff(), canWakeNow(), clearSetDown(), enterStandby(), flatness(), idleApplies(), idleVerdict() (+33 more)

### Community 12 - "Community 12"
Cohesion: 0.1
Nodes (42): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_complex_token_costs_more_than_plain_letters(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word(), test_plain_short_word_gets_no_bonus(), test_punctuation_scale_zero_floor_still_applies_some_pause(), test_sentence_end_pauses_longer_than_comma() (+34 more)

### Community 13 - "Community 13"
Cohesion: 0.09
Nodes (32): copyOtaLabel(), otaCheckTask(), InstallFirmware, timeAgo(), checkAndInstall(), checkOnly(), connectWiFi(), currentVersion() (+24 more)

### Community 14 - "Community 14"
Cohesion: 0.12
Nodes (27): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+19 more)

### Community 15 - "Community 15"
Cohesion: 0.1
Nodes (28): isBatteryBadgeTap(), isFooterMetricTap(), isPreviousSentenceTap(), test_battery_badge_tap_zone(), test_browse_scroll_rate(), test_clamp_gesture_config(), test_classify_reader_drag_intents(), test_config_changes_thresholds() (+20 more)

### Community 16 - "Community 16"
Cohesion: 0.2
Nodes (19): beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail(), fillBeepBuffer(), initCodec() (+11 more)

### Community 17 - "Community 17"
Cohesion: 0.27
Nodes (15): advanceRng(), cellAlive(), clearAndStampPattern(), clearRect(), lifeStep(), packedWordCount(), setCell(), setCellAt() (+7 more)

### Community 18 - "Community 18"
Cohesion: 0.21
Nodes (14): orpOrdinalForLength(), focusLetterIndex(), layoutWidth(), orpOrdinal(), startX(), test_focus_index_empty_word_is_negative(), test_focus_index_picks_a_letter_inside_the_word(), test_focus_index_skips_leading_punctuation() (+6 more)

### Community 19 - "Community 19"
Cohesion: 0.29
Nodes (12): batteryPercentForVoltage(), begin(), configureTca9554OutputPin(), disableBatteryAdcPathIfAvailable(), enableBatteryAdcPathIfAvailable(), holdBacklightOffForDeepSleep(), holdBatteryPowerIfAvailable(), lightSleepUntilBootButton() (+4 more)

### Community 20 - "Community 20"
Cohesion: 0.29
Nodes (10): cancel(), clampDisplayX(), clampDisplayY(), clampPhysicalX(), clampPhysicalY(), end(), poll(), readTouchPacket() (+2 more)

### Community 21 - "Community 21"
Cohesion: 0.27
Nodes (9): isFocusTimerScreen(), wrapSelection(), test_is_focus_timer_screen(), test_is_settings_screen(), test_wrap_selection_empty_list_returns_current(), test_wrap_selection_moves_within_bounds(), test_wrap_selection_single_item_stays(), test_wrap_selection_single_step_not_modulo() (+1 more)

### Community 22 - "Community 22"
Cohesion: 0.67
Nodes (2): App(), HandednessMode()

## Knowledge Gaps
- **19 isolated node(s):** `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle`, `invalidBaseURL` (+14 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 22`** (3 nodes): `App()`, `HandednessMode()`, `App.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 0` to `Community 1`, `Community 4`, `Community 5`, `Community 6`, `Community 8`, `Community 13`?**
  _High betweenness centrality (0.144) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 4` to `Community 0`, `Community 6`, `Community 7`, `Community 11`, `Community 12`, `Community 13`, `Community 15`, `Community 17`, `Community 18`, `Community 21`?**
  _High betweenness centrality (0.074) - this node is a cross-community bridge._
- **Why does `startsWith()` connect `Community 0` to `Community 1`, `Community 4`, `Community 5`, `Community 8`, `Community 9`, `Community 14`?**
  _High betweenness centrality (0.058) - this node is a cross-community bridge._
- **Are the 124 inferred relationships involving `String` (e.g. with `test_focus_index_skips_leading_punctuation()` and `duration()`) actually correct?**
  _`String` has 124 INFERRED edges - model-reasoned connections that need verification._
- **Are the 115 inferred relationships involving `c_str()` (e.g. with `test_bookmark_key_format()` and `test_parses_rss_item_fields()`) actually correct?**
  _`c_str()` has 115 INFERRED edges - model-reasoned connections that need verification._
- **Are the 41 inferred relationships involving `substring()` (e.g. with `test_bookmark_key_format()` and `test_key_format_and_prefixes()`) actually correct?**
  _`substring()` has 41 INFERRED edges - model-reasoned connections that need verification._
- **Are the 41 inferred relationships involving `reserve()` (e.g. with `approximateSyllableGroupCount()` and `maskedValue()`) actually correct?**
  _`reserve()` has 41 INFERRED edges - model-reasoned connections that need verification._