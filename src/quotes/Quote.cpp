#include "quotes/Quote.h"

namespace quotes {

namespace {

// JSON-escape a string into an existing buffer (no surrounding quotes). Matches
// the minimal escaping the companion already does for its own JSON.
void appendJsonEscaped(String &out, const String &value) {
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          // Other control chars: drop rather than emit invalid JSON.
          break;
        }
        out += c;
        break;
    }
  }
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Find the value start for "key": in a flat JSON object. Returns the index just
// past the colon (skipping spaces), or -1 if the key is absent.
int valueStart(const String &line, const char *key) {
  String needle = String("\"") + key + "\":";
  const int at = line.indexOf(needle);
  if (at < 0) {
    return -1;
  }
  int i = at + static_cast<int>(needle.length());
  while (i < static_cast<int>(line.length()) && (line[i] == ' ' || line[i] == '\t')) {
    ++i;
  }
  return i;
}

// Read a JSON string value (with escapes) starting at the opening quote.
// Returns true and fills out on success.
bool readJsonString(const String &line, int quoteIndex, String &out) {
  if (quoteIndex < 0 || quoteIndex >= static_cast<int>(line.length()) ||
      line[quoteIndex] != '"') {
    return false;
  }
  out = "";
  int i = quoteIndex + 1;
  while (i < static_cast<int>(line.length())) {
    const char c = line[i];
    if (c == '"') {
      return true;
    }
    if (c == '\\' && i + 1 < static_cast<int>(line.length())) {
      const char next = line[i + 1];
      switch (next) {
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        default:
          out += next;
          break;
      }
      i += 2;
      continue;
    }
    out += c;
    ++i;
  }
  return false;  // unterminated string
}

}  // namespace

SentenceSpan extractSentence(size_t currentIndex, size_t wordCount,
                             const WordAtFn &wordAt, const EndsSentenceFn &endsSentence) {
  SentenceSpan span;
  if (wordCount == 0) {
    return span;
  }
  if (currentIndex >= wordCount) {
    currentIndex = wordCount - 1;
  }

  // Walk back to the start: the word after the previous sentence-ending word.
  size_t start = currentIndex;
  while (start > 0 && !endsSentence(start - 1)) {
    --start;
  }

  // Walk forward to the first word that ends a sentence at or after current.
  size_t end = currentIndex;
  while (end + 1 < wordCount && !endsSentence(end)) {
    ++end;
  }

  String text;
  text.reserve(64);
  for (size_t i = start; i <= end; ++i) {
    if (i != start) {
      text += ' ';
    }
    text += wordAt(i);
  }

  span.found = true;
  span.startIndex = start;
  span.endIndex = end;
  span.text = text;
  return span;
}

String encodeJsonLine(const Quote &quote) {
  String out;
  out.reserve(quote.sentence.length() + quote.bookPath.length() +
              quote.bookTitle.length() + 48);
  out += "{\"bookPath\":\"";
  appendJsonEscaped(out, quote.bookPath);
  out += "\",\"bookTitle\":\"";
  appendJsonEscaped(out, quote.bookTitle);
  out += "\",\"wordIndex\":";
  out += String(static_cast<unsigned int>(quote.wordIndex));
  out += ",\"sentence\":\"";
  appendJsonEscaped(out, quote.sentence);
  out += "\"}";
  return out;
}

bool parseJsonLine(const String &line, Quote &quote) {
  String trimmed = line;
  trimmed.trim();
  if (trimmed.isEmpty() || trimmed[0] != '{') {
    return false;
  }

  Quote parsed;

  const int pathStart = valueStart(trimmed, "bookPath");
  if (pathStart < 0 || !readJsonString(trimmed, pathStart, parsed.bookPath) ||
      parsed.bookPath.isEmpty()) {
    return false;
  }

  const int titleStart = valueStart(trimmed, "bookTitle");
  if (titleStart >= 0) {
    readJsonString(trimmed, titleStart, parsed.bookTitle);
  }

  const int indexStart = valueStart(trimmed, "wordIndex");
  if (indexStart >= 0) {
    uint32_t value = 0;
    int i = indexStart;
    while (i < static_cast<int>(trimmed.length()) && isDigit(trimmed[i])) {
      value = value * 10UL + static_cast<uint32_t>(trimmed[i] - '0');
      ++i;
    }
    parsed.wordIndex = value;
  }

  const int sentenceStart = valueStart(trimmed, "sentence");
  if (sentenceStart < 0 || !readJsonString(trimmed, sentenceStart, parsed.sentence) ||
      parsed.sentence.isEmpty()) {
    return false;
  }

  quote = parsed;
  return true;
}

bool containsQuote(const std::vector<Quote> &records, const String &bookPath,
                   uint32_t wordIndex) {
  for (const Quote &record : records) {
    if (record.wordIndex == wordIndex && record.bookPath == bookPath) {
      return true;
    }
  }
  return false;
}

String titleCase(const String &text) {
  String out;
  out.reserve(text.length());
  bool atWordStart = true;
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    const bool isLetter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    if (!isLetter) {
      out += c;
      atWordStart = (c == ' ' || c == '-' || c == '_' || c == '/');
      continue;
    }
    if (atWordStart) {
      if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
      }
    } else {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
    }
    out += c;
    atWordStart = false;
  }
  return out;
}

String quoteListLabel(const Quote &quote, size_t maxSentenceChars) {
  String title = quote.bookTitle;
  title.trim();
  if (title.isEmpty()) {
    // Fall back to the file name (last path segment), minus a .rsvp suffix.
    const int sep = quote.bookPath.lastIndexOf('/');
    title = sep >= 0 ? quote.bookPath.substring(sep + 1) : quote.bookPath;
    String lowered = title;
    lowered.toLowerCase();
    if (lowered.endsWith(".rsvp")) {
      title = title.substring(0, title.length() - 5);
    }
  }
  title = titleCase(title);

  String sentence = quote.sentence;
  sentence.trim();
  if (maxSentenceChars > 0 && sentence.length() > maxSentenceChars) {
    sentence = sentence.substring(0, maxSentenceChars);
    sentence.trim();
    sentence += "...";
  }

  if (title.isEmpty()) {
    return sentence;
  }
  if (sentence.isEmpty()) {
    return title;
  }
  return title + ": " + sentence;
}

}  // namespace quotes
