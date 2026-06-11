#include "storage/StorageManager.h"

#include <SD_MMC.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <driver/sdmmc_types.h>
#include <esp_heap_caps.h>
#include <utility>

#include "board/BoardConfig.h"
#include "storage/EpubConverter.h"
#include "text/BookText.h"
#include "text/LatinText.h"

#ifndef RSVP_ON_DEVICE_EPUB_CONVERSION
#define RSVP_ON_DEVICE_EPUB_CONVERSION 0
#endif

#ifndef RSVP_MAX_BOOK_WORDS
#define RSVP_MAX_BOOK_WORDS 0
#endif

// Book-text parsing now lives in the BookText module; pull the pieces the
// SD-bound paths (directive readers, indexed builder, parse driver) still call.
using booktext::appendTokenizedLineWords;
using booktext::chapterTitleFromLine;
using booktext::directiveValue;
using booktext::hasBookWordLimit;
using booktext::isStandaloneRhythmToken;
using booktext::normalizeDisplayText;
using booktext::ParseStats;
using booktext::prefixHasBoundary;
using booktext::processBookLine;
using booktext::processRsvpLine;
using booktext::reachedBookWordLimit;
using booktext::stripBom;
using booktext::tokenHasReadableCharacter;
using booktext::trimAsciiWhitespace;

namespace {

constexpr const char *kBooksPath = "/books";
constexpr const char *kBookFilesPath = "/books/books";
constexpr const char *kArticleFilesPath = "/books/articles";
constexpr size_t kMaxBookWords = static_cast<size_t>(RSVP_MAX_BOOK_WORDS);
constexpr size_t kMaxChapterTitleChars = 64;
constexpr size_t kMaxBookLineChars = 4096;
constexpr size_t kInitialWordReserveMax = 50000;
constexpr size_t kParseMemoryCheckWordInterval = 512;
constexpr size_t kParseMinFreeHeapBytes = 32 * 1024;
constexpr size_t kParseMinLargestHeapBlockBytes = 8 * 1024;



bool parseMemoryLow() {
  return heap_caps_get_free_size(MALLOC_CAP_8BIT) < kParseMinFreeHeapBytes ||
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < kParseMinLargestHeapBlockBytes;
}

bool hasTextExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".txt");
}

bool hasRsvpExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".rsvp");
}

bool hasEpubExtension(const String &path) {
  String lowered = path;
  lowered.toLowerCase();
  return lowered.endsWith(".epub");
}

String siblingPathWithExtension(const String &path, const char *extension) {
  String siblingPath = path;
  const int dot = siblingPath.lastIndexOf('.');
  if (dot > 0) {
    siblingPath = siblingPath.substring(0, dot);
  }
  siblingPath += extension;
  return siblingPath;
}

String epubSiblingPathForRsvp(const String &rsvpPath) {
  return siblingPathWithExtension(rsvpPath, ".epub");
}

String normalizeBookPath(const String &path) {
  if (path.startsWith("/")) {
    return path;
  }
  return String(kBooksPath) + "/" + path;
}

String displayNameForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  if (separator < 0) {
    return path;
  }
  return path.substring(separator + 1);
}

bool isHiddenOrSidecarPath(const String &path) {
  const String name = displayNameForPath(path);
  if (name.length() == 0) {
    return true;
  }

  String lowered = name;
  lowered.toLowerCase();
  if (lowered.startsWith(".")) {
    return true;
  }

  if (lowered.endsWith(".ridx") || lowered.endsWith(".rdat") ||
      lowered.endsWith(".tmp")) {
    return true;
  }

  return lowered == "thumbs.db" || lowered == "desktop.ini";
}

String displayNameWithoutExtension(const String &path) {
  String name = displayNameForPath(path);
  String lowered = name;
  lowered.toLowerCase();
  if (lowered.endsWith(".txt")) {
    name.remove(name.length() - 4);
  } else if (lowered.endsWith(".rsvp")) {
    name.remove(name.length() - 5);
  } else if (lowered.endsWith(".epub")) {
    name.remove(name.length() - 5);
  }
  return name;
}

String rsvpCachePathForEpub(const String &epubPath) {
  return siblingPathWithExtension(epubPath, ".rsvp");
}

struct EpubProgressContext {
  StorageManager::StatusCallback statusCallback = nullptr;
  void *statusContext = nullptr;
  String title;
  String label;
  int basePercent = 0;
  int spanPercent = 100;
};

void handleEpubProgress(void *rawContext, const char *line1, const char *line2,
                        int progressPercent) {
  EpubProgressContext *context = static_cast<EpubProgressContext *>(rawContext);
  if (context == nullptr) {
    return;
  }

  progressPercent = std::max(0, std::min(100, progressPercent));
  const int overallPercent =
      context->basePercent + ((context->spanPercent * progressPercent) / 100);
  const String detail = String(line1 == nullptr ? "" : line1) + " - " +
                        String(line2 == nullptr ? "" : line2);
  const char *title = context->title.isEmpty() ? "EPUB" : context->title.c_str();
  Serial.printf("[epub-progress] %d%% %s | %s | %s\n", overallPercent, title,
                context->label.c_str(), detail.c_str());

  // Keep the display on the static "Converting EPUB" screen while ZIP work is active.
  // Full-screen redraws from inside this callback have proven risky while the SD archive is open.
  yield();
  delay(0);
}

bool fileExistsAndHasBytes(const String &path) {
  File file = SD_MMC.open(path);
  const bool exists = file && !file.isDirectory() && file.size() > 0;
  if (file) {
    file.close();
  }
  return exists;
}

bool hasCurrentEpubCache(const String &epubPath) {
  const String rsvpPath = rsvpCachePathForEpub(epubPath);
  return fileExistsAndHasBytes(rsvpPath) && EpubConverter::isCurrentCache(rsvpPath);
}

bool markerExists(const String &path) {
  File file = SD_MMC.open(path);
  const bool exists = file && !file.isDirectory();
  if (file) {
    file.close();
  }
  return exists;
}

String epubLibraryLabel(const String &path) {
  const String rsvpPath = rsvpCachePathForEpub(path);
  if (markerExists(rsvpPath + ".failed")) {
    return "EPUB failed - check serial";
  }
  if (markerExists(rsvpPath + ".converting") || markerExists(rsvpPath + ".tmp")) {
    return "EPUB interrupted";
  }
  return "EPUB - converts on open";
}

