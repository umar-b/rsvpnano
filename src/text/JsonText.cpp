#include "text/JsonText.h"

#include <cctype>
#include <cstdio>

namespace jsontext {

namespace {

bool isAsciiWhitespace(char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
      return true;
    default:
      return false;
  }
}

}  // namespace

String escape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (c < 0x20) {
          char code[7];
          std::snprintf(code, sizeof(code), "\\u%04x", c);
          escaped += code;
        } else {
          escaped += static_cast<char>(c);
        }
        break;
    }
  }
  return escaped;
}

String unescape(const String &raw) {
  String output;
  output.reserve(raw.length());

  bool escaping = false;
  for (size_t i = 0; i < raw.length(); ++i) {
    const char c = raw[i];
    if (escaping) {
      switch (c) {
        case '"':
        case '\\':
        case '/':
          output += c;
          break;
        case 'b':
          output += '\b';
          break;
        case 'f':
          output += '\f';
          break;
        case 'n':
          output += '\n';
          break;
        case 'r':
          output += '\r';
          break;
        case 't':
          output += '\t';
          break;
        default:
          output += c;
          break;
      }
      escaping = false;
      continue;
    }

    if (c == '\\') {
      escaping = true;
      continue;
    }

    output += c;
  }

  return output;
}

int skipWhitespace(const String &body, int index) {
  while (index < static_cast<int>(body.length()) && isAsciiWhitespace(body[index])) {
    ++index;
  }
  return index;
}

int findKeyColon(const String &body, const char *key, size_t from, int *keyIndex) {
  const String needle = String("\"") + key + "\"";
  const int at = body.indexOf(needle, static_cast<unsigned int>(from));
  if (at < 0) {
    return -1;
  }
  const int colonIndex = body.indexOf(':', at + needle.length());
  if (colonIndex < 0) {
    return -1;
  }
  if (keyIndex != nullptr) {
    *keyIndex = at;
  }
  return colonIndex;
}

bool readInt(const String &body, const char *key, int &value) {
  const int colonIndex = findKeyColon(body, key);
  if (colonIndex < 0) {
    return false;
  }
  int index = skipWhitespace(body, colonIndex + 1);
  bool negative = false;
  if (index < static_cast<int>(body.length()) && body[index] == '-') {
    negative = true;
    ++index;
  }
  if (index >= static_cast<int>(body.length()) ||
      !isdigit(static_cast<unsigned char>(body[index]))) {
    return false;
  }
  int result = 0;
  while (index < static_cast<int>(body.length()) &&
         isdigit(static_cast<unsigned char>(body[index]))) {
    result = result * 10 + (body[index] - '0');
    ++index;
  }
  value = negative ? -result : result;
  return true;
}

bool readInt64(const String &body, const char *key, int64_t &value) {
  const int colonIndex = findKeyColon(body, key);
  if (colonIndex < 0) {
    return false;
  }
  int index = skipWhitespace(body, colonIndex + 1);
  bool negative = false;
  if (index < static_cast<int>(body.length()) && body[index] == '-') {
    negative = true;
    ++index;
  }
  if (index >= static_cast<int>(body.length()) ||
      !isdigit(static_cast<unsigned char>(body[index]))) {
    return false;
  }
  int64_t result = 0;
  while (index < static_cast<int>(body.length()) &&
         isdigit(static_cast<unsigned char>(body[index]))) {
    result = result * 10 + static_cast<int64_t>(body[index] - '0');
    ++index;
  }
  value = negative ? -result : result;
  return true;
}

bool readBool(const String &body, const char *key, bool &value) {
  const int colonIndex = findKeyColon(body, key);
  if (colonIndex < 0) {
    return false;
  }
  const int index = skipWhitespace(body, colonIndex + 1);
  if (body.substring(index, index + 4) == "true") {
    value = true;
    return true;
  }
  if (body.substring(index, index + 5) == "false") {
    value = false;
    return true;
  }
  return false;
}

bool readString(const String &body, const char *key, String &value) {
  return readStringFrom(body, key, 0, value);
}

bool readStringFrom(const String &body, const char *key, size_t from, String &value,
                    int *keyIndex) {
  int at = -1;
  const int colonIndex = findKeyColon(body, key, from, &at);
  if (colonIndex < 0) {
    return false;
  }
  const int quoteIndex = skipWhitespace(body, colonIndex + 1);
  if (quoteIndex >= static_cast<int>(body.length()) || body[quoteIndex] != '"') {
    return false;
  }
  if (keyIndex != nullptr) {
    *keyIndex = at;
  }
  return parseStringAt(body, quoteIndex, value);
}

bool parseStringAt(const String &body, int quoteIndex, String &value, int *closingQuote) {
  if (quoteIndex < 0 || static_cast<size_t>(quoteIndex) >= body.length() ||
      body[quoteIndex] != '"') {
    return false;
  }

  String raw;
  raw.reserve(64);
  bool escaping = false;
  for (size_t i = static_cast<size_t>(quoteIndex) + 1; i < body.length(); ++i) {
    const char c = body[i];
    if (!escaping && c == '"') {
      value = unescape(raw);
      if (closingQuote != nullptr) {
        *closingQuote = static_cast<int>(i);
      }
      return true;
    }

    raw += c;
    if (escaping) {
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    }
  }

  return false;
}

bool nextArrayString(const String &body, int &index, String &value) {
  index = skipWhitespace(body, index);
  if (index >= static_cast<int>(body.length())) {
    return false;
  }
  if (body[index] == ',') {
    index = skipWhitespace(body, index + 1);
  }
  if (index >= static_cast<int>(body.length()) || body[index] == ']') {
    return false;
  }
  if (body[index] != '"') {
    return false;
  }
  int closingQuote = -1;
  if (!parseStringAt(body, index, value, &closingQuote)) {
    return false;
  }
  index = closingQuote + 1;
  return true;
}

}  // namespace jsontext
