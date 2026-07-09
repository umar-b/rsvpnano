#pragma once

#include <Arduino.h>
#include <FS.h>
#include <cstdint>
#include <vector>

#include "reader/BookWordSource.h"
#include "storage/BookIndex.h"

class IndexedBookStore : public BookWordSource {
 public:
  // The on-disk format lives in the pure bookindex module (shared with the
  // host-tested builder); re-exported here so readers keep their spelling.
  using Header = bookindex::Header;
  using WordRecord = bookindex::WordRecord;
  using ChapterRecord = bookindex::ChapterRecord;

  static constexpr uint32_t kMagic = bookindex::kMagic;
  static constexpr uint32_t kVersion = bookindex::kVersion;
  static constexpr size_t kWordCacheSize = 256;

  IndexedBookStore() = default;
  IndexedBookStore(const IndexedBookStore &) = delete;
  IndexedBookStore &operator=(const IndexedBookStore &) = delete;

  bool open(const String &indexPath, const String &dataPath, const Header &header);
  void close();
  bool isOpen() const;

  size_t wordCount() const override;
  String wordAt(size_t index) const override;
  void prefetchAround(size_t index) const override;

  const String &indexPath() const { return indexPath_; }
  const String &dataPath() const { return dataPath_; }

 private:
  bool loadWordWindow(size_t index) const;
  bool readRecords(size_t startIndex, size_t count, std::vector<WordRecord> &records) const;

  String indexPath_;
  String dataPath_;
  Header header_;
  mutable File indexFile_;
  mutable File dataFile_;
  mutable std::vector<String> cachedWords_;
  mutable size_t cachedStart_ = static_cast<size_t>(-1);
  mutable size_t cachedCount_ = 0;
};
