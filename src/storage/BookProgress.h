#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class Preferences;

// Per-book reading progress persisted in NVS, keyed by a hash of the book's
// path. The device reader (App) and the web companion (CompanionSyncManager)
// both read and write this; before this module each kept its own copy of the
// key derivation and percent math, which had to agree byte-for-byte but didn't
// share code. The pure key/percent functions below live in BookProgressKeys.cpp
// (no Preferences dependency) so they can be unit tested on the host.
namespace bookprogress {

constexpr uint32_t kNoSavedWordIndex = 0xFFFFFFFFUL;

// FNV-1a over the path bytes -- the basis for every per-book key.
uint32_t hashPath(const String &path);

// NVS keys for a book: "p<hash>" position, "c<hash>" word count, "r<hash>"
// recent-sequence, "f<hash>" finished flag. Keys stay <= 15 chars for the NVS
// limit.
String positionKey(const String &path);
String wordCountKey(const String &path);
String recentKey(const String &path);
String finishedKey(const String &path);
// "k<hash>" -- the packed bookmark blob for a book (a sequence of word indices).
String bookmarkKey(const String &path);
// "w<hash>" -- the per-book WPM override (the reading speed the user last set
// while this book was open). Distinct from the global kPrefWpm fallback.
String wpmKey(const String &path);

// Sentinel for "this book has no WPM override saved". 0 is never a valid WPM
// (the reader floor is 10), so it doubles as the absent marker.
constexpr uint16_t kNoSavedWpm = 0;

// Restore priority: a book's saved WPM override when present, else the global
// fallback. Pure so the priority rule is testable without NVS.
uint16_t resolveBookWpm(uint16_t savedBookWpm, uint16_t globalFallbackWpm);

// Maximum bookmarks kept per book. New marks past the cap drop the oldest.
constexpr size_t kMaxBookmarks = 16;

// Pure encode/decode for the packed bookmark blob: each word index is stored as
// 4 little-endian bytes, in order. decodeBookmarks tolerates trailing partial
// bytes (returns whole entries only) and caps the result at kMaxBookmarks.
std::vector<uint8_t> encodeBookmarks(const std::vector<uint32_t> &wordIndices);
std::vector<uint32_t> decodeBookmarks(const uint8_t *bytes, size_t length);

// Insert a word index into a bookmark list: ignores exact duplicates, keeps the
// list sorted ascending, and enforces kMaxBookmarks by dropping the entry
// furthest (by word distance) from the inserted one. Returns true if it changed.
bool insertBookmark(std::vector<uint32_t> &wordIndices, uint32_t wordIndex);

// Resume progress as 0..100. Returns false when there isn't enough to show
// (word count of 0 or 1).
bool progressPercent(uint32_t wordIndex, uint32_t wordCount, uint8_t &percent);

}  // namespace bookprogress

// Stateful store over an already-opened Preferences (NVS namespace owned by the
// caller). Thin wrapper around the pure key functions above.
class BookProgress {
 public:
  explicit BookProgress(Preferences &prefs) : prefs_(prefs) {}

  // Save the current position and the book's word count. Also refreshes the
  // legacy single-slot index for backward compatibility.
  void savePosition(const String &path, uint32_t wordIndex, uint32_t wordCount);
  void saveWordCount(const String &path, uint32_t wordCount);

  // Per-book WPM override: the speed the user chose while this book was open.
  // saveWpm stores it; readWpm returns it or kNoSavedWpm when none is stored.
  void saveWpm(const String &path, uint16_t wpm);
  uint16_t readWpm(const String &path);

  // Saved word index for the book, or kNoSavedWordIndex if none. When
  // allowLegacyFallback is set and no per-book key exists, migrates the legacy
  // single-slot index into the per-book key and returns it.
  uint32_t readPosition(const String &path, bool allowLegacyFallback = false);

  // Resume progress for a book read straight from NVS. False when unavailable.
  bool progressPercent(const String &path, uint8_t &percent);

  // Recent-book ordering via a monotonic sequence. markRecent stamps the book
  // with the next sequence and returns it.
  uint32_t markRecent(const String &path);
  uint32_t recentSequence(const String &path);

  // Finished flag (orthogonal to saved position -- marking finished never
  // resets the position, so re-reads resume where you were). setFinished(false)
  // clears the flag.
  void setFinished(const String &path, bool finished);
  bool isFinished(const String &path);

  // Bookmarks: bare word indices saved per book, independent of the resume
  // position. Stored as one packed blob under the "k<hash>" key. The list stays
  // sorted ascending and capped at kMaxBookmarks.
  std::vector<uint32_t> bookmarks(const String &path);
  bool addBookmark(const String &path, uint32_t wordIndex);     // false if no change
  bool removeBookmark(const String &path, uint32_t wordIndex);  // false if absent
  void clearBookmarks(const String &path);

 private:
  uint32_t nextSequence();
  Preferences &prefs_;
};
