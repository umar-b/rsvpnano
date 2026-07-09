#include "storage/EpubConverter.h"

#include <SD_MMC.h>
#include <algorithm>
#include <cstring>
#include <esp32s3/rom/miniz.h>
#include <esp_heap_caps.h>
#include <vector>

#include "text/EpubConvert.h"

// SD-card/zip adapter around the pure epubconvert:: kernel: archive walking,
// miniz inflate, progress reporting, and the temp-file/marker dance live
// here; the XHTML -> .rsvp conversion itself is host-tested in the module.
namespace {

constexpr uint32_t kZipEocdSignature = 0x06054B50UL;
constexpr uint32_t kZipCentralFileSignature = 0x02014B50UL;
constexpr uint32_t kZipLocalFileSignature = 0x04034B50UL;
constexpr uint16_t kZipStored = 0;
constexpr uint16_t kZipDeflated = 8;
constexpr size_t kZipEocdMaxSearch = 66UL * 1024UL;
constexpr size_t kMaxOpfBytes = 256UL * 1024UL;
constexpr size_t kMaxContainerBytes = 32UL * 1024UL;
constexpr uint16_t kMaxZipEntries = 2048;
constexpr uint16_t kMaxZipNameLength = 512;
constexpr size_t kReadChunkBytes = 4096;
constexpr size_t kInflateInputChunkBytes = 4096;
constexpr const char *kConverterVersion = "stream-v6";

enum class ContentExtractStatus {
  Complete,
  WordLimitReached,
  Unsupported,
  Failed,
};

struct ZipEntry {
  String name;
  uint16_t method = 0;
  uint16_t flags = 0;
  uint32_t compressedSize = 0;
  uint32_t uncompressedSize = 0;
  uint32_t localHeaderOffset = 0;
};

uint16_t readLe16(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

void serviceBackground() {
  yield();
  delay(0);
}

bool readExact(File &file, uint8_t *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const size_t chunk = std::min(kReadChunkBytes, length - offset);
    const uint32_t beforePosition = static_cast<uint32_t>(file.position());
    const int bytesRead = file.read(buffer + offset, chunk);
    if (bytesRead != static_cast<int>(chunk)) {
      Serial.printf(
          "[epub-zip] Short read at pos=%lu wanted=%u got=%d totalWanted=%u offset=%u\n",
          static_cast<unsigned long>(beforePosition), static_cast<unsigned int>(chunk),
          bytesRead, static_cast<unsigned int>(length), static_cast<unsigned int>(offset));
      return false;
    }
    offset += chunk;
    serviceBackground();
  }

  return true;
}

void reportProgress(const EpubConverter::Options &options, const char *line1, const char *line2,
                    int progressPercent) {
  if (options.progressCallback == nullptr) {
    return;
  }

  progressPercent = std::max(0, std::min(100, progressPercent));
  options.progressCallback(options.progressContext, line1, line2, progressPercent);
  serviceBackground();
}

void *allocateBuffer(size_t bytes) {
  if (bytes == 0) {
    return nullptr;
  }

  void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (buffer == nullptr) {
    buffer = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return buffer;
}

void *allocateInternalBuffer(size_t bytes) {
  if (bytes == 0) {
    return nullptr;
  }

  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void freeBuffer(void *buffer) {
  if (buffer != nullptr) {
    heap_caps_free(buffer);
  }
}

String toLowerCopy(String value) {
  value.toLowerCase();
  return value;
}

bool isArchiveHintEntry(const String &name) {
  const String lowered = toLowerCopy(name);
  return lowered.indexOf("container") >= 0 || lowered.endsWith(".opf") ||
         lowered.endsWith(".ncx") || lowered.endsWith(".xhtml") ||
         lowered.endsWith(".html") || lowered.endsWith(".htm");
}

void reportContentProgress(const EpubConverter::Options &options, size_t itemIndex,
                           size_t itemCount, uint32_t bytesRead, uint32_t totalBytes,
                           size_t wordCount) {
  if (itemCount == 0 || totalBytes == 0) {
    return;
  }

  const uint32_t cappedBytes = std::min(bytesRead, totalBytes);
  const int contentPercent = static_cast<int>((cappedBytes * 100ULL) / totalBytes);
  const int itemPercent = static_cast<int>(((itemIndex * 100ULL) + contentPercent) / itemCount);
  const int progressPercent = 25 + ((itemPercent * 70) / 100);
  const String detail = String(itemIndex + 1) + "/" + String(itemCount) + " " +
                        String(wordCount) + " words";
  reportProgress(options, "Extracting content", detail.c_str(), progressPercent);
}

class ZipArchive {
 public:
  bool open(const String &path) {
    archivePath_ = path;
    file_ = SD_MMC.open(path);
    if (!file_ || file_.isDirectory()) {
      Serial.printf("[epub-zip] Open failed: %s\n", path.c_str());
      close();
      return false;
    }

    Serial.printf("[epub-zip] Opened archive: %s size=%lu\n", path.c_str(),
                  static_cast<unsigned long>(file_.size()));
    if (!readCentralDirectory()) {
      Serial.printf("[epub-zip] Central directory read failed: %s\n", path.c_str());
      close();
      return false;
    }
    Serial.printf("[epub-zip] Archive ready: %u file entries\n",
                  static_cast<unsigned int>(entries_.size()));
    logArchiveHints("open");
    return true;
  }

  void close() {
    if (file_) {
      file_.close();
    }
    entries_.clear();
  }

  const ZipEntry *find(const String &name) const {
    const String normalized = epubconvert::normalizeZipName(name);
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].name == normalized) {
        return &entries_[i];
      }
    }

    const String lowered = toLowerCopy(normalized);
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (toLowerCopy(entries_[i].name) == lowered) {
        Serial.printf("[epub-zip] Case-insensitive ZIP match: requested=%s actual=%s\n",
                      normalized.c_str(), entries_[i].name.c_str());
        return &entries_[i];
      }
    }

