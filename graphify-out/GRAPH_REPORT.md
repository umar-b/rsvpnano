# Graph Report - rsvpnano-feat-b  (2026-06-08)

## Corpus Check
- 106 files · ~1,140,657 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1648 nodes · 4985 edges · 18 communities detected
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 1230 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 106 edges
2. `startsWith()` - 41 edges
3. `substring()` - 41 edges
4. `reserve()` - 40 edges
5. `begin()` - 40 edges
6. `setState()` - 39 edges
7. `rebuildSettingsMenuItems()` - 39 edges
8. `remove()` - 38 edges
9. `renderStatus()` - 38 edges
10. `toLowerCase()` - 36 edges

## Surprising Connections (you probably didn't know these)
- `test_is_settings_screen()` --calls--> `isSettingsScreen()`  [INFERRED]
  test/test_menu/test_main.cpp → src/app/Menu.cpp
- `test_returns_false_when_tag_missing()` --calls--> `parse()`  [INFERRED]
  test/test_release_parser/test_main.cpp → src/update/ReleaseParser.cpp
- `merge_firmware()` --calls--> `open()`  [INFERRED]
  tools/export_web_firmware.py → src/storage/IndexedBookStore.cpp
- `download_file()` --calls--> `open()`  [INFERRED]
  tools/fetch_release_firmware.py → src/storage/IndexedBookStore.cpp
- `directive_text()` --calls--> `replace()`  [INFERRED]
  tools/epub_to_rsvp.py → test/support/Arduino.h

## Communities

### Community 0 - "Community 0"
Cohesion: 0.03
Nodes (240): activateTextEntryButton(), App(), applyAudioSettings(), applyBrowseHoldScroll(), applyDisplayPreferences(), applyHandednessSettings(), applyMenuTouchGesture(), applyPacingSettings() (+232 more)

### Community 1 - "Community 1"
Cohesion: 0.03
Nodes (216): displayNameForPath(), endsWith(), lastIndexOf(), remove(), reserve(), startsWith(), String(), substring() (+208 more)

### Community 2 - "Community 2"
Cohesion: 0.06
Nodes (145): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+137 more)

### Community 3 - "Community 3"
Cohesion: 0.04
Nodes (118): chapterMenuLabel(), collectPhantomAfterText(), collectPhantomBeforeText(), currentChapterIndex(), currentChapterLabel(), currentFooterMetricLabel(), currentReaderContentToken(), estimatedPacingBonusMs() (+110 more)

### Community 4 - "Community 4"
Cohesion: 0.04
Nodes (107): c_str(), markRecent(), nextSequence(), progressPercent(), readPosition(), recentSequence(), savePosition(), saveWordCount() (+99 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (65): CaseIterable, Codable, addRssFeed(), applyWifiSettings(), ArticleEditorView, CompanionPage, articles, help (+57 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (26): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, upload(), Array (+18 more)

### Community 7 - "Community 7"
Cohesion: 0.04
Nodes (73): protectionAction(), sampleIntervalMs(), update(), advanceRng(), cellAlive(), clearAndStampPattern(), clearRect(), lifeStep() (+65 more)

### Community 8 - "Community 8"
Cohesion: 0.08
Nodes (56): replace(), chooseBooksDirectory(), cleanSidecarsInSelectedDirectory(), cleanText(), collapseZipPath(), containerRootfile(), convertDescriptorIntoItem(), createLibraryItem() (+48 more)

### Community 9 - "Community 9"
Cohesion: 0.08
Nodes (43): clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title(), first_child_text(), local_name(), main() (+35 more)

### Community 10 - "Community 10"
Cohesion: 0.08
Nodes (41): beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail(), fillBeepBuffer(), initCodec() (+33 more)

### Community 11 - "Community 11"
Cohesion: 0.1
Nodes (43): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_complex_token_costs_more_than_plain_letters(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word(), test_plain_short_word_gets_no_bonus(), test_punctuation_scale_zero_floor_still_applies_some_pause(), test_sentence_end_pauses_longer_than_comma() (+35 more)

### Community 12 - "Community 12"
Cohesion: 0.08
Nodes (32): App, copyOtaLabel(), otaCheckTask(), begin(), InstallFirmware, timeAgo(), loop(), setup() (+24 more)

### Community 13 - "Community 13"
Cohesion: 0.11
Nodes (28): books_dir_for(), candidate_books(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text() (+20 more)

### Community 14 - "Community 14"
Cohesion: 0.1
Nodes (34): applyFocusTimerTouch(), focusTimerCountsLabel(), abandon(), available(), begin(), cancelActiveTimer(), chooseGenre(), clearSession() (+26 more)

### Community 15 - "Community 15"
Cohesion: 0.08
Nodes (19): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, LocalizedError, PendingUploadStoreError, emptyDraft, saveVerificationFailed (+11 more)

### Community 16 - "Community 16"
Cohesion: 0.17
Nodes (17): begin(), beginSdCard(), cardSizeBytes(), configureMsc(), deinitHostIfNeeded(), ejected(), end(), endSdCard() (+9 more)

### Community 17 - "Community 17"
Cohesion: 0.22
Nodes (4): LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver

## Knowledge Gaps
- **19 isolated node(s):** `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle`, `invalidBaseURL` (+14 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 4` to `Community 0`, `Community 1`, `Community 3`, `Community 8`, `Community 12`?**
  _High betweenness centrality (0.131) - this node is a cross-community bridge._
- **Why does `startsWith()` connect `Community 1` to `Community 0`, `Community 4`, `Community 8`, `Community 9`, `Community 13`?**
  _High betweenness centrality (0.081) - this node is a cross-community bridge._
- **Why does `duration()` connect `Community 3` to `Community 1`?**
  _High betweenness centrality (0.046) - this node is a cross-community bridge._
- **Are the 114 inferred relationships involving `String` (e.g. with `duration()` and `test_word_source_streams_words_and_prefetches()`) actually correct?**
  _`String` has 114 INFERRED edges - model-reasoned connections that need verification._
- **Are the 102 inferred relationships involving `c_str()` (e.g. with `test_parses_rss_item_fields()` and `test_parses_atom_entry_with_href_link()`) actually correct?**
  _`c_str()` has 102 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `startsWith()` (e.g. with `write_rsvp()` and `parse_pgm()`) actually correct?**
  _`startsWith()` has 39 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `substring()` (e.g. with `test_key_format_and_prefixes()` and `.replaceNumericEntities()`) actually correct?**
  _`substring()` has 39 INFERRED edges - model-reasoned connections that need verification._