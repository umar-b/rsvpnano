#pragma once

#include <Arduino.h>
#include <FS.h>

#include "text/Dictionary.h"

// SD adapter over the pure dict:: lookup: opens /dict/dict.idx + dict.dat
// (written by tools/build_dict.py), validates the header, and answers word
// lookups with the definition text. Absent files just mean available() stays
// false -- the dictionary is an optional card asset.
class DictionaryStore {
 public:
  bool begin();
  void end();
  bool available() const { return count_ > 0; }
  uint32_t wordCount() const { return count_; }

  // Definition for a display word (normalization + stem fallbacks applied),
  // or "" when the word is unknown / the dictionary is absent.
  String lookup(const String &displayWord);

 private:
  bool readEntryAt(size_t index, dict::Entry &out);

  File index_;
  File data_;
  uint32_t count_ = 0;
};
