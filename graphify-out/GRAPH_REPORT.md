# Graph Report - rsvpnano-feat-b  (2026-06-08)

## Corpus Check
- 106 files · ~1,139,500 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1639 nodes · 4958 edges · 22 communities detected
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 1219 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 106 edges
2. `startsWith()` - 41 edges
3. `substring()` - 41 edges
4. `reserve()` - 40 edges
5. `begin()` - 39 edges
6. `setState()` - 39 edges
7. `remove()` - 38 edges
8. `rebuildSettingsMenuItems()` - 38 edges
9. `renderStatus()` - 38 edges
10. `toLowerCase()` - 36 edges

## Surprising Connections (you probably didn't know these)
- `test_is_settings_screen()` --calls--> `isSettingsScreen()`  [INFERRED]
  test/test_menu/test_main.cpp → src/app/Menu.cpp
- `test_is_tap_within_slop()` --calls--> `isTap()`  [INFERRED]
  test/test_touch_gesture/test_main.cpp → src/app/TouchGesture.cpp
- `merge_firmware()` --calls--> `open()`  [INFERRED]
  tools/export_web_firmware.py → src/storage/IndexedBookStore.cpp
- `download_file()` --calls--> `open()`  [INFERRED]
  tools/fetch_release_firmware.py → src/storage/IndexedBookStore.cpp
- `directive_text()` --calls--> `replace()`  [INFERRED]
  tools/epub_to_rsvp.py → test/support/Arduino.h

## Communities

### Community 0 - "Community 0"
Cohesion: 0.03
Nodes (267): activateTextEntryButton(), App(), applyAudioSettings(), applyDisplayPreferences(), applyFocusTimerTouch(), applyHandednessSettings(), applyMenuTouchGesture(), applyPacingSettings() (+259 more)

### Community 1 - "Community 1"
Cohesion: 0.04
Nodes (146): reserve(), startsWith(), String(), substring(), trim(), active(), applySettingsJson(), applyWifiJson() (+138 more)

### Community 2 - "Community 2"
Cohesion: 0.06
Nodes (146): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+138 more)

### Community 3 - "Community 3"
Cohesion: 0.03
Nodes (104): c_str(), markRecent(), nextSequence(), progressPercent(), readPosition(), recentSequence(), savePosition(), saveWordCount() (+96 more)

### Community 4 - "Community 4"
Cohesion: 0.04
Nodes (66): CaseIterable, Codable, addRssFeed(), applyWifiSettings(), ArticleEditorView, CompanionPage, articles, help (+58 more)

### Community 5 - "Community 5"
Cohesion: 0.06
Nodes (106): displayNameForPath(), endsWith(), lastIndexOf(), remove(), toLowerCase(), available(), addChapterMarker(), addParagraphMarker() (+98 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (25): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, Array, Data (+17 more)

### Community 7 - "Community 7"
Cohesion: 0.06
Nodes (85): shouldFinalizeReaderPause(), adjustWpm(), advance(), atEnd(), begin(), currentIndex(), currentWordDurationMs(), currentWordEndsSentence() (+77 more)

### Community 8 - "Community 8"
Cohesion: 0.08
Nodes (58): replace(), chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), cleanText(), collapseZipPath(), containerRootfile(), convertDescriptorIntoItem(), createLibraryItem() (+50 more)

### Community 9 - "Community 9"
Cohesion: 0.05
Nodes (49): App, applyBrowseHoldScroll(), isBatteryBadgeTap(), isFooterMetricTap(), isPreviousSentenceTap(), protectionAction(), sampleIntervalMs(), update() (+41 more)

### Community 10 - "Community 10"
Cohesion: 0.08
Nodes (43): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+35 more)

### Community 11 - "Community 11"
Cohesion: 0.1
Nodes (43): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_complex_token_costs_more_than_plain_letters(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word(), test_plain_short_word_gets_no_bonus(), test_punctuation_scale_zero_floor_still_applies_some_pause(), test_sentence_end_pauses_longer_than_comma() (+35 more)

### Community 12 - "Community 12"
Cohesion: 0.11
Nodes (28): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+20 more)

### Community 13 - "Community 13"
Cohesion: 0.1
Nodes (33): focusTimerCountsLabel(), abandon(), available(), begin(), cancelActiveTimer(), chooseGenre(), clearSession(), completeActiveTimer() (+25 more)

### Community 14 - "Community 14"
Cohesion: 0.08
Nodes (19): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, LocalizedError, PendingUploadStoreError, emptyDraft, saveVerificationFailed (+11 more)

### Community 15 - "Community 15"
Cohesion: 0.2
Nodes (19): beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail(), fillBeepBuffer(), initCodec() (+11 more)

### Community 16 - "Community 16"
Cohesion: 0.17
Nodes (17): begin(), beginSdCard(), cardSizeBytes(), configureMsc(), deinitHostIfNeeded(), ejected(), end(), endSdCard() (+9 more)

### Community 17 - "Community 17"
Cohesion: 0.27
Nodes (15): advanceRng(), cellAlive(), clearAndStampPattern(), clearRect(), lifeStep(), packedWordCount(), setCell(), setCellAt() (+7 more)

### Community 18 - "Community 18"
Cohesion: 0.29
Nodes (12): batteryPercentForVoltage(), begin(), configureTca9554OutputPin(), disableBatteryAdcPathIfAvailable(), enableBatteryAdcPathIfAvailable(), holdBacklightOffForDeepSleep(), holdBatteryPowerIfAvailable(), lightSleepUntilBootButton() (+4 more)

### Community 19 - "Community 19"
Cohesion: 0.29
Nodes (10): cancel(), clampDisplayX(), clampDisplayY(), clampPhysicalX(), clampPhysicalY(), end(), poll(), readTouchPacket() (+2 more)

### Community 20 - "Community 20"
Cohesion: 0.22
Nodes (4): LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver

### Community 21 - "Community 21"
Cohesion: 0.4
Nodes (3): copyOtaLabel(), otaCheckTask(), OtaUpdater()

## Knowledge Gaps
- **19 isolated node(s):** `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle`, `invalidBaseURL` (+14 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 3` to `Community 0`, `Community 1`, `Community 5`, `Community 7`, `Community 8`, `Community 9`?**
  _High betweenness centrality (0.124) - this node is a cross-community bridge._
- **Why does `startsWith()` connect `Community 1` to `Community 0`, `Community 3`, `Community 5`, `Community 8`, `Community 10`, `Community 12`?**
  _High betweenness centrality (0.066) - this node is a cross-community bridge._
- **Why does `duration()` connect `Community 7` to `Community 1`?**
  _High betweenness centrality (0.053) - this node is a cross-community bridge._
- **Are the 114 inferred relationships involving `String` (e.g. with `duration()` and `test_word_source_streams_words_and_prefetches()`) actually correct?**
  _`String` has 114 INFERRED edges - model-reasoned connections that need verification._
- **Are the 102 inferred relationships involving `c_str()` (e.g. with `test_parses_rss_item_fields()` and `test_parses_atom_entry_with_href_link()`) actually correct?**
  _`c_str()` has 102 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `startsWith()` (e.g. with `write_rsvp()` and `parse_pgm()`) actually correct?**
  _`startsWith()` has 39 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `substring()` (e.g. with `test_key_format_and_prefixes()` and `.replaceNumericEntities()`) actually correct?**
  _`substring()` has 39 INFERRED edges - model-reasoned connections that need verification._