int pathIndexIn(const std::vector<String> &paths, const String &target) {
  for (size_t i = 0; i < paths.size(); ++i) {
    if (paths[i] == target) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void logHeapSnapshot(const char *label) {
  Serial.printf("[heap] %s free8=%lu free_spiram=%lu largest8=%lu largest_spiram=%lu\n",
                label == nullptr ? "" : label,
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

struct DirectoryEntryInfo {
  String path;
  String loweredPath;
  size_t bytes = 0;
};

void appendLibraryDirectoryEntries(const char *directoryPath, std::vector<DirectoryEntryInfo> &entries) {
  File dir = SD_MMC.open(directoryPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String name = displayNameForPath(String(entry.name()));
      if (!name.isEmpty()) {
        DirectoryEntryInfo info;
        info.path = String(directoryPath) + "/" + name;
        info.loweredPath = info.path;
        info.loweredPath.toLowerCase();
        info.bytes = static_cast<size_t>(entry.size());
        entries.push_back(info);
      }
    }
    entry.close();
    // SD enumeration runs synchronously in the loop task; with a large library
    // it can hold the CPU for seconds. Yield each entry so the watchdog and
    // background tasks stay fed.
    yield();
    entry = dir.openNextFile();
  }

  dir.close();
}

std::vector<DirectoryEntryInfo> inventoryBookDirectory() {
  std::vector<DirectoryEntryInfo> entries;

  appendLibraryDirectoryEntries(kBooksPath, entries);
  appendLibraryDirectoryEntries(kBookFilesPath, entries);
  appendLibraryDirectoryEntries(kArticleFilesPath, entries);
  return entries;
}

bool inventoryHasFileWithBytes(const std::vector<DirectoryEntryInfo> &entries, const String &path) {
  String loweredPath = path;
  loweredPath.toLowerCase();
  for (const DirectoryEntryInfo &entry : entries) {
    if (entry.loweredPath == loweredPath) {
      return entry.bytes > 0;
    }
  }
  return false;
}

std::vector<String> collectBookPaths() {
  std::vector<String> bookPaths;
  const uint32_t startedMs = millis();
  const std::vector<DirectoryEntryInfo> entries = inventoryBookDirectory();
  size_t cacheProbeCount = 0;

  for (const DirectoryEntryInfo &entry : entries) {
    const String &path = entry.path;
    if (isHiddenOrSidecarPath(path)) {
      continue;
    }

    bool staleGeneratedRsvp = false;
    if (hasRsvpExtension(path) &&
        inventoryHasFileWithBytes(entries, epubSiblingPathForRsvp(path))) {
      ++cacheProbeCount;
      staleGeneratedRsvp = !EpubConverter::isCurrentCache(path);
    }

    const bool readableText =
        hasTextExtension(path) &&
        !inventoryHasFileWithBytes(entries, siblingPathWithExtension(path, ".rsvp"));

    bool pendingEpub = false;
    if (RSVP_ON_DEVICE_EPUB_CONVERSION && hasEpubExtension(path)) {
      const String rsvpPath = rsvpCachePathForEpub(path);
      const bool hasCache = inventoryHasFileWithBytes(entries, rsvpPath);
      if (!hasCache) {
        pendingEpub = true;
      } else {
        ++cacheProbeCount;
        pendingEpub = !EpubConverter::isCurrentCache(rsvpPath);
      }
    }

    if ((!staleGeneratedRsvp && hasRsvpExtension(path)) || readableText || pendingEpub) {
      bookPaths.push_back(path);
    }
  }

  std::sort(bookPaths.begin(), bookPaths.end(), [](const String &left, const String &right) {
    String leftKey = displayNameForPath(left);
    String rightKey = displayNameForPath(right);
    leftKey.toLowerCase();
    rightKey.toLowerCase();
    return leftKey < rightKey;
  });

  Serial.printf("[storage] Directory inventory: %u files, %u books, %u cache probes in %lu ms\n",
                static_cast<unsigned int>(entries.size()),
                static_cast<unsigned int>(bookPaths.size()),
                static_cast<unsigned int>(cacheProbeCount),
                static_cast<unsigned long>(millis() - startedMs));

  return bookPaths;
}

size_t countUnsupportedBookFiles() {
  size_t unsupported = 0;

  const std::vector<DirectoryEntryInfo> entries = inventoryBookDirectory();
  for (const DirectoryEntryInfo &entry : entries) {
    const String &path = entry.path;
    const bool hidden = isHiddenOrSidecarPath(path);
    if (!hidden && !hasRsvpExtension(path) && !hasTextExtension(path) && !hasEpubExtension(path)) {
      ++unsupported;
    }
  }

  return unsupported;
}

struct RsvpDirectiveValues {
  String title;
  String author;
};

RsvpDirectiveValues readRsvpDirectiveValues(const String &path) {
  RsvpDirectiveValues values;
  if (!hasRsvpExtension(path)) {
    return values;
  }

  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return values;
  }

  String line;
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }

    if (c != '\n') {
      line += c;
      if (line.length() > kMaxChapterTitleChars + 16) {
        line = "";
        break;
      }
      continue;
    }

    String trimmed = stripBom(line);
    if (trimmed.isEmpty()) {
      line = "";
      continue;
    }

    String lowered = trimmed;
    lowered.toLowerCase();
    if (values.title.isEmpty() && prefixHasBoundary(lowered, "@title")) {
      values.title = directiveValue(trimmed, "@title");
    } else if (values.author.isEmpty() && prefixHasBoundary(lowered, "@author")) {
      values.author = directiveValue(trimmed, "@author");
    } else if (!trimmed.startsWith("@")) {
      break;
    }

    if (!values.title.isEmpty() && !values.author.isEmpty()) {
      break;
    }
    line = "";
  }

  file.close();
  return values;
}

String readRsvpDirectiveValue(const String &path, const char *directive) {
  if (!hasRsvpExtension(path)) {
    return "";
  }

  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return "";
  }

  String line;
  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }

    if (c != '\n') {
      line += c;
      if (line.length() > kMaxChapterTitleChars + 16) {
        line = "";
        break;
      }
      continue;
    }

    String trimmed = stripBom(line);
    if (trimmed.isEmpty()) {
      line = "";
      continue;
    }

    String lowered = trimmed;
    lowered.toLowerCase();
    if (prefixHasBoundary(lowered, directive)) {
      file.close();
      return directiveValue(trimmed, directive);
    }

    if (!trimmed.startsWith("@")) {
      break;
    }
    line = "";
  }

  file.close();
  return "";
}

String indexedIndexPathFor(const String &path) { return path + ".ridx"; }

String indexedDataPathFor(const String &path) { return path + ".rdat"; }

String indexedTempPathFor(const String &path) { return path + ".tmp"; }

bool writeExact(File &file, const void *data, size_t bytes) {
  return file.write(reinterpret_cast<const uint8_t *>(data), bytes) == bytes;
}

bool readExact(File &file, void *data, size_t bytes) {
  return file.read(reinterpret_cast<uint8_t *>(data), bytes) == bytes;
}

uint32_t fnv1aUpdate(uint32_t hash, const uint8_t *data, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

uint32_t sourceFingerprint(File &file, uint32_t sourceSize) {
  uint32_t hash = 2166136261UL;
  uint8_t sizeBytes[4] = {
      static_cast<uint8_t>(sourceSize & 0xFF),
      static_cast<uint8_t>((sourceSize >> 8) & 0xFF),
      static_cast<uint8_t>((sourceSize >> 16) & 0xFF),
      static_cast<uint8_t>((sourceSize >> 24) & 0xFF),
  };
  hash = fnv1aUpdate(hash, sizeBytes, sizeof(sizeBytes));

  constexpr size_t kSampleBytes = 512;
  uint8_t buffer[kSampleBytes];
  const uint32_t offsets[] = {
      0,
      sourceSize > kSampleBytes ? sourceSize / 2 : 0,
      sourceSize > kSampleBytes ? sourceSize - kSampleBytes : 0,
  };

  for (uint32_t offset : offsets) {
    if (!file.seek(offset)) {
      continue;
    }
    const size_t wanted =
        static_cast<size_t>(std::min<uint32_t>(kSampleBytes, sourceSize - offset));
    const size_t read = file.read(buffer, wanted);
    hash = fnv1aUpdate(hash, buffer, read);
  }

  return hash;
}

uint32_t sourceFingerprint(const String &path, uint32_t sourceSize) {
  File file = SD_MMC.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return 0;
  }

  const uint32_t fingerprint = sourceFingerprint(file, sourceSize);
  file.close();
  return fingerprint;
}

bool readIndexHeader(const String &path, IndexedBookStore::Header &header) {
  File file = SD_MMC.open(indexedIndexPathFor(path), FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return false;
  }

  const bool ok = readExact(file, &header, sizeof(header));
  file.close();
  return ok && header.magic == IndexedBookStore::kMagic &&
         header.version == IndexedBookStore::kVersion &&
         header.headerSize == sizeof(IndexedBookStore::Header) &&
         header.recordSize == sizeof(IndexedBookStore::WordRecord) &&
         header.recordsOffset >= sizeof(IndexedBookStore::Header);
}

