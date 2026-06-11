#pragma once

#include <Arduino.h>

#include <vector>

#include "quotes/Quote.h"

// SD-backed store of starred sentences (quotes), persisted as JSON lines in
// /config/quotes.jsonl. SD is authoritative -- there are no NVS copies. The
// encode / parse / dedupe rules live in the pure quotes:: module; this class is
// the thin Arduino/SD_MMC wrapper that owns the file, matching how App and the
// companion read their other small SD JSON files.
//
// All reads re-scan the file: the list is small (a handful per reader) and this
// keeps the device and the web companion from disagreeing about an in-RAM copy.
class QuoteStore {
 public:
  // Path of the JSON-lines file. Public so the companion can serve it directly.
  static const char *filePath();

  // Read every well-formed record. Malformed lines are skipped, not fatal.
  // Returns an empty vector when the file is absent.
  std::vector<quotes::Quote> loadAll() const;

  // Append one quote unless an identical (bookPath + wordIndex) record already
  // exists. Returns true when a new line was written, false when it was a
  // duplicate (a no-op) or on write failure (added stays false).
  bool add(const quotes::Quote &quote, bool *writeFailed = nullptr);

  // Remove the record matching bookPath + wordIndex by rewriting the file.
  // Returns true if a record was removed.
  bool remove(const String &bookPath, uint32_t wordIndex);

  // True when a quote for this book path + sentence-start word index exists.
  bool contains(const String &bookPath, uint32_t wordIndex) const;

  size_t count() const;

 private:
  bool rewrite(const std::vector<quotes::Quote> &records) const;
};
