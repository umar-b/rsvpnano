#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <functional>

#include "reader/BookContent.h"
#include "text/BookText.h"

// The pure core of the indexed-book (.ridx) build: the on-disk record layout,
// the word/offset accounting, chapter/paragraph marker bookkeeping, and the
// byte-stream line splitter. Record bytes leave through a WriteWord callback
// (SD writes on device, a vector in tests); IndexedBookStore re-exports the
// PODs so the reader and this builder share one format definition.
// StorageManager keeps the SD open/rename dance, progress reporting, and the
// heap check as the adapter.
namespace bookindex {

struct Header {
  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t headerSize = 0;
  uint32_t recordSize = 0;
  uint32_t sourceSize = 0;
  uint32_t sourceFingerprint = 0;
  uint32_t wordCount = 0;
  uint32_t paragraphCount = 0;
  uint32_t chapterCount = 0;
  uint32_t recordsOffset = 0;
  uint32_t paragraphsOffset = 0;
  uint32_t chaptersOffset = 0;
  uint32_t dataSize = 0;
};

struct WordRecord {
  uint32_t offset = 0;
  uint16_t length = 0;
  uint16_t flags = 0;
};

struct ChapterRecord {
  uint32_t wordIndex = 0;
  uint32_t titleLength = 0;
  char title[64] = {};
};

constexpr uint32_t kMagic = 0x58444952UL;  // RIDX
constexpr uint32_t kVersion = 4;

// Fills the section offsets from the final counts (records, then paragraph
// word indexes, then chapter records).
Header layoutHeader(uint32_t sourceSize, uint32_t sourceFingerprint, uint32_t wordCount,
                    uint32_t paragraphCount, uint32_t chapterCount, uint32_t dataSize);

// Persists one word: its index record plus the token bytes for the data file.
// Return false on write failure -- the build stops with "SD write failed".
using WriteWord =
    std::function<bool(const WordRecord &record, const char *bytes, size_t length)>;

struct BuilderConfig {
  // Split (and count) lines longer than this before processing.
  size_t maxLineChars = 4096;
  // Consult memoryLow every N pushed words (0-th word never checked).
  size_t memoryCheckWordInterval = 512;
};

// Streams raw book/.rsvp bytes in via feed(); markers, title/author, and
// word counts accumulate on the caller's BookMetadata while each word goes
// out through writeWord. feed()/finish() return false once the build should
// stop -- either the word limit (failed() == false) or a hard failure
// (failed() == true, reason in failure()).
class Builder {
 public:
  Builder(bool rsvpFormat, BookMetadata &metadata, WriteWord writeWord,
          booktext::MemoryLowFn memoryLow = booktext::MemoryLowFn(),
          const BuilderConfig &config = BuilderConfig());

  bool feed(const uint8_t *data, size_t length);
  bool finish();

  uint32_t wordCount() const { return wordCount_; }
  uint32_t dataSize() const { return dataSize_; }
  bool failed() const { return failed_; }
  const char *failure() const { return failure_; }
  const booktext::ParseStats &stats() const { return stats_; }

 private:
  bool pushWord(String token);
  bool appendLineWords(const String &line);
  bool processBookLine(const String &line);
  bool processRsvpLine(const String &line);
  bool processLine(const String &line);
  void addChapterMarker(const String &title);
  void addParagraphMarker();

  bool rsvpFormat_;
  BookMetadata &metadata_;
  WriteWord writeWord_;
  booktext::MemoryLowFn memoryLow_;
  BuilderConfig config_;
  String line_;
  bool paragraphPending_ = true;
  bool keepGoing_ = true;
  bool finished_ = false;
  uint32_t wordCount_ = 0;
  uint32_t dataSize_ = 0;
  bool failed_ = false;
  const char *failure_ = "";
  booktext::ParseStats stats_;
};

}  // namespace bookindex