struct IndexedBuildContext {
  File *indexFile = nullptr;
  File *dataFile = nullptr;
  BookMetadata *metadata = nullptr;
  uint32_t wordCount = 0;
  uint32_t dataSize = 0;
  bool failed = false;
  const char *failure = "";
};

void addIndexedChapterMarker(IndexedBuildContext &context, const String &title) {
  if (title.isEmpty() || context.metadata == nullptr) {
    return;
  }

  ChapterMarker marker;
  marker.title = title;
  marker.wordIndex = context.wordCount;

  if (!context.metadata->chapters.empty() &&
      context.metadata->chapters.back().wordIndex == marker.wordIndex) {
    context.metadata->chapters.back() = marker;
    return;
  }

  context.metadata->chapters.push_back(marker);
}

void addIndexedParagraphMarker(IndexedBuildContext &context) {
  if (context.metadata == nullptr) {
    return;
  }

  const size_t wordIndex = context.wordCount;
  if (!context.metadata->paragraphStarts.empty() &&
      context.metadata->paragraphStarts.back() == wordIndex) {
    return;
  }

  context.metadata->paragraphStarts.push_back(wordIndex);
}

bool pushIndexedWord(String token, IndexedBuildContext &context, ParseStats *stats) {
  trimAsciiWhitespace(token);

  if (token.length() >= 3 && static_cast<uint8_t>(token[0]) == 0xEF &&
      static_cast<uint8_t>(token[1]) == 0xBB && static_cast<uint8_t>(token[2]) == 0xBF) {
    token.remove(0, 3);
  }

  trimAsciiWhitespace(token);

  if (token.isEmpty() ||
      (!tokenHasReadableCharacter(token) && !isStandaloneRhythmToken(token))) {
    return true;
  }

  if (token.length() > UINT16_MAX ||
      context.dataSize > UINT32_MAX - static_cast<uint32_t>(token.length())) {
    context.failed = true;
    context.failure = "Index limit reached";
    return false;
  }

  if ((context.wordCount % kParseMemoryCheckWordInterval) == 0 && context.wordCount > 0 &&
      parseMemoryLow()) {
    if (stats != nullptr) {
      stats->memoryLow = true;
    }
    context.failed = true;
    context.failure = "Memory limit reached";
    return false;
  }

  IndexedBookStore::WordRecord record;
  record.offset = context.dataSize;
  record.length = static_cast<uint16_t>(token.length());
  record.flags = 0;

  if (!writeExact(*context.dataFile, token.c_str(), token.length()) ||
      !writeExact(*context.indexFile, &record, sizeof(record))) {
    context.failed = true;
    context.failure = "SD write failed";
    return false;
  }

  context.dataSize += static_cast<uint32_t>(token.length());
  ++context.wordCount;
  if (context.metadata != nullptr) {
    context.metadata->wordCount = context.wordCount;
  }
  return true;
}

bool appendIndexedLineWords(const String &line, IndexedBuildContext &context, ParseStats *stats) {
  return appendTokenizedLineWords(
      line, [&](const String &token) { return pushIndexedWord(token, context, stats); },
      [&]() { return static_cast<size_t>(context.wordCount); }, stats);
}

bool processIndexedBookLine(const String &line, IndexedBuildContext &context,
                            bool &paragraphPending, ParseStats *stats) {
  const String trimmed = stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending = true;
    return true;
  }

  String chapterTitle;
  if (chapterTitleFromLine(line, chapterTitle)) {
    addIndexedChapterMarker(context, chapterTitle);
    paragraphPending = true;
  }

  if (paragraphPending) {
    addIndexedParagraphMarker(context);
    paragraphPending = false;
  }
  return appendIndexedLineWords(line, context, stats);
}

bool processIndexedRsvpLine(const String &line, IndexedBuildContext &context,
                            bool &paragraphPending, ParseStats *stats) {
  String trimmed = stripBom(line);
  if (trimmed.isEmpty()) {
    paragraphPending = true;
    return true;
  }

  if (trimmed.startsWith("@@")) {
    trimmed.remove(0, 1);
    if (paragraphPending) {
      addIndexedParagraphMarker(context);
      paragraphPending = false;
    }
    return appendIndexedLineWords(trimmed, context, stats);
  }

  if (trimmed.startsWith("@")) {
    String lowered = trimmed;
    lowered.toLowerCase();
    if (prefixHasBoundary(lowered, "@para")) {
      paragraphPending = true;
      return true;
    }
    if (prefixHasBoundary(lowered, "@chapter")) {
      String title = directiveValue(trimmed, "@chapter");
      if (title.isEmpty()) {
        title = "Chapter";
      }
      addIndexedChapterMarker(context, title);
      paragraphPending = true;
      return true;
    }
    if (context.metadata != nullptr && prefixHasBoundary(lowered, "@title")) {
      context.metadata->title = directiveValue(trimmed, "@title");
      return true;
    }
    if (context.metadata != nullptr && prefixHasBoundary(lowered, "@author")) {
      context.metadata->author = directiveValue(trimmed, "@author");
      return true;
    }
    return true;
  }

  if (paragraphPending) {
    addIndexedParagraphMarker(context);
    paragraphPending = false;
  }
  return appendIndexedLineWords(line, context, stats);
}

}  // namespace

void StorageManager::setStatusCallback(StatusCallback callback, void *context) {
  statusCallback_ = callback;
  statusContext_ = context;
}

void StorageManager::notifyStatus(const char *title, const char *line1, const char *line2,
                                  int progressPercent) {
  Serial.printf("[storage-status] %d%% %s | %s | %s\n", progressPercent,
                title == nullptr ? "" : title, line1 == nullptr ? "" : line1,
                line2 == nullptr ? "" : line2);
  if (statusCallback_ != nullptr) {
    statusCallback_(statusContext_, title, line1, line2, progressPercent);
  }
}

bool StorageManager::begin() {
  mounted_ = false;
  listedOnce_ = false;
  clearBookCache();

  mounted_ = sdCard_.mount(statusCallback_, statusContext_);
  if (!mounted_) {
    Serial.println("[storage] SD init failed after retries");
    return false;
  }

  notifyStatus("SD", "Scanning books", "EPUB converts on open", 10);
  refreshBookPaths(false);
  return true;
}

void StorageManager::end() {
  if (mounted_) {
    sdCard_.unmount();
  }
  mounted_ = false;
  listedOnce_ = false;
  clearBookCache();
}

void StorageManager::listBooks() {
  if (!mounted_ || listedOnce_) {
    return;
  }
  listedOnce_ = true;

  if (!sdCard_.directoryExists(kBooksPath)) {
    Serial.println("[storage] /books directory not found");
    return;
  }

  if (bookPaths_.empty()) {
    refreshBookPaths();
  }
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    return;
  }

  Serial.println("[storage] Listing /books, /books/books, /books/articles (.rsvp/.txt/.epub pending conversion):");
  for (const String &path : bookPaths_) {
    File entry = SD_MMC.open(path);
    if (!entry || entry.isDirectory()) {
      if (entry) {
        entry.close();
      }
      continue;
    }

    Serial.printf("  %s (%lu bytes)\n", path.c_str(),
                  static_cast<unsigned long>(entry.size()));
    entry.close();
  }
}

void StorageManager::refreshBooks(bool includeMetadata) {
  refreshBookPaths(includeMetadata);
}

bool StorageManager::loadFirstBookWords(std::vector<String> &words, String *loadedPath) {
  return loadBookWords(0, words, loadedPath);
}

size_t StorageManager::bookCount() const { return bookPaths_.size(); }

