#include <algorithm>
#include <cstdio>

#include "storage/BookProgress.h"

// Pure key derivation + percent math -- no Preferences, no SD, host-testable.
namespace bookprogress {

uint32_t hashPath(const String &path) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < path.length(); ++i) {
    hash ^= static_cast<uint8_t>(path[i]);
    hash *= 16777619UL;
  }
  return hash;
}

namespace {
String keyWithPrefix(char prefix, const String &path) {
  char key[10];
  std::snprintf(key, sizeof(key), "%c%08lx", prefix, static_cast<unsigned long>(hashPath(path)));
  return String(key);
}
}  // namespace

String positionKey(const String &path) { return keyWithPrefix('p', path); }
String wordCountKey(const String &path) { return keyWithPrefix('c', path); }
String recentKey(const String &path) { return keyWithPrefix('r', path); }
String finishedKey(const String &path) { return keyWithPrefix('f', path); }
String bookmarkKey(const String &path) { return keyWithPrefix('k', path); }

std::vector<uint8_t> encodeBookmarks(const std::vector<uint32_t> &wordIndices) {
  std::vector<uint8_t> bytes;
  bytes.reserve(wordIndices.size() * 4);
  for (uint32_t index : wordIndices) {
    bytes.push_back(static_cast<uint8_t>(index & 0xFF));
    bytes.push_back(static_cast<uint8_t>((index >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((index >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((index >> 24) & 0xFF));
  }
  return bytes;
}

std::vector<uint32_t> decodeBookmarks(const uint8_t *bytes, size_t length) {
  std::vector<uint32_t> indices;
  if (bytes == nullptr) {
    return indices;
  }
  const size_t entries = length / 4;  // tolerate a trailing partial entry
  for (size_t i = 0; i < entries && indices.size() < kMaxBookmarks; ++i) {
    const size_t offset = i * 4;
    const uint32_t value = static_cast<uint32_t>(bytes[offset]) |
                           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    indices.push_back(value);
  }
  return indices;
}

bool insertBookmark(std::vector<uint32_t> &wordIndices, uint32_t wordIndex) {
  const auto it = std::lower_bound(wordIndices.begin(), wordIndices.end(), wordIndex);
  if (it != wordIndices.end() && *it == wordIndex) {
    return false;  // exact duplicate
  }
  wordIndices.insert(it, wordIndex);

  if (wordIndices.size() > kMaxBookmarks) {
    // Drop the entry furthest from the one just inserted, keeping marks near
    // the new one. Ties drop the lower-indexed neighbour.
    size_t dropAt = 0;
    uint64_t bestDistance = 0;
    for (size_t i = 0; i < wordIndices.size(); ++i) {
      const uint32_t value = wordIndices[i];
      const uint64_t distance =
          value > wordIndex ? value - wordIndex : wordIndex - value;
      if (distance > bestDistance) {
        bestDistance = distance;
        dropAt = i;
      }
    }
    wordIndices.erase(wordIndices.begin() + dropAt);
  }
  return true;
}

bool progressPercent(uint32_t wordIndex, uint32_t wordCount, uint8_t &percent) {
  if (wordCount <= 1) {
    return false;
  }
  const uint32_t clamped = std::min(wordIndex, wordCount - 1);
  const uint32_t progress = (clamped * 100UL) / (wordCount - 1);
  percent = static_cast<uint8_t>(std::min<uint32_t>(100, progress));
  return true;
}

}  // namespace bookprogress
