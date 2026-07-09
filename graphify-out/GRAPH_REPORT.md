# Graph Report - rsvpnano  (2026-07-09)

## Corpus Check
- 204 files · ~1,195,018 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2451 nodes · 7446 edges · 28 communities detected
- Extraction: 67% EXTRACTED · 33% INFERRED · 0% AMBIGUOUS · INFERRED: 2451 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 212 edges
2. `reserve()` - 61 edges
3. `substring()` - 53 edges
4. `remove()` - 51 edges
5. `renderStatus()` - 50 edges
6. `close()` - 49 edges
7. `currentIndex()` - 48 edges
8. `open()` - 48 edges
9. `setState()` - 45 edges
10. `startsWith()` - 44 edges

## Surprising Connections (you probably didn't know these)
- `test_previous_sentence_tap_zone()` --calls--> `isPreviousSentenceTap()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano/test/test_touch_gesture/test_main.cpp → src/app/App.cpp
- `loop()` --calls--> `update()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/src/main.cpp → src/sync/CompanionSyncManager.cpp
- `defaultTypographyConfig()` --calls--> `typographyConfig()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano/src/display/DisplayManager.cpp
- `applyDisplayPreferences()` --calls--> `setDarkMode()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano/src/display/DisplayManager.cpp
- `applyDisplayPreferences()` --calls--> `setNightMode()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano/src/display/DisplayManager.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (352): activateTextEntryButton(), App(), applyAudioSettings(), applyBrowseHoldScroll(), applyBurnInJitter(), applyConsumedCompanionTime(), applyDisplayPreferences(), applyHandednessSettings() (+344 more)

### Community 1 - "Community 1"
Cohesion: 0.02
Nodes (294): displayNameForPath(), flushReadingStats(), endsWith(), lastIndexOf(), remove(), reserve(), startsWith(), String() (+286 more)

### Community 2 - "Community 2"
Cohesion: 0.03
Nodes (136): c_str(), decodeUtf8Codepoint(), isUtf8Continuation(), applySettingsJson(), enumValue(), writeRsvpHeader(), InstallFirmware, timeAgo() (+128 more)

### Community 3 - "Community 3"
Cohesion: 0.03
Nodes (86): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, CaseIterable, Codable, addRssFeed(), applyWifiSettings() (+78 more)

### Community 4 - "Community 4"
Cohesion: 0.06
Nodes (147): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+139 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (111): noteRewind(), applyFocusTimerTouch(), focusTimerCountsLabel(), maybeUpdateBackgroundFocusTimer(), protectionAction(), sampleIntervalMs(), blockExpired(), cancelActiveBlock() (+103 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (25): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, Array, Data (+17 more)

### Community 7 - "Community 7"
Cohesion: 0.05
Nodes (92): shouldFinalizeReaderPause(), updateWordSplitFrame(), adjustWpm(), advance(), atEnd(), begin(), currentIndex(), currentWordDurationMs() (+84 more)

### Community 8 - "Community 8"
Cohesion: 0.04
Nodes (80): bookCoverDrift(), bookCoverDriftStep(), jitterOffset(), seed(), advanceRng(), cellAlive(), clearAndStampPattern(), clearRect() (+72 more)

### Community 9 - "Community 9"
Cohesion: 0.05
Nodes (82): currentReaderDisplayWord(), replace(), fold(), main(), Fold to the device's renderable byte range., chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), cleanText() (+74 more)

### Community 10 - "Community 10"
Cohesion: 0.05
Nodes (73): App, copyOtaLabel(), otaCheckTask(), addBookmark(), bookmarks(), bookprogress(), clearBook(), markRecent() (+65 more)

### Community 11 - "Community 11"
Cohesion: 0.05
Nodes (75): maybeStartChapterTransition(), playFocusTimerCompletionCue(), beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail() (+67 more)

### Community 12 - "Community 12"
Cohesion: 0.05
Nodes (64): updateWithSample(), emptyReason(), filterIndices(), matches(), nextFilter(), surprisePick(), classify(), isScreenDown() (+56 more)

### Community 13 - "Community 13"
Cohesion: 0.08
Nodes (57): achievementName(), applyUnlocks(), isUnlocked(), qualifyingMask(), unlockedCount(), openStatsScreen(), averageWpm(), recordSession() (+49 more)

### Community 14 - "Community 14"
Cohesion: 0.08
Nodes (44): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+36 more)

### Community 15 - "Community 15"
Cohesion: 0.1
Nodes (47): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_clause_delay_is_independent_of_sentence_delay(), test_clause_delay_scales_semicolon_and_dash_too(), test_complex_token_costs_more_than_plain_letters(), test_default_config_keeps_legacy_combined_behaviour(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word() (+39 more)

### Community 16 - "Community 16"
Cohesion: 0.12
Nodes (27): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+19 more)

### Community 17 - "Community 17"
Cohesion: 0.11
Nodes (25): test_battery_badge_tap_zone(), test_browse_scroll_rate(), test_clamp_gesture_config(), test_classify_reader_drag_intents(), test_config_changes_thresholds(), test_config_defaults_match_baked_in_constants(), test_footer_metric_tap_zone(), test_horizontal_swipe_needs_threshold_and_axis_bias() (+17 more)

### Community 18 - "Community 18"
Cohesion: 0.11
Nodes (19): count(), DvdBounceScreensaver, LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver, WordRainScreensaver, MemoryStore (+11 more)

### Community 19 - "Community 19"
Cohesion: 0.18
Nodes (24): bonusAt(), builtCache(), linearBonusSum(), test_base_ms_clamps_indices_to_word_count(), test_base_ms_scales_with_wpm(), test_base_ms_zero_cases(), test_bonus_clamps_to_word_count(), test_bonus_full_range_matches_linear_sum() (+16 more)

### Community 20 - "Community 20"
Cohesion: 0.24
Nodes (19): finishReadingSprint(), syncSprintPlayingState(), trackFocusWorkBlock(), beginBlock(), enterPlaying(), finishBlock(), leavePlaying(), netForwardWords() (+11 more)

### Community 21 - "Community 21"
Cohesion: 0.17
Nodes (17): begin(), beginSdCard(), cardSizeBytes(), configureMsc(), deinitHostIfNeeded(), ejected(), end(), endSdCard() (+9 more)

### Community 22 - "Community 22"
Cohesion: 0.2
Nodes (18): epochLooksValid(), epochNowSec(), localDayKey(), localDayKeyNow(), restoreSnapshot(), setReference(), test_clock_invalid_until_set(), test_epoch_now_advances_with_millis() (+10 more)

### Community 23 - "Community 23"
Cohesion: 0.24
Nodes (17): appendDecodedCodepoint(), appendText(), attributeValue(), cleanText(), decodeXmlEntitiesOnce(), decodeXmlEntity(), hexValue(), indexOfIgnoreCase() (+9 more)

### Community 24 - "Community 24"
Cohesion: 0.21
Nodes (14): orpOrdinalForLength(), focusLetterIndex(), layoutWidth(), orpOrdinal(), startX(), test_focus_index_empty_word_is_negative(), test_focus_index_picks_a_letter_inside_the_word(), test_focus_index_skips_leading_punctuation() (+6 more)

### Community 25 - "Community 25"
Cohesion: 0.29
Nodes (10): cancel(), clampDisplayX(), clampDisplayY(), clampPhysicalX(), clampPhysicalY(), end(), poll(), readTouchPacket() (+2 more)

### Community 26 - "Community 26"
Cohesion: 0.27
Nodes (9): isFocusTimerScreen(), wrapSelection(), test_is_focus_timer_screen(), test_is_settings_screen(), test_wrap_selection_empty_list_returns_current(), test_wrap_selection_moves_within_bounds(), test_wrap_selection_single_item_stays(), test_wrap_selection_single_step_not_modulo() (+1 more)

### Community 27 - "Community 27"
Cohesion: 0.33
Nodes (8): absInt(), isHold(), test_hold_cancelled_by_drift(), test_hold_drift_at_box_edge_still_holds(), test_hold_fired_exactly_at_threshold(), test_hold_fires_when_still_and_long(), test_hold_not_fired_before_threshold(), test_hold_respects_custom_config()

## Knowledge Gaps
- **21 isolated node(s):** `Fold to the device's renderable byte range.`, `MemoryStore`, `Array`, `invalidURL`, `articleTooLarge` (+16 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 2` to `Community 0`, `Community 1`, `Community 4`, `Community 5`, `Community 7`, `Community 8`, `Community 9`, `Community 10`, `Community 11`, `Community 18`, `Community 19`, `Community 23`?**
  _High betweenness centrality (0.166) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 8` to `Community 2`, `Community 5`, `Community 7`, `Community 9`, `Community 10`, `Community 11`, `Community 12`, `Community 13`, `Community 15`, `Community 17`, `Community 18`, `Community 19`, `Community 20`, `Community 22`, `Community 24`, `Community 26`, `Community 27`?**
  _High betweenness centrality (0.123) - this node is a cross-community bridge._
- **Why does `reserve()` connect `Community 1` to `Community 0`, `Community 2`, `Community 4`, `Community 5`, `Community 8`, `Community 10`, `Community 12`, `Community 15`, `Community 23`?**
  _High betweenness centrality (0.077) - this node is a cross-community bridge._
- **Are the 208 inferred relationships involving `c_str()` (e.g. with `test_paragraphs_become_lines()` and `test_heading_becomes_chapter_marker_once()`) actually correct?**
  _`c_str()` has 208 INFERRED edges - model-reasoned connections that need verification._
- **Are the 166 inferred relationships involving `String` (e.g. with `test_output_wraps_before_96_columns()` and `test_focus_index_skips_leading_punctuation()`) actually correct?**
  _`String` has 166 INFERRED edges - model-reasoned connections that need verification._
- **Are the 60 inferred relationships involving `reserve()` (e.g. with `update()` and `approximateSyllableGroupCount()`) actually correct?**
  _`reserve()` has 60 INFERRED edges - model-reasoned connections that need verification._
- **Are the 51 inferred relationships involving `substring()` (e.g. with `test_bookmark_key_format()` and `test_key_format_and_prefixes()`) actually correct?**
  _`substring()` has 51 INFERRED edges - model-reasoned connections that need verification._