String StorageManager::bookPath(size_t index) const {
  if (index >= bookPaths_.size()) {
    return "";
  }
  return bookPaths_[index];
}

String StorageManager::deleteBook(size_t index) {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    Serial.printf("[storage] deleteBook: index %u out of range\n",
                  static_cast<unsigned int>(index));
    return "";
  }
  if (!mounted_) {
    Serial.println("[storage] deleteBook: SD not mounted");
    return "";
  }

  // Remove the index sidecars first (they are derived data); a failure to
  // remove a sidecar is non-fatal -- they'll be rebuilt or ignored.
  const String indexPath = indexedIndexPathFor(path);
  const String dataPath = indexedDataPathFor(path);
  SD_MMC.remove(indexedTempPathFor(indexPath));
  SD_MMC.remove(indexedTempPathFor(dataPath));
  SD_MMC.remove(indexPath);
  SD_MMC.remove(dataPath);

  const bool removed = SD_MMC.remove(path);
  if (!removed) {
    Serial.printf("[storage] deleteBook: failed to remove %s\n", path.c_str());
    return "";
  }

  Serial.printf("[storage] deleted book %s (+ index sidecars)\n", path.c_str());
  refreshBookPaths(true);
  return path;
}

bool StorageManager::bookIsArticle(size_t index) const {
  const String path = bookPath(index);
  return path.startsWith(String(kArticleFilesPath) + "/");
}

String StorageManager::bookDisplayName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  if (index < bookTitles_.size() && !bookTitles_[index].isEmpty()) {
    return bookTitles_[index];
  }

  return normalizeDisplayText(displayNameWithoutExtension(path));
}

String StorageManager::bookAuthorName(size_t index) const {
  const String path = bookPath(index);
  if (path.isEmpty()) {
    return "";
  }

  if (index < bookAuthors_.size()) {
    return bookAuthors_[index];
  }

  if (hasEpubExtension(path)) {
    return epubLibraryLabel(path);
  }

  return readRsvpDirectiveValue(path, "@author");
}

bool StorageManager::ensureEpubConverted(const String &epubPath, String &rsvpPath) {
  rsvpPath = rsvpCachePathForEpub(epubPath);

  if (!RSVP_ON_DEVICE_EPUB_CONVERSION) {
    Serial.printf("[storage] EPUB conversion disabled at build time: %s\n", epubPath.c_str());
    notifyStatus("EPUB unsupported", displayNameForPath(epubPath).c_str(),
                 "Build flag is disabled", 100);
    return false;
  }

  if (!fileExistsAndHasBytes(epubPath)) {
    Serial.printf("[storage] EPUB source missing or empty: %s\n", epubPath.c_str());
    notifyStatus("Preparing book", displayNameForPath(epubPath).c_str(), "EPUB missing", 100);
    return false;
  }

  if (fileExistsAndHasBytes(rsvpPath) && EpubConverter::isCurrentCache(rsvpPath)) {
    Serial.printf("[storage] EPUB cache hit: %s -> %s\n", epubPath.c_str(), rsvpPath.c_str());
    return true;
  }

  if (fileExistsAndHasBytes(rsvpPath)) {
    Serial.printf("[storage] EPUB cache stale after converter update: %s\n", rsvpPath.c_str());
  }

  File epubFile = SD_MMC.open(epubPath);
  const size_t epubBytes = epubFile ? static_cast<size_t>(epubFile.size()) : 0;
  if (epubFile) {
    epubFile.close();
  }

  Serial.printf("[storage] Preparing EPUB conversion: source=%s output=%s size=%lu bytes\n",
                epubPath.c_str(), rsvpPath.c_str(), static_cast<unsigned long>(epubBytes));
  logHeapSnapshot("before EPUB conversion");
  notifyStatus("Preparing book", displayNameForPath(epubPath).c_str(), "Converting EPUB", 0);

  EpubConverter::Options options;
  options.maxWords = kMaxBookWords;
  options.progressCallback = handleEpubProgress;
  EpubProgressContext progressContext;
  progressContext.statusCallback = statusCallback_;
  progressContext.statusContext = statusContext_;
  progressContext.title = "Preparing book";
  progressContext.label = displayNameForPath(epubPath);
  options.progressContext = &progressContext;

  const uint32_t startedMs = millis();
  const bool converted = EpubConverter::convertIfNeeded(epubPath, rsvpPath, options);
  const uint32_t elapsedMs = millis() - startedMs;
  logHeapSnapshot("after EPUB conversion");

  if (!converted || !fileExistsAndHasBytes(rsvpPath)) {
    Serial.printf("[storage] EPUB conversion failed after %lu ms: %s\n",
                  static_cast<unsigned long>(elapsedMs), epubPath.c_str());
    notifyStatus("Preparing book", "EPUB conversion failed", "Check serial monitor", 100);
    return false;
  }

  Serial.printf("[storage] EPUB conversion ready after %lu ms: %s\n",
                static_cast<unsigned long>(elapsedMs), rsvpPath.c_str());
  notifyStatus("Preparing book", displayNameForPath(rsvpPath).c_str(), "Conversion complete",
               100);
  return true;
}

bool StorageManager::loadBookContent(size_t index, BookContent &book, String *loadedPath,
                                     size_t *loadedIndex) {
  book.clear();

  if (!mounted_) {
    Serial.println("[storage] SD not mounted, cannot load book");
    return false;
  }

  if (!sdCard_.directoryExists(kBooksPath)) {
    Serial.println("[storage] /books directory not found");
    return false;
  }

  if (bookPaths_.empty()) {
    refreshBookPaths(false);
  }
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    return false;
  }

  if (index >= bookPaths_.size()) {
    Serial.printf("[storage] Book index %u out of range\n", static_cast<unsigned int>(index));
    return false;
  }

  for (size_t offset = 0; offset < bookPaths_.size(); ++offset) {
    const size_t candidateIndex = (index + offset) % bookPaths_.size();
    String path = bookPaths_[candidateIndex];
    size_t parsedIndex = candidateIndex;

    if (hasEpubExtension(path)) {
      String rsvpPath;
      if (!ensureEpubConverted(path, rsvpPath)) {
        return false;
      }

      refreshBookPaths();
      const int convertedIndex = pathIndexIn(bookPaths_, rsvpPath);
      if (convertedIndex < 0) {
        Serial.printf("[storage] Converted RSVP not found in refreshed library: %s\n",
                      rsvpPath.c_str());
        return false;
      }

      path = rsvpPath;
      parsedIndex = static_cast<size_t>(convertedIndex);
    }

    File entry = SD_MMC.open(path);
    if (!entry || entry.isDirectory()) {
      if (entry) {
        entry.close();
      }
      continue;
    }

    const uint32_t parseStartedMs = millis();
    const bool parsed = parseFile(entry, book, hasRsvpExtension(path));
    const uint32_t parseElapsedMs = millis() - parseStartedMs;
    if (parsed) {
      if (book.title.isEmpty()) {
        book.title = normalizeDisplayText(displayNameWithoutExtension(path));
      }
      Serial.printf("[storage] Loaded %u words and %u chapters from %s in %lu ms\n",
                    static_cast<unsigned int>(book.words.size()),
                    static_cast<unsigned int>(book.chapters.size()), path.c_str(),
                    static_cast<unsigned long>(parseElapsedMs));
      if (loadedPath != nullptr) {
        *loadedPath = path;
      }
      if (loadedIndex != nullptr) {
        *loadedIndex = parsedIndex;
      }
      entry.close();
      return true;
    }

    book.clear();
    entry.close();
  }

  Serial.println("[storage] No readable book files found under /books");
  return false;
}