    Serial.printf("[epub-zip] Entry not found: %s\n", normalized.c_str());
    logArchiveHints("missing entry");
    return nullptr;
  }

  bool extractToString(const String &name, String &output, size_t maxBytes) {
    Serial.printf("[epub-zip] Request string entry: %s\n", name.c_str());
    Serial.flush();
    const ZipEntry *entry = find(name);
    if (entry == nullptr) {
      return false;
    }
    return extractToString(*entry, output, maxBytes);
  }

  ContentExtractStatus extractContentToRsvp(const String &name, File &output, size_t &wordCount,
                                            size_t maxWords, String &lastChapterTitle,
                                            const EpubConverter::Options &options,
                                            size_t itemIndex, size_t itemCount) {
    const ZipEntry *entry = find(name);
    if (entry == nullptr) {
      Serial.printf("[epub-zip] Content entry not found: %s\n", name.c_str());
      return ContentExtractStatus::Failed;
    }
    return extractContentToRsvp(*entry, output, wordCount, maxWords, lastChapterTitle, options,
                                itemIndex, itemCount);
  }

 private:
  void logArchiveHints(const char *reason) const {
    Serial.printf("[epub-zip] Archive hints (%s): entries=%u\n",
                  reason == nullptr ? "" : reason, static_cast<unsigned int>(entries_.size()));

    size_t printed = 0;
    for (size_t i = 0; i < entries_.size() && printed < 10; ++i) {
      Serial.printf("[epub-zip]   entry[%u] %s method=%u flags=0x%04x c=%lu u=%lu local=%lu\n",
                    static_cast<unsigned int>(i), entries_[i].name.c_str(), entries_[i].method,
                    entries_[i].flags, static_cast<unsigned long>(entries_[i].compressedSize),
                    static_cast<unsigned long>(entries_[i].uncompressedSize),
                    static_cast<unsigned long>(entries_[i].localHeaderOffset));
      ++printed;
    }

    size_t hinted = 0;
    for (size_t i = 0; i < entries_.size() && hinted < 20; ++i) {
      if (!isArchiveHintEntry(entries_[i].name)) {
        continue;
      }
      Serial.printf("[epub-zip]   hint[%u] %s method=%u flags=0x%04x c=%lu u=%lu local=%lu\n",
                    static_cast<unsigned int>(i), entries_[i].name.c_str(), entries_[i].method,
                    entries_[i].flags, static_cast<unsigned long>(entries_[i].compressedSize),
                    static_cast<unsigned long>(entries_[i].uncompressedSize),
                    static_cast<unsigned long>(entries_[i].localHeaderOffset));
      ++hinted;
    }
  }

  bool readCentralDirectory() {
    const uint32_t fileSize = static_cast<uint32_t>(file_.size());
    if (fileSize < 22) {
      Serial.printf("[epub-zip] File too small for ZIP EOCD: %lu\n",
                    static_cast<unsigned long>(fileSize));
      return false;
    }

    const size_t tailSize =
        fileSize < kZipEocdMaxSearch ? static_cast<size_t>(fileSize) : kZipEocdMaxSearch;
    uint8_t *tail = static_cast<uint8_t *>(allocateBuffer(tailSize));
    if (tail == nullptr) {
      Serial.printf("[epub-zip] No memory for EOCD tail buffer: %u bytes\n",
                    static_cast<unsigned int>(tailSize));
      return false;
    }

    const uint32_t tailOffset = fileSize - static_cast<uint32_t>(tailSize);
    Serial.printf("[epub-zip] Searching EOCD: fileSize=%lu tailOffset=%lu tailSize=%u\n",
                  static_cast<unsigned long>(fileSize), static_cast<unsigned long>(tailOffset),
                  static_cast<unsigned int>(tailSize));
    bool ok = file_.seek(tailOffset) && readExact(file_, tail, tailSize);
    int eocdIndex = -1;
    if (ok) {
      for (int i = static_cast<int>(tailSize) - 22; i >= 0; --i) {
        if (readLe32(tail + i) == kZipEocdSignature) {
          eocdIndex = i;
          break;
        }
      }
    }

    if (eocdIndex < 0) {
      Serial.printf("[epub-zip] EOCD signature not found (tailRead=%s)\n", ok ? "yes" : "no");
      freeBuffer(tail);
      return false;
    }

    const uint16_t diskNumber = readLe16(tail + eocdIndex + 4);
    const uint16_t directoryDisk = readLe16(tail + eocdIndex + 6);
    const uint16_t entryCount = readLe16(tail + eocdIndex + 10);
    const uint32_t centralDirectoryOffset = readLe32(tail + eocdIndex + 16);
    const uint32_t centralDirectorySize = readLe32(tail + eocdIndex + 12);
    freeBuffer(tail);

    Serial.printf(
        "[epub-zip] EOCD found: eocdOffset=%lu entries=%u cdOffset=%lu cdSize=%lu disk=%u "
        "dirDisk=%u\n",
        static_cast<unsigned long>(tailOffset + static_cast<uint32_t>(eocdIndex)), entryCount,
        static_cast<unsigned long>(centralDirectoryOffset),
        static_cast<unsigned long>(centralDirectorySize), diskNumber, directoryDisk);

    if (diskNumber != 0 || directoryDisk != 0 || entryCount == 0 ||
        entryCount > kMaxZipEntries) {
      Serial.printf("[epub] Unsupported ZIP directory entry count: %u\n", entryCount);
      return false;
    }

    entries_.clear();
    entries_.reserve(entryCount);
    if (!file_.seek(centralDirectoryOffset)) {
      Serial.printf("[epub-zip] Could not seek to central directory offset=%lu\n",
                    static_cast<unsigned long>(centralDirectoryOffset));
      return false;
    }

    for (uint16_t i = 0; i < entryCount; ++i) {
      if ((i & 0x1F) == 0) {
        serviceBackground();
      }

      uint8_t header[46];
      if (!readExact(file_, header, sizeof(header)) ||
          readLe32(header) != kZipCentralFileSignature) {
        Serial.printf("[epub-zip] Bad central header at index=%u pos=%lu\n", i,
                      static_cast<unsigned long>(file_.position()));
        return false;
      }

      const uint16_t fileNameLength = readLe16(header + 28);
      const uint16_t extraLength = readLe16(header + 30);
      const uint16_t commentLength = readLe16(header + 32);
      if (fileNameLength == 0 || fileNameLength > kMaxZipNameLength) {
        Serial.printf("[epub] Unsupported ZIP filename length: %u\n", fileNameLength);
        return false;
      }

      char *nameBuffer = static_cast<char *>(allocateBuffer(fileNameLength + 1));
      if (nameBuffer == nullptr) {
        Serial.printf("[epub-zip] No memory for filename buffer: %u bytes\n", fileNameLength + 1);
        return false;
      }

      const bool nameRead =
          readExact(file_, reinterpret_cast<uint8_t *>(nameBuffer), fileNameLength);
      nameBuffer[fileNameLength] = '\0';

      ZipEntry entry;
      entry.name = epubconvert::normalizeZipName(String(nameBuffer));
      entry.method = readLe16(header + 10);
      entry.flags = readLe16(header + 8);
      entry.compressedSize = readLe32(header + 20);
      entry.uncompressedSize = readLe32(header + 24);
      entry.localHeaderOffset = readLe32(header + 42);
      freeBuffer(nameBuffer);

      if (!nameRead) {
        return false;
      }

      const uint32_t nextPosition =
          static_cast<uint32_t>(file_.position()) + extraLength + commentLength;
      if (!file_.seek(nextPosition)) {
        Serial.printf("[epub-zip] Could not seek past central extras for %s next=%lu\n",
                      entry.name.c_str(), static_cast<unsigned long>(nextPosition));
        return false;
      }

      if (!entry.name.endsWith("/")) {
        entries_.push_back(entry);
      }
    }

    Serial.printf("[epub-zip] Central directory parsed: kept=%u rawEntries=%u\n",
                  static_cast<unsigned int>(entries_.size()), entryCount);
    return true;
  }

  bool extractToString(const ZipEntry &entry, String &output, size_t maxBytes) {
    output = "";

    Serial.printf("[epub-zip] Extract string: %s method=%u flags=0x%04x c=%lu u=%lu max=%u\n",
                  entry.name.c_str(), entry.method, entry.flags,
                  static_cast<unsigned long>(entry.compressedSize),
                  static_cast<unsigned long>(entry.uncompressedSize),
                  static_cast<unsigned int>(maxBytes));
    Serial.flush();

    if (entry.uncompressedSize == 0 || entry.uncompressedSize > maxBytes ||
        entry.compressedSize == 0 || entry.compressedSize > maxBytes) {
      Serial.printf("[epub] Skipping %s (%lu compressed, %lu uncompressed bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.compressedSize),
                    static_cast<unsigned long>(entry.uncompressedSize));
      return false;
    }

    uint8_t localHeader[30];
    if (!file_.seek(entry.localHeaderOffset)) {
      Serial.printf("[epub-zip] Could not seek to local header: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.localHeaderOffset));
      return false;
    }
    if (!readExact(file_, localHeader, sizeof(localHeader))) {
      Serial.printf("[epub-zip] Could not read local header: %s\n", entry.name.c_str());
      return false;
    }
    const uint32_t localSignature = readLe32(localHeader);
    if (localSignature != kZipLocalFileSignature) {
      Serial.printf("[epub-zip] Bad local signature for %s signature=0x%08lx\n",
                    entry.name.c_str(), static_cast<unsigned long>(localSignature));
      return false;
    }

    const uint16_t fileNameLength = readLe16(localHeader + 26);
    const uint16_t extraLength = readLe16(localHeader + 28);
    const uint32_t dataOffset = entry.localHeaderOffset + sizeof(localHeader) + fileNameLength +
                                extraLength;
    Serial.printf("[epub-zip] Local data: %s nameLen=%u extraLen=%u dataOffset=%lu\n",
                  entry.name.c_str(), fileNameLength, extraLength,
                  static_cast<unsigned long>(dataOffset));
    if (!file_.seek(dataOffset)) {
      Serial.printf("[epub-zip] Could not seek to data: %s offset=%lu\n", entry.name.c_str(),
                    static_cast<unsigned long>(dataOffset));
      return false;
    }

    if (!output.reserve(static_cast<unsigned int>(entry.uncompressedSize + 1))) {
      Serial.printf("[epub-zip] No memory to reserve string for %s (%lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.uncompressedSize));
      return false;
    }

    bool ok = false;
    uint32_t totalOutputBytes = 0;

    auto appendBytes = [&](const uint8_t *data, size_t length) -> bool {
      if (length == 0) {
        return true;
      }
      if (totalOutputBytes + length > maxBytes) {
        Serial.printf("[epub-zip] String extraction exceeded limit for %s\n",
                      entry.name.c_str());
        return false;
      }
      if (!output.concat(reinterpret_cast<const char *>(data), static_cast<unsigned int>(length))) {
        Serial.printf("[epub-zip] String append failed for %s length=%u\n", entry.name.c_str(),
                      static_cast<unsigned int>(length));
        return false;
      }
      totalOutputBytes += static_cast<uint32_t>(length);
      return true;
    };

    if (entry.method == kZipStored) {
      Serial.printf("[epub-zip] Reading stored string payload: %s\n", entry.name.c_str());
      uint8_t *buffer = static_cast<uint8_t *>(allocateInternalBuffer(kReadChunkBytes));
      if (buffer == nullptr) {
        Serial.printf("[epub-zip] No internal buffer for stored string: %s\n",
                      entry.name.c_str());
        return false;
      }

      uint32_t remaining = entry.uncompressedSize;
      ok = true;
      while (remaining > 0) {
        const size_t chunk = std::min(kReadChunkBytes, static_cast<size_t>(remaining));
        if (!readExact(file_, buffer, chunk) || !appendBytes(buffer, chunk)) {
          ok = false;
          break;
        }
        remaining -= static_cast<uint32_t>(chunk);
        serviceBackground();
      }
      freeBuffer(buffer);
    } else if (entry.method == kZipDeflated) {
      Serial.printf("[epub-zip] Streaming inflate string payload: %s\n", entry.name.c_str());
      uint8_t *inputBuffer = static_cast<uint8_t *>(allocateInternalBuffer(kInflateInputChunkBytes));
      uint8_t *dictionary = static_cast<uint8_t *>(allocateInternalBuffer(TINFL_LZ_DICT_SIZE));
      tinfl_decompressor *inflator =
          static_cast<tinfl_decompressor *>(allocateInternalBuffer(sizeof(tinfl_decompressor)));
      if (inputBuffer == nullptr || dictionary == nullptr || inflator == nullptr) {
        Serial.printf(
            "[epub-zip] No internal inflate buffers for string: %s input=%s dict=%s inflator=%s\n",
                      entry.name.c_str(), inputBuffer == nullptr ? "no" : "yes",
            dictionary == nullptr ? "no" : "yes", inflator == nullptr ? "no" : "yes");
      } else {
        tinfl_init(inflator);

        uint32_t compressedRemaining = entry.compressedSize;
        size_t inputAvailable = 0;
        size_t inputOffset = 0;
        tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
        ok = true;

        while (status > TINFL_STATUS_DONE) {
          if (inputAvailable == 0 && compressedRemaining > 0) {
            const size_t chunk =
                std::min(kInflateInputChunkBytes, static_cast<size_t>(compressedRemaining));
            Serial.printf("[epub-zip] Reading deflate chunk: %s chunk=%u remaining=%lu\n",
                          entry.name.c_str(), static_cast<unsigned int>(chunk),
                          static_cast<unsigned long>(compressedRemaining));
            if (!readExact(file_, inputBuffer, chunk)) {
              Serial.printf("[epub-zip] Could not read deflated string payload: %s\n",
                            entry.name.c_str());
              ok = false;
              break;
            }

            compressedRemaining -= static_cast<uint32_t>(chunk);
            inputAvailable = chunk;
            inputOffset = 0;
          }

          const size_t dictionaryOffset = totalOutputBytes & (TINFL_LZ_DICT_SIZE - 1);
          uint8_t *writeCursor = dictionary + dictionaryOffset;
          size_t inSize = inputAvailable;
          size_t outSize = TINFL_LZ_DICT_SIZE - dictionaryOffset;
          const mz_uint32 flags = compressedRemaining > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;

          status = tinfl_decompress(inflator, inputBuffer + inputOffset, &inSize, dictionary,
                                    writeCursor, &outSize, flags);
          inputAvailable -= inSize;
          inputOffset += inSize;

          if (outSize > 0 && !appendBytes(writeCursor, outSize)) {
            ok = false;
            break;
          }

          serviceBackground();

          if (status < TINFL_STATUS_DONE) {
            Serial.printf("[epub-zip] Streaming inflate failed for %s status=%d\n",
                          entry.name.c_str(), static_cast<int>(status));
            ok = false;
            break;
          }

          if (inSize == 0 && outSize == 0 && status != TINFL_STATUS_DONE &&
              inputAvailable == 0 && compressedRemaining == 0) {
            Serial.printf("[epub-zip] Streaming inflate stalled for %s status=%d\n",
                          entry.name.c_str(), static_cast<int>(status));
            ok = false;
            break;
          }
        }
      }

      freeBuffer(inputBuffer);
      freeBuffer(dictionary);
      freeBuffer(inflator);
    } else {
      Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method,
                    entry.name.c_str());
    }

    if (ok && totalOutputBytes != entry.uncompressedSize) {
      Serial.printf("[epub-zip] String inflate size mismatch for %s (%lu of %lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(totalOutputBytes),
                    static_cast<unsigned long>(entry.uncompressedSize));
      ok = false;
    }

    if (ok) {
      Serial.printf("[epub-zip] Extracted string OK: %s textLen=%u\n", entry.name.c_str(),
                    static_cast<unsigned int>(output.length()));
    }

    return ok;
  }

  ContentExtractStatus extractContentToRsvp(const ZipEntry &entry, File &output, size_t &wordCount,
                                            size_t maxWords, String &lastChapterTitle,
                                            const EpubConverter::Options &options,
                                            size_t itemIndex, size_t itemCount) {
    Serial.printf("[epub-zip] Extract content: %s method=%u flags=0x%04x c=%lu u=%lu\n",
                  entry.name.c_str(), entry.method, entry.flags,
                  static_cast<unsigned long>(entry.compressedSize),
                  static_cast<unsigned long>(entry.uncompressedSize));

    if (entry.uncompressedSize == 0 || entry.compressedSize == 0 ||
        entry.uncompressedSize > options.maxContentBytes ||
        entry.compressedSize > options.maxContentBytes) {
      Serial.printf(
          "[epub] Skipping oversized content %s (%lu compressed, %lu uncompressed bytes)\n",
          entry.name.c_str(), static_cast<unsigned long>(entry.compressedSize),
          static_cast<unsigned long>(entry.uncompressedSize));
      return ContentExtractStatus::Unsupported;
    }

    uint8_t localHeader[30];
    if (!file_.seek(entry.localHeaderOffset)) {
      Serial.printf("[epub-zip] Could not seek to content local header: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(entry.localHeaderOffset));
      return ContentExtractStatus::Failed;
    }
    if (!readExact(file_, localHeader, sizeof(localHeader))) {
      Serial.printf("[epub-zip] Could not read content local header: %s\n", entry.name.c_str());
      return ContentExtractStatus::Failed;
    }
    const uint32_t localSignature = readLe32(localHeader);
    if (localSignature != kZipLocalFileSignature) {
      Serial.printf("[epub-zip] Bad content local signature for %s signature=0x%08lx\n",
                    entry.name.c_str(), static_cast<unsigned long>(localSignature));
      return ContentExtractStatus::Failed;
    }

    const uint16_t fileNameLength = readLe16(localHeader + 26);
    const uint16_t extraLength = readLe16(localHeader + 28);
    const uint32_t dataOffset = entry.localHeaderOffset + sizeof(localHeader) + fileNameLength +
                                extraLength;
    Serial.printf("[epub-zip] Content data: %s nameLen=%u extraLen=%u dataOffset=%lu\n",
                  entry.name.c_str(), fileNameLength, extraLength,
                  static_cast<unsigned long>(dataOffset));
    if (!file_.seek(dataOffset)) {
      Serial.printf("[epub-zip] Could not seek to content data: %s offset=%lu\n",
                    entry.name.c_str(), static_cast<unsigned long>(dataOffset));
      return ContentExtractStatus::Failed;
    }

    epubconvert::RsvpWriter writer(
        [&output](const char *data, size_t length) {
          output.write(reinterpret_cast<const uint8_t *>(data), length);
        },
        wordCount, maxWords, lastChapterTitle, serviceBackground);
    uint32_t totalOutputBytes = 0;
    uint32_t lastProgressBytes = 0;

    auto finishWriter = [&]() -> ContentExtractStatus {
      if (!writer.finish()) {
        return writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                         : ContentExtractStatus::Failed;
      }
      return ContentExtractStatus::Complete;
    };

    auto reportMaybe = [&](bool force) {
      if (!force && totalOutputBytes - lastProgressBytes < 32UL * 1024UL) {
        return;
      }
      lastProgressBytes = totalOutputBytes;
      reportContentProgress(options, itemIndex, itemCount, totalOutputBytes,
                            entry.uncompressedSize, wordCount);
    };

    if (entry.method == kZipStored) {
      uint8_t *buffer = static_cast<uint8_t *>(allocateInternalBuffer(kReadChunkBytes));
      if (buffer == nullptr) {
        Serial.printf("[epub] No internal memory for stored content buffer: %s\n",
                      entry.name.c_str());
        return ContentExtractStatus::Failed;
      }

      uint32_t remaining = entry.uncompressedSize;
      while (remaining > 0) {
        const size_t chunk =
            std::min(kReadChunkBytes, static_cast<size_t>(remaining));
        if (!readExact(file_, buffer, chunk)) {
          Serial.printf("[epub-zip] Stored content read failed: %s remaining=%lu\n",
                        entry.name.c_str(), static_cast<unsigned long>(remaining));
          freeBuffer(buffer);
          return ContentExtractStatus::Failed;
        }

        totalOutputBytes += static_cast<uint32_t>(chunk);
        if (!writer.write(buffer, chunk)) {
          freeBuffer(buffer);
          return writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                           : ContentExtractStatus::Failed;
        }

        remaining -= static_cast<uint32_t>(chunk);
        reportMaybe(false);
      }

      freeBuffer(buffer);
      reportMaybe(true);
      return finishWriter();
    }

    if (entry.method != kZipDeflated) {
      Serial.printf("[epub] Unsupported ZIP method %u for %s\n", entry.method,
                    entry.name.c_str());
      return ContentExtractStatus::Unsupported;
    }

    uint8_t *inputBuffer = static_cast<uint8_t *>(allocateInternalBuffer(kInflateInputChunkBytes));
    uint8_t *dictionary = static_cast<uint8_t *>(allocateInternalBuffer(TINFL_LZ_DICT_SIZE));
    tinfl_decompressor *inflator =
        static_cast<tinfl_decompressor *>(allocateInternalBuffer(sizeof(tinfl_decompressor)));
    if (inputBuffer == nullptr || dictionary == nullptr || inflator == nullptr) {
      Serial.printf(
          "[epub] No internal memory for streaming inflate buffers: %s input=%s dict=%s "
          "inflator=%s\n",
          entry.name.c_str(), inputBuffer == nullptr ? "no" : "yes",
          dictionary == nullptr ? "no" : "yes", inflator == nullptr ? "no" : "yes");
      freeBuffer(inputBuffer);
      freeBuffer(dictionary);
      freeBuffer(inflator);
      return ContentExtractStatus::Failed;
    }

    tinfl_init(inflator);

    uint32_t compressedRemaining = entry.compressedSize;
    size_t inputAvailable = 0;
    size_t inputOffset = 0;
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
    ContentExtractStatus result = ContentExtractStatus::Complete;

    while (status > TINFL_STATUS_DONE) {
      if (inputAvailable == 0 && compressedRemaining > 0) {
        const size_t chunk =
            std::min(kInflateInputChunkBytes, static_cast<size_t>(compressedRemaining));
        if (!readExact(file_, inputBuffer, chunk)) {
          Serial.printf("[epub-zip] Deflated content read failed: %s remaining=%lu\n",
                        entry.name.c_str(), static_cast<unsigned long>(compressedRemaining));
          result = ContentExtractStatus::Failed;
          break;
        }

        compressedRemaining -= static_cast<uint32_t>(chunk);
        inputAvailable = chunk;
        inputOffset = 0;
      }

      const size_t dictionaryOffset = totalOutputBytes & (TINFL_LZ_DICT_SIZE - 1);
      uint8_t *writeCursor = dictionary + dictionaryOffset;
      size_t inSize = inputAvailable;
      size_t outSize = TINFL_LZ_DICT_SIZE - dictionaryOffset;
      const mz_uint32 flags = compressedRemaining > 0 ? TINFL_FLAG_HAS_MORE_INPUT : 0;

      status = tinfl_decompress(inflator, inputBuffer + inputOffset, &inSize, dictionary,
                                writeCursor, &outSize, flags);
      inputAvailable -= inSize;
      inputOffset += inSize;

      if (outSize > 0) {
        totalOutputBytes += static_cast<uint32_t>(outSize);
        if (!writer.write(writeCursor, outSize)) {
          result = writer.reachedWordLimit() ? ContentExtractStatus::WordLimitReached
                                             : ContentExtractStatus::Failed;
          break;
        }
        reportMaybe(false);
      }

      serviceBackground();

      if (status < TINFL_STATUS_DONE) {
        Serial.printf("[epub] Inflate failed for %s status=%d\n", entry.name.c_str(),
                      static_cast<int>(status));
        result = ContentExtractStatus::Failed;
        break;
      }

      if (inSize == 0 && outSize == 0 && status != TINFL_STATUS_DONE &&
          inputAvailable == 0 && compressedRemaining == 0) {
        Serial.printf("[epub] Inflate stalled for %s status=%d\n", entry.name.c_str(),
                      static_cast<int>(status));
        result = ContentExtractStatus::Failed;
        break;
      }
    }

    freeBuffer(inputBuffer);
    freeBuffer(dictionary);
    freeBuffer(inflator);

    if (result != ContentExtractStatus::Complete) {
      return result;
    }

    if (totalOutputBytes != entry.uncompressedSize) {
      Serial.printf("[epub] Inflate size mismatch for %s (%lu of %lu bytes)\n",
                    entry.name.c_str(), static_cast<unsigned long>(totalOutputBytes),
                    static_cast<unsigned long>(entry.uncompressedSize));
      return ContentExtractStatus::Failed;
    }

    reportMaybe(true);
    return finishWriter();
  }

  String archivePath_;
  File file_;
  std::vector<ZipEntry> entries_;
};

bool convertEpubToRsvp(const String &epubPath, const String &tempPath, const String &rsvpPath,
                       const EpubConverter::Options &options) {
  reportProgress(options, "Opening EPUB", "Reading archive", 0);

  ZipArchive zip;
  if (!zip.open(epubPath)) {
    Serial.printf("[epub] Could not open EPUB archive: %s\n", epubPath.c_str());
    return false;
  }

  reportProgress(options, "Opening EPUB", "Reading metadata", 8);
  String containerXml;
  Serial.println("[epub] Reading META-INF/container.xml");
  Serial.flush();
  if (!zip.extractToString("META-INF/container.xml", containerXml, kMaxContainerBytes)) {
    Serial.println("[epub] EPUB container.xml not found or unreadable");
    zip.close();
    return false;
  }
  Serial.printf("[epub] container.xml loaded: %u chars\n",
                static_cast<unsigned int>(containerXml.length()));

  const String opfPath = epubconvert::parseRootfilePath(containerXml);
  if (opfPath.isEmpty()) {
    Serial.println("[epub] EPUB rootfile path not found");
    zip.close();
    return false;
  }
  Serial.printf("[epub] Rootfile OPF path: %s\n", opfPath.c_str());

  reportProgress(options, "Opening EPUB", "Reading package", 14);
  String opfXml;
  Serial.printf("[epub] Reading OPF package: %s\n", opfPath.c_str());
  if (!zip.extractToString(opfPath, opfXml, kMaxOpfBytes)) {
    Serial.printf("[epub] OPF file not readable: %s\n", opfPath.c_str());
    zip.close();
    return false;
  }
  Serial.printf("[epub] OPF loaded: %u chars\n", static_cast<unsigned int>(opfXml.length()));

  const String opfBaseDir = epubconvert::directoryForPath(opfPath);
  const std::vector<epubconvert::ManifestItem> manifest =
      epubconvert::parseManifestItems(opfXml, opfBaseDir);
  const std::vector<String> spineIds = epubconvert::parseSpineIds(opfXml);
  std::vector<String> readingOrder;
  readingOrder.reserve(spineIds.size());
  Serial.printf("[epub] Package parsed: manifest=%u spine=%u base=%s\n",
                static_cast<unsigned int>(manifest.size()),
                static_cast<unsigned int>(spineIds.size()), opfBaseDir.c_str());

  reportProgress(options, "Opening EPUB", "Building reading order", 20);
  for (size_t i = 0; i < spineIds.size(); ++i) {
    serviceBackground();
    const epubconvert::ManifestItem *item = epubconvert::findManifestItem(manifest, spineIds[i]);
    if (item != nullptr && epubconvert::isContentDocument(*item)) {
      readingOrder.push_back(item->path);
    }
  }

  if (readingOrder.empty()) {
    for (size_t i = 0; i < manifest.size(); ++i) {
      if (epubconvert::isContentDocument(manifest[i])) {
        readingOrder.push_back(manifest[i].path);
      }
    }
  }

  if (readingOrder.empty()) {
    Serial.println("[epub] No readable XHTML spine items found");
    zip.close();
    return false;
  }

  Serial.printf("[epub] Reading order contains %u content files\n",
                static_cast<unsigned int>(readingOrder.size()));
  const String foundDetail = String(readingOrder.size()) + " content files";
  reportProgress(options, "Opening EPUB", foundDetail.c_str(), 25);

  SD_MMC.remove(tempPath);
  File output = SD_MMC.open(tempPath, FILE_WRITE);
  if (!output) {
    Serial.printf("[epub] Could not create temporary RSVP file: %s\n", tempPath.c_str());
    zip.close();
    return false;
  }

  String title = epubconvert::parseBookTitle(opfXml);
  if (title.isEmpty()) {
    title = epubconvert::basenameWithoutExtension(epubPath);
  }
  const String author = epubconvert::parseBookAuthor(opfXml);

  epubconvert::writeRsvpHeader(
      [&output](const char *data, size_t length) {
        output.write(reinterpret_cast<const uint8_t *>(data), length);
      },
      kConverterVersion, title, author, epubPath);

  size_t wordCount = 0;
  String lastChapterTitle;

  for (size_t i = 0;
       i < readingOrder.size() && !epubconvert::reachedWordLimit(wordCount, options.maxWords);
       ++i) {
    serviceBackground();
    const int startPercent = 25 + static_cast<int>((i * 70UL) / readingOrder.size());
    const String startDetail =
        String(i + 1) + "/" + String(readingOrder.size()) + " " + String(wordCount) + " words";
    reportProgress(options, "Extracting content", startDetail.c_str(), startPercent);

    const ContentExtractStatus extractStatus =
        zip.extractContentToRsvp(readingOrder[i], output, wordCount, options.maxWords,
                                 lastChapterTitle, options, i, readingOrder.size());
    const int finishPercent = 25 + static_cast<int>(((i + 1) * 70UL) / readingOrder.size());
    const String finishDetail =
        String(i + 1) + "/" + String(readingOrder.size()) + " " + String(wordCount) + " words";
    reportProgress(options, "Parsed content", finishDetail.c_str(), finishPercent);

    if (extractStatus == ContentExtractStatus::Unsupported ||
        extractStatus == ContentExtractStatus::Failed) {
      Serial.printf("[epub] Skipping unreadable content file: %s\n", readingOrder[i].c_str());
      continue;
    }

    if (extractStatus == ContentExtractStatus::WordLimitReached) {
      break;
    }
  }

  const String finishingDetail = String(wordCount) + " words";
  reportProgress(options, "Finishing EPUB", finishingDetail.c_str(), 96);
  output.close();
  zip.close();

  if (wordCount == 0) {
    Serial.printf("[epub] No readable words extracted from %s\n", epubPath.c_str());
    SD_MMC.remove(tempPath);
    return false;
  }

  SD_MMC.remove(rsvpPath);
  if (!SD_MMC.rename(tempPath, rsvpPath)) {
    Serial.printf("[epub] Could not rename %s to %s\n", tempPath.c_str(), rsvpPath.c_str());
    SD_MMC.remove(tempPath);
    return false;
  }

  Serial.printf("[epub] Converted %s -> %s (%u words)\n", epubPath.c_str(), rsvpPath.c_str(),
                static_cast<unsigned int>(wordCount));
  const String convertedDetail = String(wordCount) + " words";
  reportProgress(options, "EPUB converted", convertedDetail.c_str(), 100);
  return true;
}

void writeFailureMarker(const String &markerPath, const char *message) {
  SD_MMC.remove(markerPath);

  File marker = SD_MMC.open(markerPath, FILE_WRITE);
  if (!marker) {
    Serial.printf("[epub] Could not create failure marker: %s\n", markerPath.c_str());
    return;
  }

  marker.println(message == nullptr ? "Conversion failed" : message);
  marker.print("converter=");
  marker.println(kConverterVersion);
  marker.close();
}

bool markerWasWrittenByCurrentConverter(File &marker) {
  String content;
  while (marker.available() && content.length() < 256) {
    content += static_cast<char>(marker.read());
  }

  const String expected = String("converter=") + kConverterVersion;
  return content.indexOf(expected) >= 0;
}

bool rsvpWasWrittenByCurrentConverter(File &file) {
  if (!file || file.isDirectory()) {
    return false;
  }

  file.seek(0);
  String line;
  size_t scannedLines = 0;
  while (file.available() && scannedLines < 12) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line.trim();
      if (line.startsWith("@converter")) {
        const String expected = String("@converter ") + kConverterVersion;
        return line == expected;
      }
      if (!line.isEmpty() && !line.startsWith("@")) {
        break;
      }
      line = "";
      ++scannedLines;
      continue;
    }
    if (line.length() < 128) {
      line += c;
    }
  }

  line.trim();
  if (line.startsWith("@converter")) {
    const String expected = String("@converter ") + kConverterVersion;
    return line == expected;
  }

  return false;
}

}  // namespace

