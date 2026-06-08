#include "reader/ReadingLoop.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "text/LatinText.h"

namespace {

constexpr const char *kDemoWords[] = {
    "This",        "is",         "the",         "minimal",     "RSVP",
    "demo",        "reader",     "running",     "on",          "the",
    "Waveshare",   "AMOLED",     "board.",

    "Rapid",       "Serial",     "Visual",      "Presentation,", "or",
    "RSVP,",       "is",         "a",           "reading",     "technique",
    "that",        "displays",   "text",        "one",         "word",
    "at",          "a",          "time",        "in",          "a",
    "fixed",       "position",   "on",          "the",         "screen.",
    "Instead",     "of",         "moving",      "your",        "eyes",
    "across",      "lines",      "and",         "paragraphs,", "you",
    "keep",        "your",       "gaze",        "locked",      "on",
    "a",           "single",     "point",       "while",       "words",
    "flash",       "in",         "sequence.",   "This",        "eliminates",
    "saccades,",   "the",        "small",       "rapid",       "eye",
    "movements",   "that",       "consume",     "a",           "surprising",
    "amount",      "of",         "time",        "during",      "traditional",
    "reading.",

    "The",         "concept",    "emerged",     "from",        "cognitive",
    "psychology",  "research",   "in",          "the",         "1970s,",
    "when",        "scientists", "began",       "studying",    "how",
    "quickly",     "the",        "human",       "brain",       "could",
    "process",     "written",    "language.",   "They",        "discovered",
    "that",        "much",       "of",          "the",         "time",
    "spent",       "reading",    "is",          "not",         "actually",
    "spent",       "understanding", "words",    "but",         "rather",
    "physically",  "relocating", "the",         "eyes",        "from",
    "one",         "word",       "to",          "the",         "next.",
    "By",          "removing",   "that",        "mechanical",  "overhead,",
    "readers",     "could",      "absorb",      "text",        "significantly",
    "faster",      "without",    "losing",      "comprehension.",

    "A",           "key",        "element",     "of",          "modern",
    "RSVP",        "readers",    "is",          "the",         "Optimal",
    "Recognition", "Point,",     "or",          "ORP.",        "Every",
    "word",        "has",        "a",           "specific",    "letter",
    "that",        "your",       "brain",       "naturally",   "fixates",
    "on",          "first.",     "For",         "short",       "words",
    "it",          "tends",      "to",          "be",          "near",
    "the",         "beginning,", "for",         "longer",      "words",
    "it",          "shifts",     "slightly",    "toward",      "the",
    "center.",     "By",         "aligning",    "this",        "letter",
    "at",          "a",          "fixed",       "position",    "on",
    "screen,",     "and",        "highlighting", "it,",        "the",
    "reader",      "can",        "recognize",   "each",        "word",
    "faster",      "because",    "the",         "eye",         "does",
    "not",         "need",       "to",          "search",      "for",
    "where",       "to",         "focus.",

    "The",         "speed",      "is",          "measured",    "in",
    "words",       "per",        "minute,",     "or",          "WPM.",
    "Average",     "silent",     "reading",     "speed",       "is",
    "around",      "200",        "to",          "250",         "WPM.",
    "With",        "RSVP,",      "many",        "people",      "comfortably",
    "reach",       "300",        "to",          "500",         "WPM",
    "after",       "a",          "short",       "adjustment",  "period.",
    "Some",        "experienced", "users",      "push",        "beyond",
    "600",         "WPM,",       "though",      "comprehension", "can",
    "start",       "to",         "decline",     "at",          "very",
    "high",        "speeds",     "depending",   "on",          "the",
    "complexity",  "of",         "the",         "material.",

    "Timing",      "is",         "also",        "adaptive.",   "Longer",
    "words",       "stay",       "on",          "screen",      "slightly",
    "longer",      "because",    "they",        "take",        "more",
    "time",        "to",         "process.",    "Words",       "followed",
    "by",          "punctuation", "like",       "commas,",     "periods,",
    "or",          "question",   "marks",       "receive",     "an",
    "extra",       "pause",      "to",          "let",         "the",
    "brain",       "register",   "the",         "end",         "of",
    "a",           "phrase",     "or",          "sentence.",   "This",
    "mimics",      "the",        "natural",     "rhythm",      "of",
    "reading,",    "and",        "prevents",    "the",         "experience",
    "from",        "feeling",    "robotic.",

    "RSVP",        "is",         "particularly", "effective",  "on",
    "mobile",      "devices",    "where",       "screen",      "space",
    "is",          "limited.",   "A",           "single",      "word",
    "at",          "a",          "time",        "needs",       "almost",
    "no",          "horizontal", "space,",      "making",      "it",
    "ideal",       "for",        "phones.",     "There",       "is",
    "no",          "scrolling,", "no",          "page",        "turning,",
    "and",         "no",         "distraction", "from",        "surrounding",
    "text.",       "You",        "simply",      "hold,",       "read,",
    "and",         "let",        "the",         "words",       "come",
    "to",          "you.",
};

constexpr size_t kDemoWordCount = sizeof(kDemoWords) / sizeof(kDemoWords[0]);
constexpr uint16_t kMinWpm = 10;
constexpr uint16_t kLowWpmMax = 100;
constexpr uint16_t kLowWpmStep = 10;
constexpr uint16_t kMaxWpm = 1000;
constexpr uint16_t kHighWpmStep = 25;
constexpr uint8_t kMaxCatchUpWords = 4;


}  // namespace

