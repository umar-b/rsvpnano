#include "text/Dictionary.h"

#include <cctype>
#include <cstring>

namespace dict {

namespace {

bool isWordChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '\'';
}

int compareWord(const Entry &entry, const String &word) {
  // entry.word is zero-padded; compare as a bounded C string.
  char bounded[kWordBytes + 1];
  std::memcpy(bounded, entry.word, kWordBytes);
  bounded[kWordBytes] = '\0';
  return std::strcmp(bounded, word.c_str());
}

bool searchExact(size_t count, const ReadEntryAt &readEntryAt, const String &word, Entry &out) {
  if (word.isEmpty() || word.length() > kWordBytes) {
    return false;
  }

  size_t low = 0;
  size_t high = count;
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    Entry entry;
    if (!readEntryAt(mid, entry)) {
      return false;
    }
    const int cmp = compareWord(entry, word);
    if (cmp == 0) {
      out = entry;
      return true;
    }
    if (cmp < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return false;
}

}  // namespace

String normalizeLookupWord(const String &raw) {
  int begin = 0;
  int end = static_cast<int>(raw.length());
  while (begin < end && !isWordChar(raw[begin])) {
    ++begin;
  }
  while (end > begin && !isWordChar(raw[end - 1])) {
    --end;
  }

  String word;
  word.reserve(end - begin);
  for (int i = begin; i < end; ++i) {
    word += static_cast<char>(std::tolower(static_cast<unsigned char>(raw[i])));
  }
  return word;
}

bool findEntry(size_t count, const ReadEntryAt &readEntryAt, const String &word, Entry &out) {
  if (searchExact(count, readEntryAt, word, out)) {
    return true;
  }

  // Light stemming: enough for plurals and common inflections without a
  // real stemmer. Each candidate is a fresh binary search.
  const size_t length = word.length();
  if (length >= 3 && word.endsWith("'s")) {
    return searchExact(count, readEntryAt, word.substring(0, length - 2), out);
  }
  if (length >= 4 && word.endsWith("ies")) {
    if (searchExact(count, readEntryAt, word.substring(0, length - 3) + "y", out)) {
      return true;
    }
  }
  if (length >= 4 && word.endsWith("es") &&
      searchExact(count, readEntryAt, word.substring(0, length - 2), out)) {
    return true;
  }
  if (length >= 3 && word.endsWith("s") && !word.endsWith("ss") &&
      searchExact(count, readEntryAt, word.substring(0, length - 1), out)) {
    return true;
  }
  if (length >= 4 && word.endsWith("ed")) {
    if (searchExact(count, readEntryAt, word.substring(0, length - 2), out) ||
        searchExact(count, readEntryAt, word.substring(0, length - 1), out)) {
      return true;
    }
  }
  if (length >= 5 && word.endsWith("ing")) {
    if (searchExact(count, readEntryAt, word.substring(0, length - 3), out) ||
        searchExact(count, readEntryAt, word.substring(0, length - 3) + "e", out)) {
      return true;
    }
  }
  return false;
}

}  // namespace dict
