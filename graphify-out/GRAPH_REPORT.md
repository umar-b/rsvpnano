# Graph Report - rsvpnano  (2026-06-11)

## Corpus Check
- 171 files · ~1,184,302 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2223 nodes · 6775 edges · 29 communities detected
- Extraction: 69% EXTRACTED · 31% INFERRED · 0% AMBIGUOUS · INFERRED: 2085 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `c_str()` - 164 edges
2. `reserve()` - 57 edges
3. `renderStatus()` - 50 edges
4. `substring()` - 49 edges
5. `currentIndex()` - 48 edges
6. `remove()` - 48 edges
7. `remove()` - 45 edges
8. `begin()` - 45 edges
9. `setState()` - 45 edges
10. `startsWith()` - 43 edges

## Surprising Connections (you probably didn't know these)
- `setup()` --calls--> `begin()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/src/main.cpp → src/sync/CompanionSyncManager.cpp
- `enterPowerOff()` --calls--> `holdBacklightOffForDeepSleep()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/board/BoardConfig.cpp
- `enterSleep()` --calls--> `lightSleepUntilBootButton()`  [INFERRED]
  src/app/App.cpp → /Users/umarb/Developer/rsvpnano-feat-b/src/board/BoardConfig.cpp
- `parse_pgm()` --calls--> `split()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/tools/generate_embedded_serif_font.py → src/reader/WordSplit.cpp
- `zip_join()` --calls--> `split()`  [INFERRED]
  /Users/umarb/Developer/rsvpnano-feat-b/tools/sd_card_converter/convert_books.py → src/reader/WordSplit.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.02
Nodes (335): activateTextEntryButton(), App(), applyAudioSettings(), applyBrowseHoldScroll(), applyBurnInJitter(), applyConsumedCompanionTime(), applyDisplayPreferences(), applyFocusTimerTouch() (+327 more)

### Community 1 - "Community 1"
Cohesion: 0.03
Nodes (207): displayNameForPath(), endsWith(), lastIndexOf(), remove(), reserve(), startsWith(), String(), substring() (+199 more)

### Community 2 - "Community 2"
Cohesion: 0.06
Nodes (147): axs15231bInit(), axs15231bPushColors(), axs15231bSetBacklight(), axs15231bSetBrightnessPercent(), axs15231bSleep(), axs15231bWake(), sendCommand(), setBacklight() (+139 more)

### Community 3 - "Community 3"
Cohesion: 0.03
Nodes (97): ArticleFetchError, articleTooLarge, emptyArticle, invalidURL, batteryPercentForVoltage(), begin(), configureTca9554OutputPin(), disableBatteryAdcPathIfAvailable() (+89 more)

### Community 4 - "Community 4"
Cohesion: 0.03
Nodes (132): c_str(), addBookmark(), bookmarks(), bookprogress(), clearBook(), markRecent(), nextSequence(), progressPercent() (+124 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (94): currentReaderDisplayWord(), resetWordSplitFrame(), replace(), clean_text(), container_rootfile(), directive_text(), extract_events(), fallback_chapter_title() (+86 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (93): App, seedStandbyScreenOff(), seedStandbyScreensaver(), protectionAction(), sampleIntervalMs(), update(), reset(), updateWithSample() (+85 more)

### Community 7 - "Community 7"
Cohesion: 0.05
Nodes (98): beginStatsSession(), handlePreviousSentenceTap(), maybeStartChapterTransition(), rewindToPreviousSentence(), shouldFinalizeReaderPause(), updateWordSplitFrame(), adjustWpm(), advance() (+90 more)

### Community 8 - "Community 8"
Cohesion: 0.05
Nodes (24): ArticleFetchService, ArticleFormatter, SharedArticle, BookDocumentPicker, Coordinator, PickedBookFile, upload(), Array (+16 more)

### Community 9 - "Community 9"
Cohesion: 0.04
Nodes (74): beginBookCelebration(), standbyRngSeed(), jitterOffset(), seed(), advanceRng(), cellAlive(), clearAndStampPattern(), clearRect() (+66 more)

### Community 10 - "Community 10"
Cohesion: 0.08
Nodes (57): achievementName(), applyUnlocks(), isUnlocked(), qualifyingMask(), unlockedCount(), openStatsScreen(), averageWpm(), recordSession() (+49 more)

### Community 11 - "Community 11"
Cohesion: 0.08
Nodes (55): playFocusTimerCompletionCue(), beep(), begin(), configureCodec(), configureCodecSampleFormat(), dacVolumeRegisterValue(), enableAudioRail(), fillBeepBuffer() (+47 more)

### Community 12 - "Community 12"
Cohesion: 0.07
Nodes (24): enterUsbTransfer(), ArticleEditorView, ContentView, saveSettings(), RsvpEvent, chapter, text, begin() (+16 more)

### Community 13 - "Community 13"
Cohesion: 0.1
Nodes (47): defaultConfig(), test_abbreviation_does_not_get_a_sentence_pause(), test_clause_delay_is_independent_of_sentence_delay(), test_clause_delay_scales_semicolon_and_dash_too(), test_complex_token_costs_more_than_plain_letters(), test_default_config_keeps_legacy_combined_behaviour(), test_duration_is_zero_when_base_interval_is_zero(), test_long_word_costs_more_than_short_word() (+39 more)

### Community 14 - "Community 14"
Cohesion: 0.07
Nodes (41): applyMenuTouchGesture(), bookIndexForPickerRow(), handleStarSentenceTap(), isFooterMetricTap(), isPreviousSentenceTap(), isStarSentenceTap(), openBookActions(), absInt() (+33 more)

### Community 15 - "Community 15"
Cohesion: 0.12
Nodes (26): books_dir_for(), clean_text(), cleanup_sidecars(), container_rootfile(), convert_one(), default_root(), directive_text(), epub_events_and_metadata() (+18 more)

### Community 16 - "Community 16"
Cohesion: 0.12
Nodes (25): copyOtaLabel(), otaCheckTask(), InstallFirmware, timeAgo(), checkAndInstall(), checkOnly(), connectWiFi(), currentVersion() (+17 more)

### Community 17 - "Community 17"
Cohesion: 0.12
Nodes (29): export_ota_binary(), find_boot_app0(), git_version(), load_flash_parts(), main(), merge_firmware(), pio_command(), run() (+21 more)

### Community 18 - "Community 18"
Cohesion: 0.13
Nodes (26): focusTimerCountsLabel(), abandon(), available(), begin(), cancelActiveTimer(), chooseGenre(), clearSession(), completeActiveTimer() (+18 more)

### Community 19 - "Community 19"
Cohesion: 0.11
Nodes (19): count(), DvdBounceScreensaver, LifeScreensaver, makeScreensaver(), MazeScreensaver, VoronoiScreensaver, WordRainScreensaver, MemoryStore (+11 more)

### Community 20 - "Community 20"
Cohesion: 0.19
Nodes (21): appendJsonEscaped(), containsQuote(), encodeJsonLine(), extractSentence(), isDigit(), parseJsonLine(), readJsonString(), valueStart() (+13 more)

### Community 21 - "Community 21"
Cohesion: 0.15
Nodes (21): decodeUtf8Codepoint(), isUtf8Continuation(), bytes(), test_decode_utf8_rejects_lone_continuation_byte(), test_decode_utf8_two_byte_sequence(), test_em_dash_entity_expands_to_spaced_hyphen(), test_named_xml_entities(), test_normalize_approximates_utf8_punctuation() (+13 more)

### Community 22 - "Community 22"
Cohesion: 0.24
Nodes (19): finishReadingSprint(), syncSprintPlayingState(), trackFocusWorkBlock(), beginBlock(), enterPlaying(), finishBlock(), leavePlaying(), netForwardWords() (+11 more)

### Community 23 - "Community 23"
Cohesion: 0.2
Nodes (18): epochLooksValid(), epochNowSec(), localDayKey(), localDayKeyNow(), restoreSnapshot(), setReference(), test_clock_invalid_until_set(), test_epoch_now_advances_with_millis() (+10 more)

### Community 24 - "Community 24"
Cohesion: 0.23
Nodes (18): appendDecodedCodepoint(), appendText(), attributeValue(), cleanText(), decodeXmlEntitiesOnce(), decodeXmlEntity(), hexValue(), indexOfIgnoreCase() (+10 more)

### Community 25 - "Community 25"
Cohesion: 0.21
Nodes (14): orpOrdinalForLength(), focusLetterIndex(), layoutWidth(), orpOrdinal(), startX(), test_focus_index_empty_word_is_negative(), test_focus_index_picks_a_letter_inside_the_word(), test_focus_index_skips_leading_punctuation() (+6 more)

### Community 26 - "Community 26"
Cohesion: 0.29
Nodes (8): clampToRange(), inRange(), test_clamp_and_in_range_agree_on_the_same_domain(), test_clamp_pulls_into_domain(), test_in_range_inclusive_of_both_ends(), test_in_range_rejects_outside(), test_index_domains_start_at_zero(), test_negative_domain_clamps_signed()

### Community 27 - "Community 27"
Cohesion: 0.29
Nodes (10): cancel(), clampDisplayX(), clampDisplayY(), clampPhysicalX(), clampPhysicalY(), end(), poll(), readTouchPacket() (+2 more)

### Community 28 - "Community 28"
Cohesion: 0.31
Nodes (9): bookCoverDrift(), bookCoverDriftStep(), test_deterministic(), test_offset_changes_across_interval(), test_offset_constant_within_interval(), test_offset_within_budget(), test_seed_phases_the_cycle(), test_step_advances_once_per_interval() (+1 more)

## Knowledge Gaps
- **20 isolated node(s):** `MemoryStore`, `Array`, `invalidURL`, `articleTooLarge`, `emptyArticle` (+15 more)
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `c_str()` connect `Community 4` to `Community 0`, `Community 1`, `Community 2`, `Community 5`, `Community 7`, `Community 9`, `Community 16`, `Community 19`, `Community 20`, `Community 21`, `Community 24`?**
  _High betweenness centrality (0.127) - this node is a cross-community bridge._
- **Why does `main()` connect `Community 9` to `Community 4`, `Community 5`, `Community 6`, `Community 7`, `Community 10`, `Community 11`, `Community 13`, `Community 14`, `Community 19`, `Community 20`, `Community 21`, `Community 22`, `Community 23`, `Community 25`, `Community 26`, `Community 28`?**
  _High betweenness centrality (0.109) - this node is a cross-community bridge._
- **Why does `currentWordDurationMs()` connect `Community 7` to `Community 0`, `Community 5`, `Community 13`?**
  _High betweenness centrality (0.046) - this node is a cross-community bridge._
- **Are the 157 inferred relationships involving `String` (e.g. with `test_focus_index_skips_leading_punctuation()` and `wordAt()`) actually correct?**
  _`String` has 157 INFERRED edges - model-reasoned connections that need verification._
- **Are the 160 inferred relationships involving `c_str()` (e.g. with `test_bookmark_key_format()` and `test_parses_rss_item_fields()`) actually correct?**
  _`c_str()` has 160 INFERRED edges - model-reasoned connections that need verification._
- **Are the 56 inferred relationships involving `reserve()` (e.g. with `approximateSyllableGroupCount()` and `maskedValue()`) actually correct?**
  _`reserve()` has 56 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `renderStatus()` (e.g. with `setState()` and `handleBatteryProtection()`) actually correct?**
  _`renderStatus()` has 39 INFERRED edges - model-reasoned connections that need verification._