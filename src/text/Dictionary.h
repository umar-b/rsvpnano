#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <functional>

// Offline dictionary lookup: word normalization, light stemming fallbacks,
// and binary search over the sorted fixed-size record index that
// tools/build_dict.py writes to /dict/ on the SD card. Pure -- records come
// through a ReadEntryAt callback (a File seek on device, a vector in tests).
// DictionaryStore is the SD adapter.
namespace dict {

constexpr uint32_t kMagic = 0x43494452UL;  // "RDIC"
constexpr size_t kWordBytes = 24;

// On-disk index record, 32 bytes: zero-padded lowercase word, then the
// definition's offset+length in dict.dat.
struct Entry {
  char word[kWordBytes];
  uint32_t offset;
  uint32_t length;
};
static_assert(sizeof(Entry) == 32, "index record layout is on-disk format");

using ReadEntryAt = std::function<bool(size_t index, Entry &out)>;

// Lowercases and trims surrounding punctuation ("Reading," -> "reading").
// Interior hyphens/apostrophes survive. "" when nothing readable remains.
String normalizeLookupWord(const String &raw);

// Binary search over `count` sorted records. On a miss, retries light stem
// variants ('s, s, es, ed, ing). The word must already be normalized.
bool findEntry(size_t count, const ReadEntryAt &readEntryAt, const String &word, Entry &out);

}  // namespace dict
