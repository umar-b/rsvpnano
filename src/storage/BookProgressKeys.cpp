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