bool StorageManager::readIndexedMetadata(const String &path, BookMetadata &metadata,
                                         IndexedBookStore::Header *headerOut) {
  metadata.clear();

  IndexedBookStore::Header header;
  if (!readIndexHeader(path, header)) {
    if (fileExistsAndHasBytes(indexedIndexPathFor(path))) {
      Serial.printf("[storage-index] invalid index header: %s\n",
                    indexedIndexPathFor(path).c_str());
    }
    return false;
  }

  File source = SD_MMC.open(path, FILE_READ);
  if (!source || source.isDirectory()) {
    if (source) {
      source.close();
    }
    Serial.printf("[storage-index] source missing while validating index: %s\n", path.c_str());
    return false;
  }

  const size_t sourceBytes = source.size();
  const uint32_t actualFingerprint =
      sourceBytes <= UINT32_MAX ? sourceFingerprint(source, static_cast<uint32_t>(sourceBytes))
                                : 0;
  source.close();
  if (sourceBytes > UINT32_MAX || header.sourceSize != static_cast<uint32_t>(sourceBytes) ||
      header.sourceFingerprint != actualFingerprint) {
    Serial.printf("[storage-index] stale index: %s size=%lu/%lu fingerprint=%08lx/%08lx\n",
                  path.c_str(), static_cast<unsigned long>(header.sourceSize),
                  static_cast<unsigned long>(sourceBytes),
                  static_cast<unsigned long>(header.sourceFingerprint),
                  static_cast<unsigned long>(actualFingerprint));
    return false;
  }

  File data = SD_MMC.open(indexedDataPathFor(path), FILE_READ);
  if (!data || data.isDirectory() || data.size() < header.dataSize) {
    const size_t dataBytes = data ? data.size() : 0;
    if (data) {
      data.close();
    }
    Serial.printf("[storage-index] data sidecar invalid: %s size=%lu expected=%lu\n",
                  indexedDataPathFor(path).c_str(), static_cast<unsigned long>(dataBytes),
                  static_cast<unsigned long>(header.dataSize));
    return false;
  }
  data.close();

  File indexFile = SD_MMC.open(indexedIndexPathFor(path), FILE_READ);
  if (!indexFile || indexFile.isDirectory()) {
    if (indexFile) {
      indexFile.close();
    }
    Serial.printf("[storage-index] index sidecar cannot reopen: %s\n",
                  indexedIndexPathFor(path).c_str());
    return false;
  }

  metadata.wordCount = header.wordCount;
  metadata.title = readRsvpDirectiveValue(path, "@title");
  metadata.author = readRsvpDirectiveValue(path, "@author");
  if (metadata.title.isEmpty()) {
    metadata.title = normalizeDisplayText(displayNameWithoutExtension(path));
  }

  if (header.paragraphCount > 0) {
    metadata.paragraphStarts.reserve(header.paragraphCount);
    if (!indexFile.seek(header.paragraphsOffset)) {
      indexFile.close();
      metadata.clear();
      Serial.printf("[storage-index] paragraph section seek failed: %s offset=%lu\n",
                    indexedIndexPathFor(path).c_str(),
                    static_cast<unsigned long>(header.paragraphsOffset));
      return false;
    }
    for (uint32_t i = 0; i < header.paragraphCount; ++i) {
      uint32_t wordIndex = 0;
      if (!readExact(indexFile, &wordIndex, sizeof(wordIndex))) {
        indexFile.close();
        metadata.clear();
        Serial.printf("[storage-index] paragraph section read failed: %s item=%lu\n",
                      indexedIndexPathFor(path).c_str(), static_cast<unsigned long>(i));
        return false;
      }
      metadata.paragraphStarts.push_back(wordIndex);
    }
  }

  if (header.chapterCount > 0) {
    metadata.chapters.reserve(header.chapterCount);
    if (!indexFile.seek(header.chaptersOffset)) {
      indexFile.close();
      metadata.clear();
      Serial.printf("[storage-index] chapter section seek failed: %s offset=%lu\n",
                    indexedIndexPathFor(path).c_str(),
                    static_cast<unsigned long>(header.chaptersOffset));
      return false;
    }
    for (uint32_t i = 0; i < header.chapterCount; ++i) {
      IndexedBookStore::ChapterRecord record;
      if (!readExact(indexFile, &record, sizeof(record))) {
        indexFile.close();
        metadata.clear();
        Serial.printf("[storage-index] chapter section read failed: %s item=%lu\n",
                      indexedIndexPathFor(path).c_str(), static_cast<unsigned long>(i));
        return false;
      }
      ChapterMarker marker;
      marker.wordIndex = record.wordIndex;
      const uint32_t titleLength =
          std::min<uint32_t>(record.titleLength, sizeof(record.title));
      String title;
      title.reserve(titleLength);
      for (uint32_t j = 0; j < titleLength; ++j) {
        title += record.title[j];
      }
      marker.title = title;
      metadata.chapters.push_back(marker);
    }
  }

  indexFile.close();
  if (metadata.wordCount > 0 && metadata.paragraphStarts.empty()) {
    metadata.paragraphStarts.push_back(0);
  }
  if (headerOut != nullptr) {
    *headerOut = header;
  }
  if (metadata.wordCount == 0) {
    Serial.printf("[storage-index] index has no words: %s\n", indexedIndexPathFor(path).c_str());
    return false;
  }
  return true;
}

