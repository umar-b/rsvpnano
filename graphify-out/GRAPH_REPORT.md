# Graph Report - rsvpnano  (2026-06-11)

## Corpus Check
- 174 files · ~1,185,891 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2240 nodes · 6817 edges · 31 communities detected
- Extraction: 69% EXTRACTED · 31% INFERRED · 0% AMBIGUOUS · INFERRED: 2102 edges (avg confidence: 0.8)
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
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 30|Community 30]]

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 164 edges
2. `reserve()` - 57 edges
3. `renderStatus()` - 50 edges
4. `substring()` - 49 edges
5. `currentIndex()` - 49 edges
6. `remove()` - 48 edges
7. `remove()` - 45 edges
8. `begin()` - 45 edges
9. `setState()` - 45 edges
10. `startsWith()` - 43 edges

## Surprising Connections (you probably didn't know these)
- `test_is_tap_within_slop()` --calls--> `isTap()`  [INFERRED]
  test/test_touch_gesture/test_main.cpp → src/app/TouchGesture.cpp
- `setup()` --calls--> `begin()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/src/main.cpp → src/sync/CompanionSyncManager.cpp
- `enterUsbTransfer()` --calls--> `statusMessage()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/usb/UsbMassStorageManager.cpp
- `enterPowerOff()` --calls--> `holdBacklightOffForDeepSleep()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/board/BoardConfig.cpp
- `enterSleep()` --calls--> `lightSleepUntilBootButton()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/board/BoardConfig.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (290): displayNameForPath(), flushReadingStats(), jsonUint(), loadReadingStats(), loadStatsHistoryFromJson(), c_str(), endsWith(), lastIndexOf() (+282 more)

### Community 1 - "Community 1"
Cohesion: 0.02
Nodes (300): activateTextEntryButton(), applyAudioSettings(), applyConsumedCompanionTime(), applyDisplayPreferences(), applyFocusTimerTouch(), applyHandednessSettings(), applyMenuTouchGesture(), applyPacingSettings() (+292 more)

### Community 2 - "Community 2"
Cohesion: 0.04
Nodes (142): App(), applyBrowseHoldScroll(), applyBurnInJitter(), beginStatsSession(), collectPhantomAfterText(), collectPhantomBeforeText(), currentChapterIndex(), currentChapterLabel() (+134 more)

### Community 3 - "Community 3"
Cohesion: 0.06
Nodes (146): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+138 more)

### Community 4 - "Community 4"
Cohesion: 0.03
Nodes (97): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, batteryPercentForVoltage(), begin(), configureTca9554OutputPin(), disableBatteryAdcPathIfAvailable() (+89 more)

### Community 5 - "Community 5"
Cohesion: 0.03
Nodes (104): App, seedStandbyScreenOff(), seedStandbyScreensaver(), standbyRngSeed(), protectionAction(), sampleIntervalMs(), active(), update() (+96 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (43): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, decodeUtf8Codepoint(), isUtf8Continuation() (+35 more)

### Community 7 - "Community 7"
Cohesion: 0.05
Nodes (71): jitterOffset(), seed(), advanceRng(), cellAlive(), clearAndStampPattern(), clearRect(), lifeStep(), packedWordCount() (+63 more)

### Community 8 - "Community 8"
Cohesion: 0.05
Nodes (77): currentReaderDisplayWord(), resetWordSplitFrame(), replace(), chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), cleanText(), collapseZipPath(), containerRootfile() (+69 more)

### Community 9 - "Community 9"
Cohesion: 0.08
Nodes (54): beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail(), fillBeepBuffer(), initCodec() (+46 more)

### Community 10 - "Community 10"
Cohesion: 0.08
Nodes (44): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+36 more)

### Community 11 - "Community 11"
Cohesion: 0.07
Nodes (23): ArticleEditorView, ContentView, saveSettings(), RsvpEvent, chapter, text, begin(), beginSdCard() (+15 more)

### Community 12 - "Community 12"
Cohesion: 0.1
Nodes (47): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_clause_delay_is_independent_of_sentence_delay(), test_clause_delay_scales_semicolon_and_dash_too(), test_complex_token_costs_more_than_plain_letters(), test_default_config_keeps_legacy_combined_behaviour(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word() (+39 more)

### Community 13 - "Community 13"
Cohesion: 0.08
Nodes (35): copyOtaLabel(), otaCheckTask(), InstallFirmware, timeAgo(), checkAndInstall(), checkOnly(), connectWiFi(), currentVersion() (+27 more)

### Community 14 - "Community 14"
Cohesion: 0.12
Nodes (27): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+19 more)

### Community 15 - "Community 15"
Cohesion: 0.1
Nodes (34): bookprogress(), clearBook(), markRecent(), nextSequence(), progressPercent(), readPosition(), readWpm(), savePosition() (+26 more)

### Community 16 - "Community 16"
Cohesion: 0.12
Nodes (36): averageWpm(), recordSession(), bestStreak(), currentStreak(), goalProgressPermille(), goalReached(), indexOfDay(), pushNewDay() (+28 more)

### Community 17 - "Community 17"
Cohesion: 0.1
Nodes (26): handleStarSentenceTap(), isPreviousSentenceTap(), isStarSentenceTap(), test_browse_scroll_rate(), test_clamp_gesture_config(), test_classify_reader_drag_intents(), test_config_changes_thresholds(), test_config_defaults_match_baked_in_constants() (+18 more)

