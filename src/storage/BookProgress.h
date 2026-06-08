#pragma once

#include <Arduino.h>

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
// recent-sequence. Keys stay <= 15 chars for the NVS limit.
String positionKey(const String &path);
String wordCountKey(const String &path);
String recentKey(const String &path);

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

 private:
  uint32_t nextSequence();
  Preferences &prefs_;
};
