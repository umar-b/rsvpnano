#include "text/EpubConvert.h"

#include <cctype>
#include <cstring>

#include "text/TextDecode.h"

namespace epubconvert {

namespace {

constexpr size_t kMaxTagChars = 512;
constexpr size_t kMaxEntityChars = 16;
constexpr size_t kOutputWrapWidth = 96;
constexpr size_t kBufferedTextFlushThreshold = 220;

bool isWhitespace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

String toLowerCopy(String value) {
  value.toLowerCase();
  return value;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

void appendNormalizedChar(String &target, char c) {
  if (c == '\r' || c == '\n' || c == '\t') {
    c = ' ';
  }

  if (isWhitespace(c)) {
    if (!target.isEmpty() && target[target.length() - 1] != ' ') {
      target += ' ';
    }
    return;
  }

  target += c;
}

bool hasReadableText(const String &token) {
  for (size_t i = 0; i < token.length(); ++i) {
    const uint8_t value = static_cast<uint8_t>(token[i]);
    if (std::isalnum(value) != 0 || value >= 0x80) {
      return true;
    }
  }
  return false;
}

bool isReadableTextChar(char c) {
  const uint8_t value = static_cast<uint8_t>(c);
  return std::isalnum(value) != 0 || value >= 0x80;
}

bool isInlineWordHyphen(const String &text, size_t index) {
  if (index == 0 || index + 1 >= text.length() || text[index] != '-') {
    return false;
  }
  if (text[index - 1] == '-' || text[index + 1] == '-') {
    return false;
  }
  return isReadableTextChar(text[index - 1]) && isReadableTextChar(text[index + 1]);
}

bool isHyphenToken(const String &token) {
  if (token.isEmpty()) {
    return false;
  }
  for (size_t i = 0; i < token.length(); ++i) {
    if (token[i] != '-') {
      return false;
    }
  }
  return true;
}

bool isEllipsisToken(const String &token) {
  if (token.length() < 3) {
    return false;
  }
  for (size_t i = 0; i < token.length(); ++i) {
    if (token[i] != '.') {
      return false;
    }
  }
  return true;
}

bool hasReadableOrRhythmText(const String &token) {
  return hasReadableText(token) || isHyphenToken(token);
}

String tagNameFromTag(const String &tag, bool &closing, bool &selfClosing) {
  closing = false;
  selfClosing = false;

  size_t position = 1;
  while (position < tag.length() && isWhitespace(tag[position])) {
    ++position;
  }
  if (position < tag.length() && tag[position] == '/') {
    closing = true;
    ++position;
  }
  while (position < tag.length() && isWhitespace(tag[position])) {
    ++position;
  }

  const size_t start = position;
  while (position < tag.length()) {
    const char c = tag[position];
    if (!(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == ':' || c == '-' || c == '_')) {
      break;
    }
    ++position;
  }

  String name = tag.substring(start, position);
  name.toLowerCase();

  for (int i = tag.length() - 1; i >= 0; --i) {
    if (isWhitespace(tag[i]) || tag[i] == '>') {
      continue;
    }
    selfClosing = tag[i] == '/';
    break;
  }

  return name;
}

bool isSkipTag(const String &name) {
  return name == "head" || name == "script" || name == "style" || name == "svg" ||
         name == "math" || name == "nav";
}

bool isHeadingTag(const String &name) {
  return name.length() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6';
}

bool isBlockTag(const String &name) {
  return name == "p" || name == "div" || name == "section" || name == "article" ||
         name == "blockquote" || name == "li" || name == "tr" || name == "br" ||
         name == "hr" || name == "dd" || name == "dt";
}

String parseDcMetadata(const String &opfXml, const char *tagName) {
  const String openTag = String("<dc:") + tagName;
  const String closeTag = String("</dc:") + tagName;
  int position = 0;
  while (position >= 0) {
    position = opfXml.indexOf(openTag, position);
    if (position < 0) {
      break;
    }

    const int openEnd = opfXml.indexOf('>', position);
    if (openEnd < 0) {
      break;
    }
    const int closeStart = opfXml.indexOf(closeTag, openEnd + 1);
    if (closeStart < 0) {
      break;
    }

    const String value = plainTextFromXmlFragment(opfXml.substring(openEnd + 1, closeStart));
    if (!value.isEmpty()) {
      return value;
    }

    position = closeStart + 1;
  }

  return "";
}

}  // namespace

String normalizeZipName(String path) {
  path.replace('\\', '/');
  while (path.startsWith("/")) {
    path.remove(0, 1);
  }
  return path;
}

String percentDecodePath(const String &path) {
  String decoded;
  decoded.reserve(path.length());

  for (size_t i = 0; i < path.length(); ++i) {
    if (path[i] == '%' && i + 2 < path.length()) {
      const int high = hexValue(path[i + 1]);
      const int low = hexValue(path[i + 2]);
      if (high >= 0 && low >= 0) {
        decoded += static_cast<char>((high << 4) | low);
        i += 2;
        continue;
      }
    }
    decoded += path[i];
  }

  return decoded;
}

String collapseZipPath(const String &path) {
  std::vector<String> parts;
  size_t start = 0;

  while (start <= path.length()) {
    int separator = path.indexOf('/', start);
    if (separator < 0) {
      separator = path.length();
    }

    String part = path.substring(start, separator);
    if (part == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else if (!part.isEmpty() && part != ".") {
      parts.push_back(part);
    }

    if (static_cast<size_t>(separator) >= path.length()) {
      break;
    }
    start = static_cast<size_t>(separator) + 1;
  }

  String collapsed;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      collapsed += "/";
    }
    collapsed += parts[i];
  }
  return collapsed;
}

String directoryForPath(const String &path) {
  const int separator = path.lastIndexOf('/');
  if (separator < 0) {
    return "";
  }
  return path.substring(0, separator + 1);
}

String resolveZipPath(const String &baseDirectory, const String &href) {
  String path = href;

  int fragment = path.indexOf('#');
  if (fragment >= 0) {
    path = path.substring(0, fragment);
  }
  int query = path.indexOf('?');
  if (query >= 0) {
    path = path.substring(0, query);
  }

  path = percentDecodePath(path);
  path = normalizeZipName(path);
  if (!href.startsWith("/")) {
    path = baseDirectory + path;
  }

  return collapseZipPath(path);
}

String basenameWithoutExtension(const String &path) {
  const int separator = path.lastIndexOf('/');
  String name = separator >= 0 ? path.substring(separator + 1) : path;
  const int dot = name.lastIndexOf('.');
  if (dot > 0) {
    name = name.substring(0, dot);
  }
  name.trim();
  return name.isEmpty() ? String("Untitled") : name;
}

String attributeValue(const String &tag, const char *name) {
  const String key(name);
  int position = 0;

  while (position >= 0 && static_cast<size_t>(position) < tag.length()) {
    position = tag.indexOf(key, position);
    if (position < 0) {
      return "";
    }

    const bool boundaryBefore =
        position == 0 || isWhitespace(tag[position - 1]) || tag[position - 1] == '<' ||
        tag[position - 1] == '/';
    int afterName = position + key.length();
    if (!boundaryBefore ||
        (static_cast<size_t>(afterName) < tag.length() &&
         !(isWhitespace(tag[afterName]) || tag[afterName] == '='))) {
      position = afterName;
      continue;
    }

    while (static_cast<size_t>(afterName) < tag.length() && isWhitespace(tag[afterName])) {
      ++afterName;
    }
    if (static_cast<size_t>(afterName) >= tag.length() || tag[afterName] != '=') {
      position = afterName;
      continue;
    }
    ++afterName;
    while (static_cast<size_t>(afterName) < tag.length() && isWhitespace(tag[afterName])) {
      ++afterName;
    }
    if (static_cast<size_t>(afterName) >= tag.length()) {
      return "";
    }

    const char quote = tag[afterName];
    if (quote == '"' || quote == '\'') {
      const int end = tag.indexOf(quote, afterName + 1);
      if (end < 0) {
        return "";
      }
      return tag.substring(afterName + 1, end);
    }

    int end = afterName;
    while (static_cast<size_t>(end) < tag.length() && !isWhitespace(tag[end]) && tag[end] != '>') {
      ++end;
    }
    return tag.substring(afterName, end);
  }

  return "";
}

String plainTextFromXmlFragment(const String &fragment) {
  String text;
  text.reserve(std::min<size_t>(fragment.length(), 160));

  for (size_t i = 0; i < fragment.length(); ++i) {
    const char c = fragment[i];
    if (c == '<') {
      const int tagEnd = fragment.indexOf('>', i + 1);
      if (tagEnd < 0) {
        break;
      }
      i = tagEnd;
      appendNormalizedChar(text, ' ');
      continue;
    }

    if (c == '&') {
      const int entityEnd = fragment.indexOf(';', i + 1);
      if (entityEnd > 0 && entityEnd - static_cast<int>(i) <= 12) {
        const String decoded = textdecode::decodedEntityText(fragment.substring(i + 1, entityEnd));
        for (size_t decodedIndex = 0; decodedIndex < decoded.length(); ++decodedIndex) {
          appendNormalizedChar(text, decoded[decodedIndex]);
        }
        i = entityEnd;
        continue;
      }
    }

    appendNormalizedChar(text, c);
  }

  text.trim();
  return textdecode::normalizeDisplayText(text);
}

String parseRootfilePath(const String &containerXml) {
  int position = 0;
  while (position >= 0) {
    position = containerXml.indexOf("<rootfile", position);
    if (position < 0) {
      break;
    }

    const int end = containerXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = containerXml.substring(position, end + 1);
    const String path = attributeValue(tag, "full-path");
    if (!path.isEmpty()) {
      return normalizeZipName(path);
    }

    position = end + 1;
  }

  return "";
}

String parseBookTitle(const String &opfXml) { return parseDcMetadata(opfXml, "title"); }

String parseBookAuthor(const String &opfXml) { return parseDcMetadata(opfXml, "creator"); }

std::vector<ManifestItem> parseManifestItems(const String &opfXml, const String &opfBaseDir) {
  std::vector<ManifestItem> items;
  int position = 0;

  while (position >= 0) {
    position = opfXml.indexOf("<item", position);
    if (position < 0) {
      break;
    }

    const int afterName = position + 5;
    if (static_cast<size_t>(afterName) < opfXml.length() &&
        !isWhitespace(opfXml[afterName]) && opfXml[afterName] != '/' &&
        opfXml[afterName] != '>') {
      position = afterName;
      continue;
    }

    const int end = opfXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = opfXml.substring(position, end + 1);
    ManifestItem item;
    item.id = attributeValue(tag, "id");
    item.path = resolveZipPath(opfBaseDir, attributeValue(tag, "href"));
    item.mediaType = attributeValue(tag, "media-type");

    if (!item.id.isEmpty() && !item.path.isEmpty()) {
      items.push_back(item);
    }

    position = end + 1;
  }

  return items;
}

std::vector<String> parseSpineIds(const String &opfXml) {
  std::vector<String> ids;
  int position = 0;

  while (position >= 0) {
    position = opfXml.indexOf("<itemref", position);
    if (position < 0) {
      break;
    }

    const int end = opfXml.indexOf('>', position);
    if (end < 0) {
      break;
    }

    const String tag = opfXml.substring(position, end + 1);
    const String idref = attributeValue(tag, "idref");
    if (!idref.isEmpty()) {
      ids.push_back(idref);
    }

    position = end + 1;
  }

  return ids;
}

const ManifestItem *findManifestItem(const std::vector<ManifestItem> &items, const String &id) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].id == id) {
      return &items[i];
    }
  }
  return nullptr;
}