void ReadingLoop::begin(uint32_t nowMs) {
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::setWords(std::vector<String> words, uint32_t nowMs) {
  wordSource_ = nullptr;
  loadedWords_ = std::move(words);
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::setWordSource(BookWordSource *source, uint32_t nowMs) {
  loadedWords_.clear();
  wordSource_ = source;
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::clearLoadedBook(uint32_t nowMs) {
  wordSource_ = nullptr;
  loadedWords_.clear();
  currentIndex_ = 0;
  lastAdvanceMs_ = nowMs;
  setCurrentWordFromIndex();
}

void ReadingLoop::start(uint32_t nowMs) { lastAdvanceMs_ = nowMs; }

bool ReadingLoop::update(uint32_t nowMs, bool allowCatchUp) {
  bool changed = false;
  const uint8_t maxCatchUpWords = allowCatchUp ? kMaxCatchUpWords : 1;

  for (uint8_t catchUp = 0; catchUp < maxCatchUpWords; ++catchUp) {
    const uint32_t durationMs = currentWordDurationMs();
    if (durationMs == 0 || nowMs - lastAdvanceMs_ < durationMs) {
      break;
    }

    lastAdvanceMs_ += durationMs;
    if (!advance(1)) {
      break;
    }
    changed = true;
  }

  return changed;
}

const String &ReadingLoop::currentWord() const { return currentWord_; }

size_t ReadingLoop::currentIndex() const { return currentIndex_; }

uint16_t ReadingLoop::wpm() const { return wpm_; }

uint32_t ReadingLoop::wordIntervalMs() const { return 60000UL / wpm_; }

uint32_t ReadingLoop::currentWordDurationMs() const {
  bool nextWordStartsLowercase = false;
  const size_t nextIndex = currentIndex_ + 1;
  if (nextIndex < wordCount()) {
    nextWordStartsLowercase = wordpacing::startsWithLowercaseLetter(wordAt(nextIndex));
  } else if (!usingLoadedBook() && nextIndex < kDemoWordCount) {
    nextWordStartsLowercase = wordpacing::startsWithLowercaseLetter(String(kDemoWords[nextIndex]));
  }

  return wordpacing::durationForWord(currentWord_, nextWordStartsLowercase, wordIntervalMs(), pacingConfig_);
}

uint32_t ReadingLoop::wordPacingBonusMsAt(size_t index) const {
  const size_t count = wordCount();
  if (count == 0 || index >= count) {
    return 0;
  }

  const String word = wordAt(index);
  const bool nextLowercase = nextWordStartsLowercaseAt(index);
  return wordpacing::bonusMsForWord(word, nextLowercase, pacingConfig_);
}

uint32_t ReadingLoop::elapsedInCurrentWordMs(uint32_t nowMs) const {
  if (nowMs <= lastAdvanceMs_) {
    return 0;
  }
  return nowMs - lastAdvanceMs_;
}

bool ReadingLoop::currentWordEndsSentence() const {
  return wordEndsSentenceAt(currentIndex_);
}

bool ReadingLoop::atEnd() const {
  const size_t count = wordCount();
  return count == 0 || currentIndex_ + 1 >= count;
}

void ReadingLoop::scrub(int steps) {
  seekRelative(currentIndex_, steps);
}

void ReadingLoop::seekTo(size_t wordIndex) {
  const size_t count = wordCount();
  if (count == 0) {
    currentWord_ = "";
    return;
  }

  if (wordIndex >= count) {
    wordIndex = count - 1;
  }

  currentIndex_ = wordIndex;
  setCurrentWordFromIndex();
}

void ReadingLoop::seekRelative(size_t baseIndex, int steps) {
  const size_t count = wordCount();
  if (count == 0) {
    return;
  }

  if (baseIndex >= count) {
    baseIndex = count - 1;
  }

  int nextIndex = static_cast<int>(baseIndex) + steps;
  if (usingLoadedBook()) {
    if (nextIndex < 0) {
      nextIndex = 0;
    }
    if (nextIndex >= static_cast<int>(count)) {
      nextIndex = static_cast<int>(count) - 1;
    }
  } else {
    nextIndex %= static_cast<int>(count);
    if (nextIndex < 0) {
      nextIndex += static_cast<int>(count);
    }
  }

  currentIndex_ = static_cast<size_t>(nextIndex);
  setCurrentWordFromIndex();
}

void ReadingLoop::rewindSentence() {
  const size_t count = wordCount();
  if (count == 0) {
    return;
  }

  const size_t currentSentenceStart = sentenceStartAtOrBefore(currentIndex_);
  if (currentSentenceStart == currentIndex_ && currentIndex_ > 0) {
    seekTo(sentenceStartAtOrBefore(currentIndex_ - 1));
    return;
  }

  seekTo(currentSentenceStart);
}

void ReadingLoop::adjustWpm(int delta) {
  if (delta == 0) {
    return;
  }

  int nextWpm = static_cast<int>(wpm_);
  if (delta > 0) {
    nextWpm += nextWpm < static_cast<int>(kLowWpmMax) ? kLowWpmStep : kHighWpmStep;
    if (nextWpm > static_cast<int>(kLowWpmMax) &&
        wpm_ < static_cast<uint16_t>(kLowWpmMax)) {
      nextWpm = kLowWpmMax;
    }
  } else {
    nextWpm -= nextWpm <= static_cast<int>(kLowWpmMax) ? kLowWpmStep : kHighWpmStep;
    if (nextWpm < static_cast<int>(kLowWpmMax) &&
        wpm_ > static_cast<uint16_t>(kLowWpmMax)) {
      nextWpm = kLowWpmMax;
    }
  }
  if (nextWpm < static_cast<int>(kMinWpm)) {
    nextWpm = kMinWpm;
  }
  if (nextWpm > static_cast<int>(kMaxWpm)) {
    nextWpm = kMaxWpm;
  }
  wpm_ = static_cast<uint16_t>(nextWpm);
}

void ReadingLoop::setWpm(uint16_t wpm) {
  if (wpm < kMinWpm) {
    wpm = kMinWpm;
  }
  if (wpm > kMaxWpm) {
    wpm = kMaxWpm;
  }
  wpm_ = wpm;
}

void ReadingLoop::setPacingConfig(const PacingConfig &config) {
  pacingConfig_ = wordpacing::clampConfig(config);
}

const ReadingLoop::PacingConfig &ReadingLoop::pacingConfig() const { return pacingConfig_; }

bool ReadingLoop::advance(size_t steps) {
  const size_t count = wordCount();
  if (count == 0) {
    currentWord_ = "";
    return false;
  }

  const size_t previousIndex = currentIndex_;
  if (usingLoadedBook()) {
    const size_t maxIndex = count - 1;
    if (currentIndex_ < maxIndex) {
      const size_t remaining = maxIndex - currentIndex_;
      currentIndex_ += (steps < remaining) ? steps : remaining;
    }
  } else {
    currentIndex_ = (currentIndex_ + steps) % count;
  }

  if (currentIndex_ == previousIndex) {
    return false;
  }

  setCurrentWordFromIndex();
  return true;
}

void ReadingLoop::setCurrentWordFromIndex() {
  if (wordCount() == 0) {
    currentWord_ = "";
    return;
  }

  if (wordSource_ != nullptr) {
    wordSource_->prefetchAround(currentIndex_);
  }
  currentWord_ = wordAt(currentIndex_);
}

size_t ReadingLoop::wordCount() const {
  if (wordSource_ != nullptr) {
    return wordSource_->wordCount();
  }
  if (!loadedWords_.empty()) {
    return loadedWords_.size();
  }
  return kDemoWordCount;
}

String ReadingLoop::wordAt(size_t index) const {
  if (wordSource_ != nullptr) {
    return wordSource_->wordAt(index);
  }
  if (!loadedWords_.empty()) {
    return loadedWords_[index];
  }
  return String(kDemoWords[index]);
}

bool ReadingLoop::usingLoadedBook() const {
  return wordSource_ != nullptr || !loadedWords_.empty();
}

bool ReadingLoop::nextWordStartsLowercaseAt(size_t wordIndex) const {
  const size_t nextIndex = wordIndex + 1;
  if (nextIndex >= wordCount()) {
    return false;
  }

  return wordpacing::startsWithLowercaseLetter(wordAt(nextIndex));
}

bool ReadingLoop::wordEndsSentenceAt(size_t wordIndex) const {
  if (wordIndex >= wordCount()) {
    return false;
  }

  const String word = wordAt(wordIndex);
  if (word.isEmpty()) {
    return false;
  }

  return wordpacing::wordEndsSentence(word, nextWordStartsLowercaseAt(wordIndex));
}

size_t ReadingLoop::sentenceStartAtOrBefore(size_t wordIndex) const {
  const size_t count = wordCount();
  if (count == 0) {
    return 0;
  }

  if (wordIndex >= count) {
    wordIndex = count - 1;
  }

  while (wordIndex > 0) {
    if (wordEndsSentenceAt(wordIndex - 1)) {
      break;
    }
    --wordIndex;
  }

  return wordIndex;
}