### Community 18 - "Community 18"
Cohesion: 0.13
Nodes (26): focusTimerCountsLabel(), abandon(), available(), begin(), cancelActiveTimer(), chooseGenre(), clearSession(), completeActiveTimer() (+18 more)

### Community 19 - "Community 19"
Cohesion: 0.14
Nodes (28): appendDecodedCodepoint(), appendText(), attributeValue(), cleanText(), decodeXmlEntitiesOnce(), decodeXmlEntity(), hexValue(), hostLabelForUrl() (+20 more)

### Community 20 - "Community 20"
Cohesion: 0.11
Nodes (19): count(), DvdBounceScreensaver, LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver, WordRainScreensaver, MemoryStore (+11 more)

### Community 21 - "Community 21"
Cohesion: 0.18
Nodes (22): appendJsonEscaped(), containsQuote(), encodeJsonLine(), extractSentence(), isDigit(), parseJsonLine(), readJsonString(), valueStart() (+14 more)

### Community 22 - "Community 22"
Cohesion: 0.25
Nodes (20): achievementName(), applyUnlocks(), isUnlocked(), qualifyingMask(), unlockedCount(), cleanSlate(), test_apply_unlocks_reports_only_new_bits(), test_bitmask_roundtrip_and_count() (+12 more)

### Community 23 - "Community 23"
Cohesion: 0.2
Nodes (18): epochLooksValid(), epochNowSec(), localDayKey(), localDayKeyNow(), restoreSnapshot(), setReference(), test_clock_invalid_until_set(), test_epoch_now_advances_with_millis() (+10 more)

### Community 24 - "Community 24"
Cohesion: 0.26
Nodes (17): syncSprintPlayingState(), beginBlock(), enterPlaying(), finishBlock(), leavePlaying(), netForwardWords(), test_begin_block_resets_prior_total(), test_block_started_paused_then_play() (+9 more)

### Community 25 - "Community 25"
Cohesion: 0.21
Nodes (14): orpOrdinalForLength(), focusLetterIndex(), layoutWidth(), orpOrdinal(), startX(), test_focus_index_empty_word_is_negative(), test_focus_index_picks_a_letter_inside_the_word(), test_focus_index_skips_leading_punctuation() (+6 more)

### Community 26 - "Community 26"
Cohesion: 0.29
Nodes (10): cancel(), clampDisplayX(), clampDisplayY(), clampPhysicalX(), clampPhysicalY(), end(), poll(), readTouchPacket() (+2 more)

### Community 27 - "Community 27"
Cohesion: 0.29
Nodes (10): renderBookCoverStandby(), bookCoverDrift(), bookCoverDriftStep(), test_deterministic(), test_offset_changes_across_interval(), test_offset_constant_within_interval(), test_offset_within_budget(), test_seed_phases_the_cycle() (+2 more)

### Community 28 - "Community 28"
Cohesion: 0.27
Nodes (9): isFocusTimerScreen(), wrapSelection(), test_is_focus_timer_screen(), test_is_settings_screen(), test_wrap_selection_empty_list_returns_current(), test_wrap_selection_moves_within_bounds(), test_wrap_selection_single_item_stays(), test_wrap_selection_single_step_not_modulo() (+1 more)

### Community 29 - "Community 29"
Cohesion: 0.33
Nodes (8): absInt(), isHold(), test_hold_cancelled_by_drift(), test_hold_drift_at_box_edge_still_holds(), test_hold_fired_exactly_at_threshold(), test_hold_fires_when_still_and_long(), test_hold_not_fired_before_threshold(), test_hold_respects_custom_config()

### Community 30 - "Community 30"
Cohesion: 0.7
Nodes (4): absf(), rateForRoll(), reset(), updateWithSample()

## Knowledge Gaps
- **20 isolated node(s):** `MemoryStore`, `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle` (+15 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 0` to `Community 1`, `Community 2`, `Community 3`, `Community 6`, `Community 7`, `Community 8`, `Community 13`, `Community 15`, `Community 19`, `Community 20`, `Community 21`?**
  _High betweenness centrality (0.133) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 7` to `Community 0`, `Community 2`, `Community 5`, `Community 6`, `Community 8`, `Community 9`, `Community 12`, `Community 13`, `Community 15`, `Community 16`, `Community 17`, `Community 19`, `Community 20`, `Community 21`, `Community 22`, `Community 23`, `Community 24`, `Community 25`, `Community 27`, `Community 28`, `Community 29`?**
  _High betweenness centrality (0.110) - this node is a cross-community bridge._
- **Why does `split()` connect `Community 8` to `Community 0`, `Community 4`, `Community 6`, `Community 10`, `Community 14`?**
  _High betweenness centrality (0.049) - this node is a cross-community bridge._
- **Are the 157 inferred relationships involving `String` (e.g. with `test_focus_index_skips_leading_punctuation()` and `wordAt()`) actually correct?**
  _`String` has 157 INFERRED edges - model-reasoned connections that need verification._
- **Are the 160 inferred relationships involving `c_str()` (e.g. with `test_bookmark_key_format()` and `test_parses_rss_item_fields()`) actually correct?**
  _`c_str()` has 160 INFERRED edges - model-reasoned connections that need verification._
- **Are the 56 inferred relationships involving `reserve()` (e.g. with `approximateSyllableGroupCount()` and `maskedValue()`) actually correct?**
  _`reserve()` has 56 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `renderStatus()` (e.g. with `setState()` and `handleBatteryProtection()`) actually correct?**
  _`renderStatus()` has 39 INFERRED edges - model-reasoned connections that need verification._