bool isContentDocument(const ManifestItem &item) {
  const String mediaType = toLowerCopy(item.mediaType);
  const String path = toLowerCopy(item.path);
  return mediaType == "application/xhtml+xml" || mediaType == "text/html" ||
         path.endsWith(".xhtml") || path.endsWith(".html") || path.endsWith(".htm");
}

bool reachedWordLimit(size_t wordCount, size_t maxWords) {
  return maxWords > 0 && wordCount >= maxWords;
}

void writeRsvpHeader(const ByteSink &sink, const char *converterVersion, const String &title,
                     const String &author, const String &sourcePath) {
  auto line = [&sink](const String &text) {
    sink(text.c_str(), text.length());
    sink("\r\n", 2);
  };

  line("@rsvp 1");
  line(String("@converter ") + converterVersion);
  line(String("@title ") + title);
  if (!author.isEmpty()) {
    line(String("@author ") + author);
  }
  line(String("@source ") + sourcePath);
  line("");
}

RsvpWriter::RsvpWriter(ByteSink sink, size_t &wordCount, size_t maxWords,
                       String &lastChapterTitle, Yield yield)
    : sink_(std::move(sink)),
      wordCount_(wordCount),
      maxWords_(maxWords),
      lastChapterTitle_(lastChapterTitle),
      yield_(std::move(yield)) {
  line_.reserve(160);
  heading_.reserve(80);
  tag_.reserve(96);
  entity_.reserve(16);
}