bool StorageManager::buildIndexedBook(const String &path, BookMetadata &metadata,
                                      bool rsvpFormat) {
  metadata.clear();

  File source = SD_MMC.open(path, FILE_READ);
  if (!source || source.isDirectory()) {
    if (source) {
      source.close();
    }
    Serial.printf("[storage-index] cannot open source: %s\n", path.c_str());
    notifyStatus("Index failed", displayNameForPath(path).c_str(), "File unreadable", 100);
    return false;
  }

  const size_t sourceBytes = source.size();
  if (sourceBytes == 0 || sourceBytes > UINT32_MAX) {
    source.close();
    Serial.printf("[storage-index] unsupported source size: %s (%lu bytes)\n",
                  path.c_str(), static_cast<unsigned long>(sourceBytes));
    notifyStatus("Index failed", displayNameForPath(path).c_str(),
                 sourceBytes == 0 ? "No readable words" : "Book too large", 100);
    return false;
  }
  const uint32_t fingerprint = sourceFingerprint(source, static_cast<uint32_t>(sourceBytes));
  if (!source.seek(0)) {
    source.close();
    Serial.printf("[storage-index] source rewind failed: %s\n", path.c_str());
    notifyStatus("Index failed", displayNameForPath(path).c_str(), "Source read failed", 100);
    return false;
  }

  const String label = displayNameForPath(path);
  notifyStatus("Indexing book", label.c_str(), "Building word index", 0);

  const String indexPath = indexedIndexPathFor(path);
  const String dataPath = indexedDataPathFor(path);
  const String tmpIndexPath = indexedTempPathFor(indexPath);
  const String tmpDataPath = indexedTempPathFor(dataPath);
  SD_MMC.remove(tmpIndexPath);
  SD_MMC.remove(tmpDataPath);

  File indexFile = SD_MMC.open(tmpIndexPath, FILE_WRITE);
  File dataFile = SD_MMC.open(tmpDataPath, FILE_WRITE);
  if (!indexFile || !dataFile) {
    if (indexFile) {
      indexFile.close();
    }
    if (dataFile) {
      dataFile.close();
    }
    source.close();
    SD_MMC.remove(tmpIndexPath);
    SD_MMC.remove(tmpDataPath);
    notifyStatus("Index failed", label.c_str(), "SD write failed", 100);
    return false;
  }

  IndexedBookStore::Header header;
  if (!writeExact(indexFile, &header, sizeof(header))) {
    indexFile.close();
    dataFile.close();
    source.close();
    SD_MMC.remove(tmpIndexPath);
    SD_MMC.remove(tmpDataPath);
    notifyStatus("Index failed", label.c_str(), "SD write failed", 100);
    return false;
  }

  IndexedBuildContext context;
  context.indexFile = &indexFile;
  context.dataFile = &dataFile;
  context.metadata = &metadata;

  String line;
  line.reserve(256);
  bool paragraphPending = true;
  bool keepReading = true;
  bool parseFailed = false;
  ParseStats stats;

  constexpr size_t kBufSize = 4096;
  static uint8_t buf[kBufSize];
  size_t totalBytesRead = 0;
  size_t nextProgressBytes = 0;
  const uint32_t startedMs = millis();

  while (keepReading && source.available()) {
    const size_t bytesRead = source.read(buf, kBufSize);
    if (bytesRead == 0) {
      break;
    }
    totalBytesRead += bytesRead;

    if (sourceBytes > 0 && totalBytesRead >= nextProgressBytes) {
      const int progress = static_cast<int>(
          std::min<size_t>(90, (totalBytesRead * 90UL) / sourceBytes));
      notifyStatus("Indexing book", label.c_str(), "Building word index", progress);
      nextProgressBytes = totalBytesRead + 256 * 1024;
    }
    yield();

    for (size_t i = 0; i < bytesRead && keepReading; ++i) {
      const char c = static_cast<char>(buf[i]);

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        keepReading = rsvpFormat
                          ? processIndexedRsvpLine(line, context, paragraphPending, &stats)
                          : processIndexedBookLine(line, context, paragraphPending, &stats);
        if (!keepReading && hasBookWordLimit()) {
          Serial.printf("[storage-index] Reached %lu word limit, truncating book\n",
                        static_cast<unsigned long>(kMaxBookWords));
        } else if (!keepReading && (stats.memoryLow || context.failed)) {
          parseFailed = true;
        }
        line = "";
        continue;
      }

      line += c;
      if (line.length() >= kMaxBookLineChars) {
        keepReading = rsvpFormat
                          ? processIndexedRsvpLine(line, context, paragraphPending, &stats)
                          : processIndexedBookLine(line, context, paragraphPending, &stats);
        ++stats.longLineSplits;
        if (!keepReading && (stats.memoryLow || context.failed)) {
          parseFailed = true;
        }
        line = "";
      }
    }
  }

  if (!line.isEmpty() && keepReading && !reachedBookWordLimit(context.wordCount)) {
    keepReading = rsvpFormat ? processIndexedRsvpLine(line, context, paragraphPending, &stats)
                             : processIndexedBookLine(line, context, paragraphPending, &stats);
    if (!keepReading && (stats.memoryLow || context.failed)) {
      parseFailed = true;
    }
  }

  if (stats.longLineSplits > 0 || stats.malformedUtf8 > 0 || stats.nonAsciiCodepoints > 0) {
    Serial.printf("[storage-index] Parse cleanup: long_lines=%u malformed_utf8=%u non_ascii=%u\n",
                  static_cast<unsigned int>(stats.longLineSplits),
                  static_cast<unsigned int>(stats.malformedUtf8),
                  static_cast<unsigned int>(stats.nonAsciiCodepoints));
  }

  if (parseFailed || context.wordCount == 0) {
    const char *detail = context.failure[0] == '\0' ? "No readable words" : context.failure;
    indexFile.close();
    dataFile.close();
    source.close();
    SD_MMC.remove(tmpIndexPath);
    SD_MMC.remove(tmpDataPath);
    metadata.clear();
    notifyStatus("Index failed", label.c_str(), detail, 100);
    return false;
  }

  if (metadata.paragraphStarts.empty()) {
    metadata.paragraphStarts.push_back(0);
  }
  if (metadata.title.isEmpty()) {
    metadata.title = normalizeDisplayText(displayNameWithoutExtension(path));
  }

  header.magic = IndexedBookStore::kMagic;
  header.version = IndexedBookStore::kVersion;
  header.headerSize = sizeof(IndexedBookStore::Header);
  header.recordSize = sizeof(IndexedBookStore::WordRecord);
  header.sourceSize = static_cast<uint32_t>(sourceBytes);
  header.sourceFingerprint = fingerprint;
  header.wordCount = context.wordCount;
  header.paragraphCount = static_cast<uint32_t>(metadata.paragraphStarts.size());
  header.chapterCount = static_cast<uint32_t>(metadata.chapters.size());
  header.recordsOffset = sizeof(IndexedBookStore::Header);
  header.paragraphsOffset =
      header.recordsOffset + header.wordCount * sizeof(IndexedBookStore::WordRecord);
  header.chaptersOffset = header.paragraphsOffset + header.paragraphCount * sizeof(uint32_t);
  header.dataSize = context.dataSize;

  if (!indexFile.seek(header.paragraphsOffset)) {
    parseFailed = true;
  }

  for (size_t i = 0; !parseFailed && i < metadata.paragraphStarts.size(); ++i) {
    const uint32_t wordIndex = static_cast<uint32_t>(metadata.paragraphStarts[i]);
    parseFailed = !writeExact(indexFile, &wordIndex, sizeof(wordIndex));
  }

  if (!parseFailed && !indexFile.seek(header.chaptersOffset)) {
    parseFailed = true;
  }

  for (size_t i = 0; !parseFailed && i < metadata.chapters.size(); ++i) {
    IndexedBookStore::ChapterRecord record;
    record.wordIndex = static_cast<uint32_t>(metadata.chapters[i].wordIndex);
    const String &title = metadata.chapters[i].title;
    record.titleLength = std::min<uint32_t>(title.length(), sizeof(record.title));
    for (uint32_t j = 0; j < record.titleLength; ++j) {
      record.title[j] = title[j];
    }
    parseFailed = !writeExact(indexFile, &record, sizeof(record));
  }

  if (!parseFailed && (!indexFile.seek(0) || !writeExact(indexFile, &header, sizeof(header)))) {
    parseFailed = true;
  }

  indexFile.close();
  dataFile.close();
  source.close();

  if (parseFailed) {
    SD_MMC.remove(tmpIndexPath);
    SD_MMC.remove(tmpDataPath);
    metadata.clear();
    notifyStatus("Index failed", label.c_str(), "SD write failed", 100);
    return false;
  }

  SD_MMC.remove(indexPath);
  SD_MMC.remove(dataPath);
  const bool renamed =
      SD_MMC.rename(tmpIndexPath, indexPath) && SD_MMC.rename(tmpDataPath, dataPath);
  if (!renamed) {
    SD_MMC.remove(tmpIndexPath);
    SD_MMC.remove(tmpDataPath);
    SD_MMC.remove(indexPath);
    SD_MMC.remove(dataPath);
    metadata.clear();
    notifyStatus("Index failed", label.c_str(), "Rename failed", 100);
    return false;
  }

  Serial.printf("[storage-index] Built %u words, %u chapters from %s in %lu ms\n",
                static_cast<unsigned int>(metadata.wordCount),
                static_cast<unsigned int>(metadata.chapters.size()), path.c_str(),
                static_cast<unsigned long>(millis() - startedMs));
  notifyStatus("Index ready", label.c_str(), "Book ready", 100);
  return true;
}

