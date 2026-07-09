#pragma once

#include <Arduino.h>
#include <stddef.h>

#include <functional>
#include <vector>

// The pure kernel of EPUB conversion: package/manifest parsing and the
// streaming XHTML -> .rsvp word-stream writer. No SD card, no zip, no
// Serial -- bytes come in through write(), converted .rsvp bytes leave
// through a ByteSink, so the whole pipeline runs on the host with string
// literals. EpubConverter keeps the zip archive, miniz inflate, and the
// temp-file/marker dance as the adapter.
namespace epubconvert {

// Receives converted .rsvp output bytes (File::write on device, a String
// in tests). Line endings are CRLF, matching Arduino println.
using ByteSink = std::function<void(const char *data, size_t length)>;
// Optional cooperative-yield hook, called every few hundred processed chars.
using Yield = std::function<void()>;

// --- EPUB package structure (container.xml / OPF) ---

struct ManifestItem {
  String id;
  String path;
  String mediaType;
};

String normalizeZipName(String path);
String percentDecodePath(const String &path);
String collapseZipPath(const String &path);
String directoryForPath(const String &path);
// Resolves an OPF href (fragment/query stripped, percent-decoded) against
// the OPF's directory into a normalized zip entry path.
String resolveZipPath(const String &baseDirectory, const String &href);
String basenameWithoutExtension(const String &path);

// Value of a name="..." attribute inside a raw tag string; empty when absent.
String attributeValue(const String &tag, const char *name);
// Strips tags, decodes entities, collapses whitespace.
String plainTextFromXmlFragment(const String &fragment);

String parseRootfilePath(const String &containerXml);
String parseBookTitle(const String &opfXml);
String parseBookAuthor(const String &opfXml);
std::vector<ManifestItem> parseManifestItems(const String &opfXml, const String &opfBaseDir);
std::vector<String> parseSpineIds(const String &opfXml);
const ManifestItem *findManifestItem(const std::vector<ManifestItem> &items, const String &id);
bool isContentDocument(const ManifestItem &item);

// maxWords == 0 means unlimited.
bool reachedWordLimit(size_t wordCount, size_t maxWords);

void writeRsvpHeader(const ByteSink &sink, const char *converterVersion, const String &title,
                     const String &author, const String &sourcePath);

// Streaming XHTML -> .rsvp state machine. Feed raw (possibly chunk-split)
// XHTML bytes through write(); body lines, @chapter markers from headings,
// and word wrapping come out of the sink. wordCount and lastChapterTitle
// live in the caller because they persist across spine items while a writer
// is constructed per item.
class RsvpWriter {
 public:
  RsvpWriter(ByteSink sink, size_t &wordCount, size_t maxWords, String &lastChapterTitle,
             Yield yield = Yield());

  // false when the word limit stopped output (check reachedWordLimit()).
  bool write(const uint8_t *data, size_t length);
  bool finish();

  bool reachedWordLimit() const { return reachedWordLimit_; }

 private:
  enum class Mode {
    Text,
    Tag,
    Entity,
    Comment,
  };

  void emit(const char *data, size_t length);
  void emitLine(const String &text);
  void maybeYield(size_t counter, size_t mask);
  bool writeBodyLine(const String &line);
  bool flushWordAlignedPrefix();
  bool writeChapterMarker(const String &title);
  bool flushLine();
  void appendToActiveText(char c);
  bool processDecodedText(char c);
  bool processTextChar(char c);
  bool processTag(const String &tag);
  bool processEntityChar(char c);
  bool processCommentChar(char c);
  bool processChar(char c);

  ByteSink sink_;
  size_t &wordCount_;
  size_t maxWords_;
  String &lastChapterTitle_;
  Yield yield_;
  String line_;
  String heading_;
  String tag_;
  String entity_;
  String commentTail_;
  Mode mode_ = Mode::Text;
  bool inHeading_ = false;
  bool reachedWordLimit_ = false;
  int skipDepth_ = 0;
};

}  // namespace epubconvert