void RsvpWriter::emit(const char *data, size_t length) { sink_(data, length); }

void RsvpWriter::emitLine(const String &text) {
  emit(text.c_str(), text.length());
  emit("\r\n", 2);
}

void RsvpWriter::maybeYield(size_t counter, size_t mask) {
  if (yield_ && (counter & mask) == 0) {
    yield_();
  }
}

bool RsvpWriter::write(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    maybeYield(i, 0x3FF);
    if (!processChar(static_cast<char>(data[i]))) {
      return false;
    }
  }

  return true;
}

bool RsvpWriter::finish() {
  mode_ = Mode::Text;
  return flushLine();
}

bool RsvpWriter::writeBodyLine(const String &line) {
  const String normalizedLine = textdecode::normalizeDisplayText(line);
  String token;
  String pendingToken;
  String outputLine;
  token.reserve(32);
  pendingToken.reserve(32);

  auto flushOutputLine = [&]() {
    if (outputLine.isEmpty()) {
      return;
    }
    // Body lines that start with '@' would read as directives; the reader
    // treats a doubled '@' as the escape.
    if (outputLine.startsWith("@")) {
      emit("@", 1);
    }
    emitLine(outputLine);
    outputLine = "";
  };

  auto writeToken = [&](const String &value) -> bool {
    if (value.isEmpty() || !hasReadableOrRhythmText(value)) {
      return true;
    }

    if (epubconvert::reachedWordLimit(wordCount_, maxWords_)) {
      flushOutputLine();
      return false;
    }

    if (outputLine.length() + value.length() + 1 > kOutputWrapWidth) {
      flushOutputLine();
    }

    if (!outputLine.isEmpty()) {
      outputLine += ' ';
    }
    outputLine += value;
    ++wordCount_;
    return true;
  };

  auto flushPending = [&]() -> bool {
    if (pendingToken.isEmpty()) {
      return true;
    }
    const bool ok = writeToken(pendingToken);
    pendingToken = "";
    return ok;
  };

  auto finishToken = [&](String value) -> bool {
    value.trim();
    if (value.isEmpty()) {
      return true;
    }

    if (isEllipsisToken(value)) {
      if (!pendingToken.isEmpty()) {
        pendingToken += "...";
      }
      return true;
    }

    if (isHyphenToken(value)) {
      return flushPending() && writeToken("-");
    }

    if (!flushPending()) {
      return false;
    }
    pendingToken = value;
    return true;
  };

  auto flushToken = [&]() -> bool {
    if (token.isEmpty()) {
      return true;
    }
    const bool ok = finishToken(token);
    token = "";
    return ok;
  };

  bool ok = true;
  for (size_t i = 0; ok && i < normalizedLine.length(); ++i) {
    maybeYield(i, 0x7F);

    const char c = normalizedLine[i];
    if (isWhitespace(c)) {
      ok = flushToken();
      continue;
    }

    if (c == '-') {
      if (isInlineWordHyphen(normalizedLine, i)) {
        token += c;
        continue;
      }
      ok = flushToken() && finishToken("-");
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '-') {
        ++i;
      }
      continue;
    }

    if (c == '.' && i + 2 < normalizedLine.length() && normalizedLine[i + 1] == '.' &&
        normalizedLine[i + 2] == '.') {
      token += "...";
      i += 2;
      while (i + 1 < normalizedLine.length() && normalizedLine[i + 1] == '.') {
        ++i;
      }
      ok = flushToken();
      continue;
    }

    token += c;
  }

  if (ok) {
    ok = flushToken() && flushPending();
    flushOutputLine();
    ok = ok && !epubconvert::reachedWordLimit(wordCount_, maxWords_);
  } else {
    flushOutputLine();
  }

  if (!ok) {
    // Every false path above is the word limit; record it here so callers
    // never misread a limit stop as a failure.
    reachedWordLimit_ = true;
  }
  return ok;
}