bool EpubConverter::isCurrentCache(const String &rsvpPath) {
  File existing = SD_MMC.open(rsvpPath);
  const bool current = rsvpWasWrittenByCurrentConverter(existing);
  if (existing) {
    existing.close();
  }
  return current;
}

bool EpubConverter::convertIfNeeded(const String &epubPath, const String &rsvpPath,
                                    const Options &options) {
  File existing = SD_MMC.open(rsvpPath);
  if (existing && !existing.isDirectory() && existing.size() > 0) {
    const bool currentCache = rsvpWasWrittenByCurrentConverter(existing);
    existing.close();
    if (currentCache) {
      return true;
    }

    Serial.printf("[epub] Rebuilding stale RSVP cache after converter update: %s\n",
                  rsvpPath.c_str());
    SD_MMC.remove(rsvpPath);
  } else if (existing) {
    existing.close();
  }

  const String tempPath = rsvpPath + ".tmp";
  const String failedPath = rsvpPath + ".failed";
  const String lockPath = rsvpPath + ".converting";

  File lock = SD_MMC.open(lockPath);
  if (lock) {
    const bool lockMarker = !lock.isDirectory();
    const bool currentLock = lockMarker && markerWasWrittenByCurrentConverter(lock);
    lock.close();
    if (lockMarker) {
      SD_MMC.remove(lockPath);
      SD_MMC.remove(tempPath);
      if (currentLock) {
        Serial.printf("[epub] Previous conversion restart detected, skipping: %s\n",
                      epubPath.c_str());
        writeFailureMarker(failedPath, "Previous conversion restarted before completion.");
        reportProgress(options, "Previous restart", "Skipping this EPUB", 100);
        return false;
      }

      Serial.printf("[epub] Retrying interrupted EPUB after converter update: %s\n",
                    epubPath.c_str());
    }
  }

  File temp = SD_MMC.open(tempPath);
  if (temp) {
    const bool interruptedTemp = !temp.isDirectory();
    temp.close();
    if (interruptedTemp) {
      Serial.printf("[epub] Removing stale temporary conversion file and retrying: %s\n",
                    epubPath.c_str());
      SD_MMC.remove(tempPath);
    }
  }

  File failed = SD_MMC.open(failedPath);
  if (failed) {
    const bool failedMarker = !failed.isDirectory();
    const bool currentFailure = failedMarker && markerWasWrittenByCurrentConverter(failed);
    failed.close();
    if (failedMarker) {
      if (currentFailure) {
        Serial.printf("[epub] Skipping EPUB with failure marker: %s\n", epubPath.c_str());
        return false;
      }

      Serial.printf("[epub] Retrying EPUB after converter update: %s\n", epubPath.c_str());
      SD_MMC.remove(failedPath);
    }
  }

  Serial.printf("[epub] Converting on device: %s\n", epubPath.c_str());
  writeFailureMarker(lockPath, "Conversion in progress. Delete this file only if retrying.");
  const bool converted = convertEpubToRsvp(epubPath, tempPath, rsvpPath, options);
  SD_MMC.remove(lockPath);
  if (!converted) {
    writeFailureMarker(failedPath, "Conversion failed. Remove this marker to retry.");
    return false;
  }

  SD_MMC.remove(failedPath);
  return true;
}