bool StorageManager::ensureIndexedBook(const String &path, BookMetadata &metadata,
                                       bool rsvpFormat, bool allowIndexBuild) {
  if (readIndexedMetadata(path, metadata)) {
    notifyStatus("Opening book", displayNameForPath(path).c_str(), "Index is current", 45);
    return true;
  }

  if (!allowIndexBuild) {
    notifyStatus("Index needed", displayNameForPath(path).c_str(), "Open from library", 100);
    return false;
  }

  Serial.printf("[storage-index] rebuilding missing/stale index: %s\n", path.c_str());
  notifyStatus("Opening book", displayNameForPath(path).c_str(), "Index needs rebuild", 20);
  if (!buildIndexedBook(path, metadata, rsvpFormat)) {
    return false;
  }
  if (!readIndexedMetadata(path, metadata)) {
    Serial.printf("[storage-index] freshly built index failed validation: %s\n", path.c_str());
    notifyStatus("Index failed", displayNameForPath(path).c_str(), "Validation failed", 100);
    return false;
  }
  return true;
}

bool StorageManager::loadIndexedBook(size_t index, IndexedBookStore &store,
                                     BookMetadata &metadata, String *loadedPath,
                                     size_t *loadedIndex, bool allowIndexBuild,
                                     bool allowEpubConversion) {
  metadata.clear();

  if (!mounted_) {
    Serial.println("[storage] SD not mounted, cannot load indexed book");
    notifyStatus("Book open failed", "SD not mounted", "Check card", 100);
    return false;
  }

  if (!sdCard_.directoryExists(kBooksPath)) {
    Serial.println("[storage] /books directory not found");
    notifyStatus("Book open failed", "Folders missing", "Run SD check", 100);
    return false;
  }

  if (bookPaths_.empty()) {
    refreshBookPaths(false);
  }
  if (bookPaths_.empty()) {
    Serial.println("[storage] No readable .rsvp, .txt, or .epub books found under /books");
    notifyStatus("Book open failed", "No books found", "Add books to SD", 100);
    return false;
  }

  if (index >= bookPaths_.size()) {
    Serial.printf("[storage] Book index %u out of range\n", static_cast<unsigned int>(index));
    notifyStatus("Book open failed", "Library changed", "Open list again", 100);
    return false;
  }

  String path = bookPaths_[index];
  size_t parsedIndex = index;
  if (hasEpubExtension(path)) {
    if (!allowEpubConversion) {
      notifyStatus("Index needed", displayNameForPath(path).c_str(), "Open from library", 100);
      return false;
    }

    String rsvpPath;
    if (!ensureEpubConverted(path, rsvpPath)) {
      return false;
    }

    refreshBookPaths();
    const int convertedIndex = pathIndexIn(bookPaths_, rsvpPath);
    if (convertedIndex < 0) {
      Serial.printf("[storage] Converted RSVP not found in refreshed library: %s\n",
                    rsvpPath.c_str());
      notifyStatus("Book open failed", displayNameForPath(path).c_str(),
                   "Conversion cache missing", 100);
      return false;
    }

    path = rsvpPath;
    parsedIndex = static_cast<size_t>(convertedIndex);
  }

  File entry = SD_MMC.open(path, FILE_READ);
  if (!entry || entry.isDirectory()) {
    if (entry) {
      entry.close();
    }
    Serial.printf("[storage] Selected book is not readable: %s\n", path.c_str());
    notifyStatus("Book open failed", displayNameForPath(path).c_str(), "File unreadable", 100);
    return false;
  }
  entry.close();

  notifyStatus("Opening book", displayNameForPath(path).c_str(), "Checking index", 12);
  if (!ensureIndexedBook(path, metadata, hasRsvpExtension(path), allowIndexBuild)) {
    metadata.clear();
    return false;
  }

  IndexedBookStore::Header header;
  if (!readIndexedMetadata(path, metadata, &header)) {
    metadata.clear();
    notifyStatus("Book open failed", displayNameForPath(path).c_str(), "Index invalid", 100);
    return false;
  }

  notifyStatus("Opening book", displayNameForPath(path).c_str(), "Opening word cache", 80);
  if (!store.open(indexedIndexPathFor(path), indexedDataPathFor(path), header)) {
    metadata.clear();
    notifyStatus("Book open failed", displayNameForPath(path).c_str(), "Index unreadable", 100);
    return false;
  }

  if (loadedPath != nullptr) {
    *loadedPath = path;
  }
  if (loadedIndex != nullptr) {
    *loadedIndex = parsedIndex;
  }

  Serial.printf("[storage] Opened indexed book %s: %u words, %u chapters\n", path.c_str(),
                static_cast<unsigned int>(metadata.wordCount),
                static_cast<unsigned int>(metadata.chapters.size()));
  return true;
}

bool StorageManager::loadBookWords(size_t index, std::vector<String> &words, String *loadedPath,
                                   size_t *loadedIndex) {
  BookContent book;
  if (!loadBookContent(index, book, loadedPath, loadedIndex)) {
    words.clear();
    return false;
  }

  words = std::move(book.words);
  return true;
}

StorageManager::DiagnosticResult StorageManager::diagnoseSdCard() {
  DiagnosticResult result;
  notifyStatus("SD check", "Mounting card", "", 5);

  if (!mounted_) {
    mounted_ = sdCard_.mount();
  }

  result.mounted = mounted_;
  if (!mounted_) {
    result.summary = "Card not mounted";
    result.detail = "Format FAT32 MBR";
    Serial.println("[sd-check] mount failed; likely format/partition issue, seating, or card fault");
    return result;
  }

  result.sizeMb = sdCard_.sizeMb();
  result.cardType = sdCard_.typeLabel();
  Serial.printf("[sd-check] mounted type=%s size=%llu MB\n", result.cardType.c_str(),
                result.sizeMb);

  notifyStatus("SD check", "Checking folders", "", 30);
  result.booksDirectory = sdCard_.directoryExists(kBooksPath);
  result.bookFilesDirectory = sdCard_.directoryExists(kBookFilesPath);
  result.articleFilesDirectory = sdCard_.directoryExists(kArticleFilesPath);
  result.configDirectory = sdCard_.directoryExists("/config");
  if (!result.booksDirectory || !result.bookFilesDirectory || !result.articleFilesDirectory ||
      !result.configDirectory) {
    result.summary = "Folders missing";
    result.detail = "Can create layout";
    Serial.printf("[sd-check] v0.0.4 folders missing /books=%u /books/books=%u "
                  "/books/articles=%u /config=%u\n",
                  result.booksDirectory ? 1 : 0, result.bookFilesDirectory ? 1 : 0,
                  result.articleFilesDirectory ? 1 : 0, result.configDirectory ? 1 : 0);
    notifyStatus("SD check", "Folders missing", "Confirm repair", 38);
    return result;
  }

  notifyStatus("SD check", "Scanning /books", "", 45);
  bookPaths_ = collectBookPaths();
  rebuildBookMetadataCache();
  result.bookCount = bookPaths_.size();
  result.unsupportedCount = countUnsupportedBookFiles();

  notifyStatus("SD check", "Testing write", "", 70);
  result.writable = sdCard_.writeProbe(kBooksPath);
  result.booksWritable = sdCard_.writeProbe(kBookFilesPath);
  result.articlesWritable = sdCard_.writeProbe(kArticleFilesPath);
  result.configWritable = sdCard_.writeProbe("/config");
  if (!result.writable) {
    result.summary = "Write test failed";
    result.detail = "Format FAT32 MBR";
    Serial.println("[sd-check] /books write/delete probe failed");
    return result;
  }
  if (!result.booksWritable || !result.articlesWritable || !result.configWritable) {
    result.summary = "Folder write failed";
    result.detail = "Format FAT32 MBR";
    Serial.printf("[sd-check] folder write failed books=%u articles=%u config=%u\n",
                  result.booksWritable ? 1 : 0, result.articlesWritable ? 1 : 0,
                  result.configWritable ? 1 : 0);
    return result;
  }

  if (result.bookCount == 0) {
    result.summary = "No books found";
    if (result.unsupportedCount > 0) {
      result.detail = "Use .rsvp .txt .epub";
    } else {
      result.detail = "Upload to /books/books";
    }
    Serial.printf("[sd-check] no supported books; unsupported=%u\n",
                  static_cast<unsigned int>(result.unsupportedCount));
    return result;
  }

  result.summary = String(result.bookCount) + " books OK";
  result.detail = result.cardType + " " + String(static_cast<unsigned int>(result.sizeMb)) + " MB";
  Serial.printf("[sd-check] OK books=%u unsupported=%u writable=%u\n",
                static_cast<unsigned int>(result.bookCount),
                static_cast<unsigned int>(result.unsupportedCount), result.writable ? 1 : 0);
  return result;
}