bool RsvpWriter::flushWordAlignedPrefix() {
  line_.trim();
  if (line_.isEmpty()) {
    line_ = "";
    return true;
  }

  int split = static_cast<int>(line_.length()) - 1;
  while (split >= 0 && !isWhitespace(line_[split])) {
    --split;
  }

  if (split < 0) {
    return true;
  }

  String prefix = line_.substring(0, split);
  String remainder = line_.substring(split + 1);
  prefix.trim();
  remainder.trim();

  if (prefix.isEmpty()) {
    line_ = remainder;
    return true;
  }

  const bool keepGoing = writeBodyLine(prefix);
  line_ = remainder;
  return keepGoing;
}

bool RsvpWriter::writeChapterMarker(const String &title) {
  String cleaned = textdecode::normalizeDisplayText(title);
  cleaned.trim();
  if (cleaned.isEmpty() || cleaned == lastChapterTitle_) {
    return true;
  }

  emit("@chapter ", 9);
  emitLine(cleaned);
  lastChapterTitle_ = cleaned;
  return true;
}

bool RsvpWriter::flushLine() {
  line_.trim();
  if (line_.isEmpty()) {
    return true;
  }

  const bool keepGoing = writeBodyLine(line_);
  line_ = "";
  return keepGoing;
}

void RsvpWriter::appendToActiveText(char c) {
  if (inHeading_) {
    appendNormalizedChar(heading_, c);
    return;
  }

  appendNormalizedChar(line_, c);
}

