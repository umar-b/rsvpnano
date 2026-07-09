#include "storage/BookIndex.h"

#include <algorithm>
#include <utility>

namespace bookindex {

Header layoutHeader(uint32_t sourceSize, uint32_t sourceFingerprint, uint32_t wordCount,
                    uint32_t paragraphCount, uint32_t chapterCount, uint32_t dataSize) {
  Header header;
  header.magic = kMagic;
  header.version = kVersion;
  header.headerSize = sizeof(Header);
  header.recordSize = sizeof(WordRecord);
  header.sourceSize = sourceSize;
  header.sourceFingerprint = sourceFingerprint;
  header.wordCount = wordCount;
  header.paragraphCount = paragraphCount;
  header.chapterCount = chapterCount;
  header.recordsOffset = sizeof(Header);
  header.paragraphsOffset = header.recordsOffset + wordCount * sizeof(WordRecord);
  header.chaptersOffset = header.paragraphsOffset + paragraphCount * sizeof(uint32_t);
  header.dataSize = dataSize;
  return header;
}

Builder::Builder(bool rsvpFormat, BookMetadata &metadata, WriteWord writeWord,
                 booktext::MemoryLowFn memoryLow, const BuilderConfig &config)
    : rsvpFormat_(rsvpFormat),
      metadata_(metadata),
      writeWord_(std::move(writeWord)),
      memoryLow_(std::move(memoryLow)),
      config_(config) {
  line_.reserve(256);
}

bool Builder::feed(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length && keepGoing_; ++i) {
    const char c = static_cast<char>(data[i]);

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      keepGoing_ = processLine(line_);
      line_ = "";
      continue;
    }

    line_ += c;
    if (line_.length() >= config_.maxLineChars) {
      keepGoing_ = processLine(line_);
      ++stats_.longLineSplits;
      line_ = "";
    }
  }

  return keepGoing_;
}

bool Builder::finish() {
  if (finished_) {
    return keepGoing_;
  }
  finished_ = true;

  if (!line_.isEmpty() && keepGoing_ && !booktext::reachedBookWordLimit(wordCount_)) {
    keepGoing_ = processLine(line_);
  }
  line_ = "";
  return keepGoing_;
}

bool Builder::pushWord(String token) {
  booktext::trimAsciiWhitespace(token);

  if (token.length() >= 3 && static_cast<uint8_t>(token[0]) == 0xEF &&
      static_cast<uint8_t>(token[1]) == 0xBB && static_cast<uint8_t>(token[2]) == 0xBF) {
    token.remove(0, 3);
  }

  booktext::trimAsciiWhitespace(token);

  if (token.isEmpty() || (!booktext::tokenHasReadableCharacter(token) &&
                          !booktext::isStandaloneRhythmToken(token))) {
    return true;
  }

  if (token.length() > UINT16_MAX ||
      dataSize_ > UINT32_MAX - static_cast<uint32_t>(token.length())) {
    failed_ = true;
    failure_ = "Index limit reached";
    return false;
  }

  if (config_.memoryCheckWordInterval > 0 && wordCount_ > 0 &&
      (wordCount_ % config_.memoryCheckWordInterval) == 0 && memoryLow_ && memoryLow_()) {
    stats_.memoryLow = true;
    failed_ = true;
    failure_ = "Memory limit reached";
    return false;
  }

  WordRecord record;
  record.offset = dataSize_;
  record.length = static_cast<uint16_t>(token.length());
  record.flags = 0;

  if (!writeWord_(record, token.c_str(), token.length())) {
    failed_ = true;
    failure_ = "SD write failed";
    return false;
  }

  dataSize_ += static_cast<uint32_t>(token.length());
  ++wordCount_;
  metadata_.wordCount = wordCount_;
  return true;
}

bool Builder::appendLineWords(const String &line) {
  return booktext::appendTokenizedLineWords(
      line, [&](const String &token) { return pushWord(token); },
      [&]() { return static_cast<size_t>(wordCount_); }, &stats_);
}

void Builder::addChapterMarker(const String &title) {
  if (title.isEmpty()) {
    return;
  }

  ChapterMarker marker;
  marker.title = title;
  marker.wordIndex = wordCount_;

  if (!metadata_.chapters.empty() && metadata_.chapters.back().wordIndex == marker.wordIndex) {
    metadata_.chapters.back() = marker;
    return;
  }

  metadata_.chapters.push_back(marker);
}

void Builder::addParagraphMarker() {
  const size_t wordIndex = wordCount_;
  if (!metadata_.paragraphStarts.empty() && metadata_.paragraphStarts.back() == wordIndex) {
    return;
  }

  metadata_.paragraphStarts.push_back(wordIndex);
}

bool Builder::processLine(const String &line) {
  return rsvpFormat_ ? processRsvpLine(line) : processBookLine(line);
}

bool Builder::processBookLine(const String &line) {
  const String trimmed = booktext::stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending_ = true;
    return true;
  }

  String chapterTitle;
  if (booktext::chapterTitleFromLine(line, chapterTitle)) {
    addChapterMarker(chapterTitle);
    paragraphPending_ = true;
  }

  if (paragraphPending_) {
    addParagraphMarker();
    paragraphPending_ = false;
  }
  return appendLineWords(line);
}

bool Builder::processRsvpLine(const String &line) {
  String trimmed = booktext::stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending_ = true;
    return true;
  }

  if (trimmed.startsWith("@@")) {
    trimmed.remove(0, 1);
    if (paragraphPending_) {
      addParagraphMarker();
      paragraphPending_ = false;
    }
    return appendLineWords(trimmed);
  }

  if (trimmed.startsWith("@")) {
    String lowered = trimmed;
    lowered.toLowerCase();
    if (booktext::prefixHasBoundary(lowered, "@para")) {
      paragraphPending_ = true;
      return true;
    }
    if (booktext::prefixHasBoundary(lowered, "@chapter")) {
      String title = booktext::directiveValue(trimmed, "@chapter");
      if (title.isEmpty()) {
        title = "Chapter";
      }
      addChapterMarker(title);
      paragraphPending_ = true;
      return true;
    }
    if (booktext::prefixHasBoundary(lowered, "@title")) {
      metadata_.title = booktext::directiveValue(trimmed, "@title");
      return true;
    }
    if (booktext::prefixHasBoundary(lowered, "@author")) {
      metadata_.author = booktext::directiveValue(trimmed, "@author");
      return true;
    }
    return true;
  }

  if (paragraphPending_) {
    addParagraphMarker();
    paragraphPending_ = false;
  }
  return appendLineWords(line);
}

}  // namespace bookindex
