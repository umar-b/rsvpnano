#include "storage/DictionaryStore.h"

#include <SD_MMC.h>

namespace {

constexpr const char *kIndexPath = "/dict/dict.idx";
constexpr const char *kDataPath = "/dict/dict.dat";
constexpr size_t kHeaderBytes = 8;  // magic + count
constexpr size_t kMaxDefinitionBytes = 512;

}  // namespace

bool DictionaryStore::begin() {
  end();

  index_ = SD_MMC.open(kIndexPath, FILE_READ);
  if (!index_ || index_.isDirectory()) {
    end();
    return false;
  }

  uint8_t header[kHeaderBytes];
  if (index_.read(header, sizeof(header)) != sizeof(header)) {
    end();
    return false;
  }
  const uint32_t magic = static_cast<uint32_t>(header[0]) | (static_cast<uint32_t>(header[1]) << 8) |
                         (static_cast<uint32_t>(header[2]) << 16) |
                         (static_cast<uint32_t>(header[3]) << 24);
  const uint32_t count = static_cast<uint32_t>(header[4]) | (static_cast<uint32_t>(header[5]) << 8) |
                         (static_cast<uint32_t>(header[6]) << 16) |
                         (static_cast<uint32_t>(header[7]) << 24);
  if (magic != dict::kMagic || count == 0 ||
      index_.size() < kHeaderBytes + static_cast<size_t>(count) * sizeof(dict::Entry)) {
    Serial.printf("[dict] bad index magic=0x%08lx count=%lu\n", static_cast<unsigned long>(magic),
                  static_cast<unsigned long>(count));
    end();
    return false;
  }

  data_ = SD_MMC.open(kDataPath, FILE_READ);
  if (!data_ || data_.isDirectory()) {
    end();
    return false;
  }

  count_ = count;
  Serial.printf("[dict] ready: %lu words\n", static_cast<unsigned long>(count_));
  return true;
}

void DictionaryStore::end() {
  if (index_) {
    index_.close();
  }
  if (data_) {
    data_.close();
  }
  count_ = 0;
}

bool DictionaryStore::readEntryAt(size_t index, dict::Entry &out) {
  if (!index_.seek(kHeaderBytes + index * sizeof(dict::Entry))) {
    return false;
  }
  return index_.read(reinterpret_cast<uint8_t *>(&out), sizeof(out)) == sizeof(out);
}

String DictionaryStore::lookup(const String &displayWord) {
  if (count_ == 0) {
    return "";
  }

  const String word = dict::normalizeLookupWord(displayWord);
  dict::Entry entry;
  if (!dict::findEntry(
          count_, [this](size_t index, dict::Entry &out) { return readEntryAt(index, out); },
          word, entry)) {
    return "";
  }

  const size_t length = std::min(static_cast<size_t>(entry.length), kMaxDefinitionBytes);
  if (!data_.seek(entry.offset)) {
    return "";
  }
  String definition;
  definition.reserve(length);
  for (size_t i = 0; i < length && data_.available(); ++i) {
    definition += static_cast<char>(data_.read());
  }
  return definition;
}
