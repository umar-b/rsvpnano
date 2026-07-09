#pragma once

#include <Arduino.h>
#include <vector>

#include "reader/BookWordSource.h"
#include "reader/RampIn.h"
#include "reader/WordPacing.h"

class ReadingLoop {
 public:
  // Word-timing heuristics live in the wordpacing module; the reader exposes
  // them under their historical name so callers stay unchanged.
  using PacingConfig = wordpacing::PacingConfig;

  void begin(uint32_t nowMs);
  void start(uint32_t nowMs);
  bool update(uint32_t nowMs, bool allowCatchUp = true);
  void setWords(std::vector<String> words, uint32_t nowMs);
  void setWordSource(BookWordSource *source, uint32_t nowMs);
  void clearLoadedBook(uint32_t nowMs);
  void scrub(int steps);
  void seekTo(size_t wordIndex);
  void seekRelative(size_t baseIndex, int steps);
  void rewindSentence();
  void adjustWpm(int delta);
  void setWpm(uint16_t wpm);
  void setPacingConfig(const PacingConfig &config);
  const PacingConfig &pacingConfig() const;

  // Ramp-in: when enabled, the first words after every start()/resume flash a
  // little slower and ease back to the set WPM. Pure timing geometry lives in
  // the rampin module; the reader only tracks how many words have advanced
  // since the last resume and folds the scale into the word interval.
  void setRampInEnabled(bool enabled);
  bool rampInEnabled() const;

  // Adaptive-pace ease: a temporary interval stretch (1000 = none, 1100 = 10%
  // slower) folded into the word interval like the ramp scale. The set WPM is
  // untouched, so display/persistence/estimates never see the ease.
  void setEaseScalePermille(uint16_t permille);
  uint16_t easeScalePermille() const { return easeScalePermille_; }

  const String &currentWord() const;
  String wordAt(size_t index) const;
  size_t currentIndex() const;
  size_t wordCount() const;
  uint16_t wpm() const;
  uint32_t wordIntervalMs() const;
  uint32_t currentWordDurationMs() const;
  uint32_t wordPacingBonusMsAt(size_t index) const;
  uint32_t elapsedInCurrentWordMs(uint32_t nowMs) const;
  bool currentWordEndsSentence() const;
  // Whether the word at an arbitrary index ends a sentence -- the same boundary
  // convention rewind/star use. Public so quote extraction (App) can reuse it
  // without duplicating the punctuation/lowercase-follow rule.
  bool wordEndsSentenceAt(size_t wordIndex) const;
  bool atEnd() const;

 private:
  bool advance(size_t steps);
  void setCurrentWordFromIndex();
  bool usingLoadedBook() const;
  bool nextWordStartsLowercaseAt(size_t wordIndex) const;
  size_t sentenceStartAtOrBefore(size_t wordIndex) const;

  uint32_t rampScaledIntervalMs() const;

  size_t currentIndex_ = 0;
  uint32_t lastAdvanceMs_ = 0;
  uint16_t wpm_ = 300;
  // Starts "ramp finished" so a reader that has not begun playing (e.g. while
  // paused, scrubbing, or under test) runs at the full set WPM. start() resets
  // this to 0 to begin a fresh ramp on every resume into Playing.
  uint32_t wordsSinceResume_ = rampin::kRampWords;
  bool rampInEnabled_ = true;
  uint16_t easeScalePermille_ = 1000;
  PacingConfig pacingConfig_;
  String currentWord_;
  std::vector<String> loadedWords_;
  BookWordSource *wordSource_ = nullptr;
};