bool StorageManager::repairSdCardFolders() {
  if (!mounted_) {
    Serial.println("[sd-check] folder repair skipped: card not mounted");
    return false;
  }

  Serial.println("[sd-check] repairing v0.0.4 folder layout");
  const bool rootWritable = sdCard_.writeProbe("/");
  Serial.printf("[sd-check] root write probe=%u\n", rootWritable ? 1 : 0);

  const bool booksOk = sdCard_.ensureDirectory(kBooksPath);
  const bool bookFilesOk = booksOk && sdCard_.ensureDirectory(kBookFilesPath);
  const bool articleFilesOk = booksOk && sdCard_.ensureDirectory(kArticleFilesPath);
  const bool configOk = sdCard_.ensureDirectory("/config");
  const bool ok = rootWritable && booksOk && bookFilesOk && articleFilesOk && configOk;
  if (ok) {
    Serial.println("[sd-check] repaired v0.0.4 folder layout");
  } else {
    Serial.printf("[sd-check] folder repair failed rootWritable=%u /books=%u /books/books=%u "
                  "/books/articles=%u /config=%u\n",
                  rootWritable ? 1 : 0, booksOk ? 1 : 0, bookFilesOk ? 1 : 0,
                  articleFilesOk ? 1 : 0, configOk ? 1 : 0);
  }
  return ok;
}

void StorageManager::refreshBookPaths(bool includeMetadata) {
  if (!mounted_) {
    clearBookCache();
    return;
  }

  notifyStatus("SD", "Reading library", "", 96);
  bookPaths_ = collectBookPaths();
  if (includeMetadata) {
    rebuildBookMetadataCache();
  } else {
    bookTitles_.clear();
    bookAuthors_.clear();
    Serial.printf("[storage] Metadata cache skipped for %u entries\n",
                  static_cast<unsigned int>(bookPaths_.size()));
  }

  size_t rsvpCount = 0;
  size_t textCount = 0;
  size_t pendingEpubCount = 0;
  for (const String &path : bookPaths_) {
    if (hasRsvpExtension(path)) {
      ++rsvpCount;
    } else if (hasTextExtension(path)) {
      ++textCount;
    } else if (hasEpubExtension(path)) {
      ++pendingEpubCount;
    }
  }

  Serial.printf("[storage] Library scan: %u books (%u rsvp, %u txt, %u pending epub)\n",
                static_cast<unsigned int>(bookPaths_.size()),
                static_cast<unsigned int>(rsvpCount), static_cast<unsigned int>(textCount),
                static_cast<unsigned int>(pendingEpubCount));
}

void StorageManager::rebuildBookMetadataCache() {
  bookTitles_.clear();
  bookAuthors_.clear();
  bookTitles_.reserve(bookPaths_.size());
  bookAuthors_.reserve(bookPaths_.size());

  const uint32_t startedMs = millis();
  size_t rsvpMetadataCount = 0;
  for (const String &path : bookPaths_) {
    String title;
    String author;

    // One SD header read per book, back to back; yield between books so a
    // large library cannot starve the loop task for seconds.
    yield();
    if (hasRsvpExtension(path)) {
      const RsvpDirectiveValues values = readRsvpDirectiveValues(path);
      title = values.title;
      author = values.author;
      ++rsvpMetadataCount;
    } else if (hasEpubExtension(path)) {
      author = epubLibraryLabel(path);
    }

    bookTitles_.push_back(title);
    bookAuthors_.push_back(author);
  }

  Serial.printf("[storage] Metadata cache: %u entries (%u rsvp) in %lu ms\n",
                static_cast<unsigned int>(bookPaths_.size()),
                static_cast<unsigned int>(rsvpMetadataCount),
                static_cast<unsigned long>(millis() - startedMs));
}

void StorageManager::clearBookCache() {
  bookPaths_.clear();
  bookTitles_.clear();
  bookAuthors_.clear();
}

bool StorageManager::parseFile(File &file, BookContent &book, bool rsvpFormat) {
  book.clear();
  const size_t initialReserve =
      std::min(kInitialWordReserveMax, static_cast<size_t>(file.size() / 8));
  if (initialReserve > 0) {
    book.words.reserve(initialReserve);
  }
  String line;
  line.reserve(256);
  bool paragraphPending = true;
  bool keepReading = true;
  bool parseFailed = false;
  ParseStats stats;
  const booktext::MemoryLowFn memLow = [] { return parseMemoryLow(); };

  constexpr size_t kBufSize = 4096;
  static uint8_t buf[kBufSize];

  while (keepReading && file.available()) {
    const size_t bytesRead = file.read(buf, kBufSize);
    if (bytesRead == 0) {
      break;
    }
    yield();

    for (size_t i = 0; i < bytesRead && keepReading; ++i) {
      const char c = static_cast<char>(buf[i]);

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending, &stats, memLow)
                                 : processBookLine(line, book, paragraphPending, &stats, memLow);
        if (!keepReading && hasBookWordLimit()) {
          Serial.printf("[storage] Reached %lu word limit, truncating book\n",
                        static_cast<unsigned long>(kMaxBookWords));
        } else if (!keepReading && stats.memoryLow) {
          parseFailed = true;
          Serial.printf("[storage] Book load stopped: low memory free8=%lu largest8=%lu\n",
                        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                        static_cast<unsigned long>(
                            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        }
        line = "";
        continue;
      }

      line += c;
      if (line.length() >= kMaxBookLineChars) {
        keepReading = rsvpFormat ? processRsvpLine(line, book, paragraphPending, &stats, memLow)
                                 : processBookLine(line, book, paragraphPending, &stats, memLow);
        ++stats.longLineSplits;
        if (!keepReading && stats.memoryLow) {
          parseFailed = true;
          Serial.printf("[storage] Book load stopped: low memory free8=%lu largest8=%lu\n",
                        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                        static_cast<unsigned long>(
                            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        }
        line = "";
      }
    }
  }

  if (!line.isEmpty() && keepReading && !reachedBookWordLimit(book.words.size())) {
    if (rsvpFormat) {
      keepReading = processRsvpLine(line, book, paragraphPending, &stats, memLow);
    } else {
      keepReading = processBookLine(line, book, paragraphPending, &stats, memLow);
    }
    if (!keepReading && stats.memoryLow) {
      parseFailed = true;
    }
  }

  if (stats.longLineSplits > 0 || stats.malformedUtf8 > 0 || stats.nonAsciiCodepoints > 0) {
    Serial.printf("[storage] Parse cleanup: long_lines=%u malformed_utf8=%u non_ascii=%u\n",
                  static_cast<unsigned int>(stats.longLineSplits),
                  static_cast<unsigned int>(stats.malformedUtf8),
                  static_cast<unsigned int>(stats.nonAsciiCodepoints));
  }

  if (parseFailed) {
    book.clear();
    notifyStatus("Book too large", "Memory limit reached", "Try converter/app", 100);
    return false;
  }

  if (!book.words.empty() && book.paragraphStarts.empty()) {
    book.paragraphStarts.push_back(0);
  }

  return !book.words.empty();
}
