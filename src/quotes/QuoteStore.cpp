#include "quotes/QuoteStore.h"

#include <FS.h>
#include <SD_MMC.h>

namespace {
constexpr const char *kConfigDir = "/config";
constexpr const char *kQuotesFile = "/config/quotes.jsonl";
}  // namespace

const char *QuoteStore::filePath() { return kQuotesFile; }

std::vector<quotes::Quote> QuoteStore::loadAll() const {
  std::vector<quotes::Quote> records;
  File file = SD_MMC.open(kQuotesFile, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return records;
  }

  String line;
  line.reserve(160);
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\n') {
      quotes::Quote quote;
      if (quotes::parseJsonLine(line, quote)) {
        records.push_back(quote);
      }
      line = "";
      continue;
    }
    if (c != '\r') {
      line += c;
    }
  }
  if (!line.isEmpty()) {
    quotes::Quote quote;
    if (quotes::parseJsonLine(line, quote)) {
      records.push_back(quote);
    }
  }
  file.close();
  return records;
}

bool QuoteStore::contains(const String &bookPath, uint32_t wordIndex) const {
  const std::vector<quotes::Quote> records = loadAll();
  return quotes::containsQuote(records, bookPath, wordIndex);
}

size_t QuoteStore::count() const { return loadAll().size(); }

bool QuoteStore::add(const quotes::Quote &quote, bool *writeFailed) {
  if (writeFailed != nullptr) {
    *writeFailed = false;
  }
  if (contains(quote.bookPath, quote.wordIndex)) {
    return false;  // Already starred -- no-op.
  }

  SD_MMC.mkdir(kConfigDir);
  File file = SD_MMC.open(kQuotesFile, FILE_APPEND);
  if (!file) {
    Serial.printf("[quotes] could not open %s for append\n", kQuotesFile);
    if (writeFailed != nullptr) {
      *writeFailed = true;
    }
    return false;
  }

  file.print(quotes::encodeJsonLine(quote));
  file.print('\n');
  file.close();
  Serial.printf("[quotes] starred word=%u in %s\n",
                static_cast<unsigned int>(quote.wordIndex), quote.bookPath.c_str());
  return true;
}

bool QuoteStore::remove(const String &bookPath, uint32_t wordIndex) {
  std::vector<quotes::Quote> records = loadAll();
  std::vector<quotes::Quote> kept;
  kept.reserve(records.size());
  bool removed = false;
  for (const quotes::Quote &record : records) {
    if (!removed && record.wordIndex == wordIndex && record.bookPath == bookPath) {
      removed = true;
      continue;
    }
    kept.push_back(record);
  }
  if (!removed) {
    return false;
  }
  rewrite(kept);
  return true;
}

bool QuoteStore::rewrite(const std::vector<quotes::Quote> &records) const {
  SD_MMC.mkdir(kConfigDir);
  const String tmpPath = String(kQuotesFile) + ".tmp";
  SD_MMC.remove(tmpPath);
  File file = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!file) {
    Serial.printf("[quotes] could not write %s\n", tmpPath.c_str());
    return false;
  }
  for (const quotes::Quote &record : records) {
    file.print(quotes::encodeJsonLine(record));
    file.print('\n');
  }
  file.close();

  SD_MMC.remove(kQuotesFile);
  if (!SD_MMC.rename(tmpPath, kQuotesFile)) {
    Serial.println("[quotes] rename of quotes file failed");
    return false;
  }
  return true;
}
