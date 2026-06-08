#include "storage/BookProgress.h"

#include <Preferences.h>

#include <algorithm>

#include "settings/PreferenceKeys.h"

using namespace settings;

void BookProgress::savePosition(const String &path, uint32_t wordIndex, uint32_t wordCount) {
  prefs_.putUInt(bookprogress::positionKey(path).c_str(), wordIndex);
  prefs_.putUInt(bookprogress::wordCountKey(path).c_str(), wordCount);
  prefs_.putUInt(kPrefLegacyWordIndex, wordIndex);
}

void BookProgress::saveWordCount(const String &path, uint32_t wordCount) {
  prefs_.putUInt(bookprogress::wordCountKey(path).c_str(), wordCount);
}

uint32_t BookProgress::readPosition(const String &path, bool allowLegacyFallback) {
  const String key = bookprogress::positionKey(path);
  if (prefs_.isKey(key.c_str())) {
    return prefs_.getUInt(key.c_str(), 0);
  }

  if (allowLegacyFallback && prefs_.isKey(kPrefLegacyWordIndex)) {
    const uint32_t legacyWordIndex = prefs_.getUInt(kPrefLegacyWordIndex, 0);
    prefs_.putUInt(key.c_str(), legacyWordIndex);
    Serial.printf("[progress] migrated legacy position word=%u to key=%s\n",
                  static_cast<unsigned int>(legacyWordIndex), key.c_str());
    return legacyWordIndex;
  }

  return bookprogress::kNoSavedWordIndex;
}

bool BookProgress::progressPercent(const String &path, uint8_t &percent) {
  const String positionKey = bookprogress::positionKey(path);
  const String countKey = bookprogress::wordCountKey(path);
  if (!prefs_.isKey(positionKey.c_str()) || !prefs_.isKey(countKey.c_str())) {
    return false;
  }
  const uint32_t wordCount = prefs_.getUInt(countKey.c_str(), 0);
  const uint32_t wordIndex = prefs_.getUInt(positionKey.c_str(), 0);
  return bookprogress::progressPercent(wordIndex, wordCount, percent);
}

uint32_t BookProgress::nextSequence() {
  uint32_t sequence = prefs_.getUInt(kPrefRecentSeq, 0);
  if (sequence == 0xFFFFFFFEUL) {
    sequence = 0;
  }
  ++sequence;
  prefs_.putUInt(kPrefRecentSeq, sequence);
  return sequence;
}

uint32_t BookProgress::markRecent(const String &path) {
  if (path.isEmpty()) {
    return 0;
  }
  const uint32_t sequence = nextSequence();
  prefs_.putUInt(bookprogress::recentKey(path).c_str(), sequence);
  return sequence;
}

uint32_t BookProgress::recentSequence(const String &path) {
  return prefs_.getUInt(bookprogress::recentKey(path).c_str(), 0);
}

void BookProgress::setFinished(const String &path, bool finished) {
  if (path.isEmpty()) {
    return;
  }
  const String key = bookprogress::finishedKey(path);
  if (finished) {
    prefs_.putBool(key.c_str(), true);
  } else if (prefs_.isKey(key.c_str())) {
    prefs_.remove(key.c_str());
  }
}

bool BookProgress::isFinished(const String &path) {
  const String key = bookprogress::finishedKey(path);
  if (!prefs_.isKey(key.c_str())) {
    return false;
  }
  return prefs_.getBool(key.c_str(), false);
}

std::vector<uint32_t> BookProgress::bookmarks(const String &path) {
  const String key = bookprogress::bookmarkKey(path);
  if (!prefs_.isKey(key.c_str())) {
    return {};
  }
  const size_t length = prefs_.getBytesLength(key.c_str());
  if (length == 0) {
    return {};
  }
  std::vector<uint8_t> buffer(length);
  const size_t read = prefs_.getBytes(key.c_str(), buffer.data(), length);
  return bookprogress::decodeBookmarks(buffer.data(), read);
}

bool BookProgress::addBookmark(const String &path, uint32_t wordIndex) {
  if (path.isEmpty()) {
    return false;
  }
  std::vector<uint32_t> marks = bookmarks(path);
  if (!bookprogress::insertBookmark(marks, wordIndex)) {
    return false;
  }
  const std::vector<uint8_t> bytes = bookprogress::encodeBookmarks(marks);
  prefs_.putBytes(bookprogress::bookmarkKey(path).c_str(), bytes.data(), bytes.size());
  return true;
}

bool BookProgress::removeBookmark(const String &path, uint32_t wordIndex) {
  std::vector<uint32_t> marks = bookmarks(path);
  const auto it = std::find(marks.begin(), marks.end(), wordIndex);
  if (it == marks.end()) {
    return false;
  }
  marks.erase(it);
  const String key = bookprogress::bookmarkKey(path);
  if (marks.empty()) {
    prefs_.remove(key.c_str());
  } else {
    const std::vector<uint8_t> bytes = bookprogress::encodeBookmarks(marks);
    prefs_.putBytes(key.c_str(), bytes.data(), bytes.size());
  }
  return true;
}

void BookProgress::clearBookmarks(const String &path) {
  const String key = bookprogress::bookmarkKey(path);
  if (prefs_.isKey(key.c_str())) {
    prefs_.remove(key.c_str());
  }
}