bool RsvpWriter::processDecodedText(char c) {
  if (skipDepth_ > 0) {
    return true;
  }

  appendToActiveText(c);
  if (!inHeading_ && line_.length() > kBufferedTextFlushThreshold) {
    return flushWordAlignedPrefix();
  }

  return true;
}

bool RsvpWriter::processTextChar(char c) {
  if (c == '<') {
    tag_ = "<";
    mode_ = Mode::Tag;
    return true;
  }

  if (c == '&') {
    if (skipDepth_ > 0) {
      return true;
    }
    entity_ = "";
    mode_ = Mode::Entity;
    return true;
  }

  return processDecodedText(c);
}

bool RsvpWriter::processTag(const String &tag) {
  bool closing = false;
  bool selfClosing = false;
  const String name = tagNameFromTag(tag, closing, selfClosing);

  if (name.isEmpty() || tag.startsWith("<!") || tag.startsWith("<?")) {
    return true;
  }

  if (skipDepth_ > 0) {
    if (!closing && isSkipTag(name) && !selfClosing) {
      ++skipDepth_;
    } else if (closing && isSkipTag(name)) {
      --skipDepth_;
    }
    return true;
  }

  if (isSkipTag(name) && !closing && !selfClosing) {
    if (!flushLine()) {
      return false;
    }
    skipDepth_ = 1;
    return true;
  }

  if (isHeadingTag(name)) {
    if (closing) {
      inHeading_ = false;
      const String cleanedHeading = plainTextFromXmlFragment(heading_);
      if (!writeChapterMarker(cleanedHeading)) {
        return false;
      }
      heading_ = "";
    } else if (!selfClosing) {
      if (!flushLine()) {
        return false;
      }
      inHeading_ = true;
      heading_ = "";
    }
    return true;
  }

  if (isBlockTag(name) && (closing || name == "br" || name == "hr" || name == "li")) {
    return flushLine();
  }
  if (isBlockTag(name)) {
    appendNormalizedChar(line_, ' ');
  }

  return true;
}

bool RsvpWriter::processEntityChar(char c) {
  if (c == ';') {
    mode_ = Mode::Text;
    const String decoded = textdecode::decodedEntityText(entity_);
    for (size_t decodedIndex = 0; decodedIndex < decoded.length(); ++decodedIndex) {
      if (!processDecodedText(decoded[decodedIndex])) {
        return false;
      }
    }
    return true;
  }

  if (c == '<') {
    mode_ = Mode::Text;
    if (!processDecodedText(' ')) {
      return false;
    }
    return processTextChar(c);
  }

  if (entity_.length() >= kMaxEntityChars || isWhitespace(c)) {
    mode_ = Mode::Text;
    return processDecodedText(' ');
  }

  entity_ += c;
  return true;
}

bool RsvpWriter::processCommentChar(char c) {
  commentTail_ += c;
  if (commentTail_.length() > 3) {
    commentTail_.remove(0, commentTail_.length() - 3);
  }

  if (commentTail_ == "-->") {
    commentTail_ = "";
    mode_ = Mode::Text;
  }

  return true;
}

bool RsvpWriter::processChar(char c) {
  switch (mode_) {
    case Mode::Text:
      return processTextChar(c);
    case Mode::Entity:
      return processEntityChar(c);
    case Mode::Comment:
      return processCommentChar(c);
    case Mode::Tag:
      tag_ += c;
      if (tag_ == "<!--") {
        tag_ = "";
        commentTail_ = "";
        mode_ = Mode::Comment;
        return true;
      }
      if (tag_.length() > kMaxTagChars) {
        tag_ = "";
        mode_ = Mode::Text;
        return processDecodedText(' ');
      }
      if (c == '>') {
        const String completedTag = tag_;
        tag_ = "";
        mode_ = Mode::Text;
        return processTag(completedTag);
      }
      return true;
  }

  return true;
}

}  // namespace epubconvert
