# Graph Report - rsvpnano  (2026-07-09)

## Corpus Check
- 188 files · ~1,188,356 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2355 nodes · 7169 edges · 30 communities detected
- Extraction: 68% EXTRACTED · 32% INFERRED · 0% AMBIGUOUS · INFERRED: 2318 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 193 edges
2. `reserve()` - 57 edges
3. `renderStatus()` - 50 edges
4. `remove()` - 49 edges
5. `substring()` - 48 edges
6. `currentIndex()` - 48 edges
7. `setState()` - 45 edges
8. `startsWith()` - 44 edges
9. `remove()` - 44 edges
10. `begin()` - 43 edges

## Surprising Connections (you probably didn't know these)
- `test_previous_sentence_tap_zone()` --calls--> `isPreviousSentenceTap()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano/test/test_touch_gesture/test_main.cpp → src/app/App.cpp
- `setup()` --calls--> `begin()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/src/main.cpp → src/sync/CompanionSyncManager.cpp
- `systemEpochIfValid()` --calls--> `maybeSyncClockViaSntp()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano/src/net/WifiConnection.cpp → src/app/App.cpp
- `defaultTypographyConfig()` --calls--> `typographyConfig()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano/src/display/DisplayManager.cpp
- `applyDisplayPreferences()` --calls--> `setDarkMode()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano/src/display/DisplayManager.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (345): activateTextEntryButton(), App(), applyAudioSettings(), applyBrowseHoldScroll(), applyBurnInJitter(), applyConsumedCompanionTime(), applyDisplayPreferences(), applyHandednessSettings() (+337 more)

### Community 1 - "Community 1"
Cohesion: 0.02
Nodes (316): displayNameForPath(), flushReadingStats(), jsonUint(), loadReadingStats(), loadStatsHistoryFromJson(), c_str(), endsWith(), lastIndexOf() (+308 more)

### Community 2 - "Community 2"
Cohesion: 0.03
Nodes (85): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, CaseIterable, Codable, addRssFeed(), applyWifiSettings() (+77 more)

### Community 3 - "Community 3"
Cohesion: 0.06
Nodes (144): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+136 more)

### Community 4 - "Community 4"
Cohesion: 0.04
Nodes (102): App, applyFocusTimerTouch(), focusTimerCountsLabel(), maybeUpdateBackgroundFocusTimer(), protectionAction(), sampleIntervalMs(), blockExpired(), cancelActiveBlock() (+94 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (95): maybeStartChapterTransition(), playFocusTimerCompletionCue(), beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail() (+87 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (26): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, upload(), Array (+18 more)

### Community 7 - "Community 7"
Cohesion: 0.05
Nodes (92): shouldFinalizeReaderPause(), adjustWpm(), advance(), atEnd(), begin(), currentWordDurationMs(), currentWordEndsSentence(), elapsedInCurrentWordMs() (+84 more)

### Community 8 - "Community 8"
Cohesion: 0.04
Nodes (80): jitterOffset(), seed(), advanceRng(), cellAlive(), clearAndStampPattern(), clearRect(), lifeStep(), packedWordCount() (+72 more)

### Community 9 - "Community 9"
Cohesion: 0.06
Nodes (69): chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), collapseZipPath(), containerRootfile(), convertDescriptorIntoItem(), createLibraryItem(), decodeTextBytes(), decodeWithDeclaredEncoding() (+61 more)

### Community 10 - "Community 10"
Cohesion: 0.05
Nodes (64): updateWithSample(), emptyReason(), filterIndices(), matches(), nextFilter(), surprisePick(), classify(), isScreenDown() (+56 more)

### Community 11 - "Community 11"
Cohesion: 0.06
Nodes (46): applySettingsJson(), enumValue(), handleTime(), handleTimeStatic(), InstallFirmware, timeAgo(), findKeyColon(), isAsciiWhitespace() (+38 more)

### Community 12 - "Community 12"
Cohesion: 0.1
Nodes (47): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_clause_delay_is_independent_of_sentence_delay(), test_clause_delay_scales_semicolon_and_dash_too(), test_complex_token_costs_more_than_plain_letters(), test_default_config_keeps_legacy_combined_behaviour(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word() (+39 more)

### Community 13 - "Community 13"
Cohesion: 0.1
Nodes (35): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+27 more)

### Community 14 - "Community 14"
Cohesion: 0.12
Nodes (27): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+19 more)

### Community 15 - "Community 15"
Cohesion: 0.12
Nodes (36): averageWpm(), recordSession(), bestStreak(), currentStreak(), goalProgressPermille(), goalReached(), indexOfDay(), pushNewDay() (+28 more)

### Community 16 - "Community 16"
Cohesion: 0.11
Nodes (31): bookprogress(), clearBook(), markRecent(), nextSequence(), progressPercent(), readPosition(), savePosition(), saveWordCount() (+23 more)

### Community 17 - "Community 17"
Cohesion: 0.1
Nodes (27): isBatteryBadgeTap(), isFooterMetricTap(), test_battery_badge_tap_zone(), test_browse_scroll_rate(), test_clamp_gesture_config(), test_classify_reader_drag_intents(), test_config_changes_thresholds(), test_config_defaults_match_baked_in_constants() (+19 more)

### Community 18 - "Community 18"
Cohesion: 0.14
Nodes (28): appendDecodedCodepoint(), appendText(), attributeValue(), cleanText(), decodeXmlEntitiesOnce(), decodeXmlEntity(), hexValue(), hostLabelForUrl() (+20 more)

### Community 19 - "Community 19"
Cohesion: 0.11
Nodes (19): count(), DvdBounceScreensaver, LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver, WordRainScreensaver, MemoryStore (+11 more)

### Community 20 - "Community 20"
Cohesion: 0.15
Nodes (22): copyOtaLabel(), otaCheckTask(), drainBody(), httpGet(), isRedirectStatus(), checkAndInstall(), checkOnly(), connectWiFi() (+14 more)

### Community 21 - "Community 21"
Cohesion: 0.19
Nodes (23): formatReadingTimeRemaining(), bonusAt(), builtCache(), linearBonusSum(), test_base_ms_clamps_indices_to_word_count(), test_base_ms_scales_with_wpm(), test_base_ms_zero_cases(), test_bonus_clamps_to_word_count() (+15 more)

### Community 22 - "Community 22"
Cohesion: 0.15
Nodes (21): decodeUtf8Codepoint(), isUtf8Continuation(), bytes(), test_decode_utf8_rejects_lone_continuation_byte(), test_decode_utf8_two_byte_sequence(), test_em_dash_entity_expands_to_spaced_hyphen(), test_named_xml_entities(), test_normalize_approximates_utf8_punctuation() (+13 more)

### Community 23 - "Community 23"
Cohesion: 0.18
Nodes (21): appendJsonEscaped(), containsQuote(), extractSentence(), parseJsonLine(), readJsonString(), titleCase(), valueStart(), endsSentence() (+13 more)

### Community 24 - "Community 24"
Cohesion: 0.25
Nodes (20): achievementName(), applyUnlocks(), isUnlocked(), qualifyingMask(), unlockedCount(), cleanSlate(), test_apply_unlocks_reports_only_new_bits(), test_bitmask_roundtrip_and_count() (+12 more)

### Community 25 - "Community 25"
Cohesion: 0.17
Nodes (17): begin(), beginSdCard(), cardSizeBytes(), configureMsc(), deinitHostIfNeeded(), ejected(), end(), endSdCard() (+9 more)

### Community 26 - "Community 26"
Cohesion: 0.24
Nodes (19): finishReadingSprint(), syncSprintPlayingState(), trackFocusWorkBlock(), beginBlock(), enterPlaying(), finishBlock(), leavePlaying(), netForwardWords() (+11 more)

### Community 27 - "Community 27"
Cohesion: 0.2
Nodes (18): epochLooksValid(), epochNowSec(), localDayKey(), localDayKeyNow(), restoreSnapshot(), setReference(), test_clock_invalid_until_set(), test_epoch_now_advances_with_millis() (+10 more)

### Community 28 - "Community 28"
Cohesion: 0.21
Nodes (14): orpOrdinalForLength(), focusLetterIndex(), layoutWidth(), orpOrdinal(), startX(), test_focus_index_empty_word_is_negative(), test_focus_index_picks_a_letter_inside_the_word(), test_focus_index_skips_leading_punctuation() (+6 more)

### Community 29 - "Community 29"
Cohesion: 0.29
Nodes (10): renderBookCoverStandby(), bookCoverDrift(), bookCoverDriftStep(), test_deterministic(), test_offset_changes_across_interval(), test_offset_constant_within_interval(), test_offset_within_budget(), test_seed_phases_the_cycle() (+2 more)

## Knowledge Gaps
- **20 isolated node(s):** `MemoryStore`, `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle` (+15 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 1` to `Community 0`, `Community 3`, `Community 4`, `Community 5`, `Community 7`, `Community 8`, `Community 9`, `Community 11`, `Community 16`, `Community 18`, `Community 19`, `Community 20`, `Community 21`, `Community 22`, `Community 23`?**
  _High betweenness centrality (0.142) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 8` to `Community 1`, `Community 4`, `Community 5`, `Community 7`, `Community 9`, `Community 10`, `Community 11`, `Community 12`, `Community 15`, `Community 16`, `Community 17`, `Community 18`, `Community 19`, `Community 21`, `Community 22`, `Community 23`, `Community 24`, `Community 26`, `Community 27`, `Community 28`, `Community 29`?**
  _High betweenness centrality (0.109) - this node is a cross-community bridge._
- **Why does `reserve()` connect `Community 0` to `Community 1`, `Community 3`, `Community 4`, `Community 8`, `Community 10`, `Community 11`, `Community 12`, `Community 16`, `Community 18`, `Community 20`, `Community 23`?**
  _High betweenness centrality (0.047) - this node is a cross-community bridge._
- **Are the 189 inferred relationships involving `c_str()` (e.g. with `test_paragraphs_become_lines()` and `test_heading_becomes_chapter_marker_once()`) actually correct?**
  _`c_str()` has 189 INFERRED edges - model-reasoned connections that need verification._
- **Are the 160 inferred relationships involving `String` (e.g. with `test_output_wraps_before_96_columns()` and `test_focus_index_skips_leading_punctuation()`) actually correct?**
  _`String` has 160 INFERRED edges - model-reasoned connections that need verification._
- **Are the 56 inferred relationships involving `reserve()` (e.g. with `update()` and `approximateSyllableGroupCount()`) actually correct?**
  _`reserve()` has 56 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `renderStatus()` (e.g. with `setState()` and `handleBatteryProtection()`) actually correct?**
  _`renderStatus()` has 39 INFERRED edges - model-reasoned connections that need verification._