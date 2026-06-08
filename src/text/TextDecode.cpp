#include "text/TextDecode.h"

#include "text/LatinText.h"

namespace textdecode {
namespace {

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

char entityCodepointByte(uint32_t codepoint) {
  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    return static_cast<char>(storedByte);
  }
  return ' ';
}

char entityPunctuationChar(uint32_t codepoint) {
  if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
    return static_cast<char>(codepoint - 0xFEE0);
  }

  switch (codepoint) {
    case 0x00A0:
      return ' ';
    case 0x00AB:
    case 0x00BB:
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x201F:
    case 0x2033:
    case 0x2036:
    case 0x300C:
    case 0x300D:
    case 0x300E:
    case 0x300F:
      return '"';
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x201B:
    case 0x2032:
    case 0x2035:
    case 0x2039:
    case 0x203A:
      return '\'';
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
    case 0x2043:
    case 0x2212:
      return '-';
    case 0x2022:
    case 0x00B7:
    case 0x2219:
      return '*';
    case 0x2026:
      return '.';
    case 0x207D:
    case 0x208D:
    case 0x2768:
    case 0x276A:
      return '(';
    case 0x207E:
    case 0x208E:
    case 0x2769:
    case 0x276B:
      return ')';
    case 0x2045:
    case 0x2308:
    case 0x230A:
    case 0x3010:
    case 0x3014:
    case 0x3016:
    case 0x3018:
    case 0x301A:
      return '[';
    case 0x2046:
    case 0x2309:
    case 0x230B:
    case 0x3011:
    case 0x3015:
    case 0x3017:
    case 0x3019:
    case 0x301B:
      return ']';
    case 0x2774:
    case 0x2776:
      return '{';
    case 0x2775:
    case 0x2777:
      return '}';
    case 0x2329:
    case 0x27E8:
    case 0x3008:
    case 0x300A:
      return '<';
    case 0x232A:
    case 0x27E9:
    case 0x3009:
    case 0x300B:
      return '>';
    case 0xFF0C:
      return ',';
    case 0xFF0E:
      return '.';
    case 0xFF1A:
      return ':';
    case 0xFF1B:
      return ';';
    case 0xFF01:
      return '!';
    case 0xFF1F:
      return '?';
    default:
      return '\0';
  }
}

bool parseNumericEntityCodepoint(const String &entity, uint32_t &value) {
  if (!entity.startsWith("#")) {
    return false;
  }

  value = 0;
  int start = 1;
  int base = 10;
  if (entity.length() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
    start = 2;
    base = 16;
  }
  if (static_cast<size_t>(start) >= entity.length()) {
    return false;
  }

  for (size_t i = start; i < entity.length(); ++i) {
    const int digit = base == 16 ? hexValue(entity[i]) : (entity[i] >= '0' && entity[i] <= '9'
                                                             ? entity[i] - '0'
                                                             : -1);
    if (digit < 0 || digit >= base) {
      return false;
    }
    value = value * base + static_cast<uint32_t>(digit);
  }

  return true;
}

bool isSentenceDashCodepoint(uint32_t codepoint) {
  return codepoint == 0x2012 || codepoint == 0x2013 || codepoint == 0x2014 ||
         codepoint == 0x2015;
}

bool isUtf8Continuation(uint8_t value) { return (value & 0xC0) == 0x80; }

void appendDisplayApproximation(String &target, uint32_t codepoint) {
  if (codepoint >= 32 && codepoint <= 126) {
    target += static_cast<char>(codepoint);
    return;
  }

  if (codepoint == 0x200B || codepoint == 0xFEFF) {
    return;
  }

  if (codepoint == 0x00A0) {
    target += ' ';
    return;
  }

  if (codepoint >= 0x2000 && codepoint <= 0x200A) {
    target += ' ';
    return;
  }

  uint8_t storedByte = 0;
  if (LatinText::storageByteForCodepoint(codepoint, storedByte)) {
    target += static_cast<char>(storedByte);
    return;
  }

  if (codepoint >= 0x3000 && codepoint <= 0x303F) {
    return;
  }

  switch (codepoint) {
    case 0x2010:
    case 0x2011:
      target += '-';
      return;
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
      target += " - ";
      return;
    case 0x2018:
    case 0x2019:
    case 0x201B:
      target += '\'';
      return;
    case 0x201C:
    case 0x201D:
    case 0x201F:
      target += '"';
      return;
    case 0x2026:
      target += "...";
      return;
    case 0x2032:
      target += '\'';
      return;
    case 0x2033:
      target += '"';
      return;
    case 0x2039:
      target += '<';
      return;
    case 0x203A:
      target += '>';
      return;
    default:
      return;
  }
}

}  // namespace

char decodedEntityChar(const String &entity) {
  if (entity == "amp") {
    return '&';
  }
  if (entity == "lt") {
    return '<';
  }
  if (entity == "gt") {
    return '>';
  }
  if (entity == "quot") {
    return '"';
  }
  if (entity == "apos") {
    return '\'';
  }
  if (entity == "nbsp") {
    return ' ';
  }
  if (entity == "iexcl") {
    return static_cast<char>(0x16);
  }
  if (entity == "iquest") {
    return static_cast<char>(0x17);
  }
  if (entity == "ldquo" || entity == "rdquo" || entity == "bdquo") {
    return '"';
  }
  if (entity == "lsquo" || entity == "rsquo" || entity == "sbquo") {
    return '\'';
  }
  if (entity == "laquo" || entity == "raquo") {
    return '"';
  }
  if (entity == "lsaquo" || entity == "rsaquo") {
    return '\'';
  }
  if (entity == "lpar") {
    return '(';
  }
  if (entity == "rpar") {
    return ')';
  }
  if (entity == "lbrack") {
    return '[';
  }
  if (entity == "rbrack") {
    return ']';
  }
  if (entity == "lcub") {
    return '{';
  }
  if (entity == "rcub") {
    return '}';
  }
  if (entity == "ndash" || entity == "mdash") {
    return '-';
  }
  if (entity == "hyphen" || entity == "minus") {
    return '-';
  }
  if (entity == "hellip") {
    return '.';
  }
  if (entity == "middot" || entity == "bull") {
    return '*';
  }
  struct NamedLatin1Entity {
    const char *name;
    uint8_t value;
  };
  static constexpr NamedLatin1Entity kLatin1Entities[] = {
      {"Agrave", 0xC0}, {"Aacute", 0xC1}, {"Acirc", 0xC2},  {"Atilde", 0xC3},
      {"Auml", 0xC4},   {"Aring", 0xC5},  {"AElig", 0xC6},  {"Ccedil", 0xC7},
      {"Egrave", 0xC8}, {"Eacute", 0xC9}, {"Ecirc", 0xCA},  {"Euml", 0xCB},
      {"Igrave", 0xCC}, {"Iacute", 0xCD}, {"Icirc", 0xCE},  {"Iuml", 0xCF},
      {"ETH", 0xD0},    {"Ntilde", 0xD1}, {"Ograve", 0xD2}, {"Oacute", 0xD3},
      {"Ocirc", 0xD4},  {"Otilde", 0xD5}, {"Ouml", 0xD6},   {"Oslash", 0xD8},
      {"Ugrave", 0xD9}, {"Uacute", 0xDA}, {"Ucirc", 0xDB},  {"Uuml", 0xDC},
      {"Yacute", 0xDD}, {"THORN", 0xDE},  {"szlig", 0xDF},  {"agrave", 0xE0},
      {"aacute", 0xE1}, {"acirc", 0xE2},  {"atilde", 0xE3}, {"auml", 0xE4},
      {"aring", 0xE5},  {"aelig", 0xE6},  {"ccedil", 0xE7}, {"egrave", 0xE8},
      {"eacute", 0xE9}, {"ecirc", 0xEA},  {"euml", 0xEB},   {"igrave", 0xEC},
      {"iacute", 0xED}, {"icirc", 0xEE},  {"iuml", 0xEF},   {"eth", 0xF0},
      {"ntilde", 0xF1}, {"ograve", 0xF2}, {"oacute", 0xF3}, {"ocirc", 0xF4},
      {"otilde", 0xF5}, {"ouml", 0xF6},   {"oslash", 0xF8}, {"ugrave", 0xF9},
      {"uacute", 0xFA}, {"ucirc", 0xFB},  {"uuml", 0xFC},   {"yacute", 0xFD},
      {"thorn", 0xFE},  {"yuml", 0xFF},
  };
  for (const NamedLatin1Entity &entry : kLatin1Entities) {
    if (entity == entry.name) {
      return static_cast<char>(entry.value);
    }
  }
  if (entity == "times") {
    return 'x';
  }
  if (entity == "divide") {
    return '/';
  }
  if (entity == "AElig") {
    return static_cast<char>(0xC6);
  }
  if (entity == "aelig") {
    return static_cast<char>(0xE6);
  }
  if (entity == "Aring") {
    return static_cast<char>(0xC5);
  }
  if (entity == "aring") {
    return static_cast<char>(0xE5);
  }
  if (entity == "Auml") {
    return static_cast<char>(0xC4);
  }
  if (entity == "auml") {
    return static_cast<char>(0xE4);
  }
  if (entity == "Ccedil") {
    return static_cast<char>(0xC7);
  }
  if (entity == "ccedil") {
    return static_cast<char>(0xE7);
  }
  if (entity == "ETH") {
    return static_cast<char>(0xD0);
  }
  if (entity == "eth") {
    return static_cast<char>(0xF0);
  }
  if (entity == "Ntilde") {
    return static_cast<char>(0xD1);
  }
  if (entity == "ntilde") {
    return static_cast<char>(0xF1);
  }
  if (entity == "Oslash") {
    return static_cast<char>(0xD8);
  }
  if (entity == "oslash") {
    return static_cast<char>(0xF8);
  }
  if (entity == "Ouml") {
    return static_cast<char>(0xD6);
  }
  if (entity == "ouml") {
    return static_cast<char>(0xF6);
  }
  if (entity == "THORN") {
    return static_cast<char>(0xDE);
  }
  if (entity == "thorn") {
    return static_cast<char>(0xFE);
  }
  if (entity == "Uuml") {
    return static_cast<char>(0xDC);
  }
  if (entity == "uuml") {
    return static_cast<char>(0xFC);
  }
  if (entity == "szlig") {
    return static_cast<char>(0xDF);
  }
  if (entity == "Dcaron") {
    return static_cast<char>(0x01);
  }
  if (entity == "dcaron") {
    return static_cast<char>(0x02);
  }
  if (entity == "Ecaron") {
    return static_cast<char>(0x03);
  }
  if (entity == "ecaron") {
    return static_cast<char>(0x04);
  }
  if (entity == "Ncaron") {
    return static_cast<char>(0x05);
  }
  if (entity == "ncaron") {
    return static_cast<char>(0x06);
  }
  if (entity == "Rcaron") {
    return static_cast<char>(0x07);
  }
  if (entity == "rcaron") {
    return static_cast<char>(0x08);
  }
  if (entity == "Tcaron") {
    return static_cast<char>(0x0E);
  }
  if (entity == "tcaron") {
    return static_cast<char>(0x0F);
  }
  if (entity == "Uring") {
    return static_cast<char>(0x10);
  }
  if (entity == "uring") {
    return static_cast<char>(0x11);
  }
  if (entity == "Odblac") {
    return static_cast<char>(0x12);
  }
  if (entity == "odblac") {
    return static_cast<char>(0x13);
  }
  if (entity == "Udblac") {
    return static_cast<char>(0x14);
  }
  if (entity == "udblac") {
    return static_cast<char>(0x15);
  }
  if (entity == "OElig") {
    return static_cast<char>(0x80);
  }
  if (entity == "oelig") {
    return static_cast<char>(0x81);
  }
  if (entity == "Scaron") {
    return static_cast<char>(0x86);
  }
  if (entity == "scaron") {
    return static_cast<char>(0x87);
  }
  if (entity == "Zcaron") {
    return static_cast<char>(0x88);
  }
  if (entity == "zcaron") {
    return static_cast<char>(0x89);
  }
  if (entity == "Amacr") {
    return static_cast<char>(0xA1);
  }
  if (entity == "amacr") {
    return static_cast<char>(0xA2);
  }
  if (entity == "Emacr") {
    return static_cast<char>(0xA3);
  }
  if (entity == "emacr") {
    return static_cast<char>(0xA4);
  }
  if (entity == "Gcedil" || entity == "Gcommaaccent") {
    return static_cast<char>(0xA5);
  }
  if (entity == "gcedil" || entity == "gcommaaccent") {
    return static_cast<char>(0xA6);
  }
  if (entity == "Imacr") {
    return static_cast<char>(0xA7);
  }
  if (entity == "imacr") {
    return static_cast<char>(0xA8);
  }
  if (entity == "Kcedil" || entity == "Kcommaaccent") {
    return static_cast<char>(0xA9);
  }
  if (entity == "kcedil" || entity == "kcommaaccent") {
    return static_cast<char>(0xAA);
  }
  if (entity == "Lcedil" || entity == "Lcommaaccent") {
    return static_cast<char>(0xAB);
  }
  if (entity == "lcedil" || entity == "lcommaaccent") {
    return static_cast<char>(0xAC);
  }
  if (entity == "Ncedil" || entity == "Ncommaaccent") {
    return static_cast<char>(0xAE);
  }
  if (entity == "ncedil" || entity == "ncommaaccent") {
    return static_cast<char>(0xAF);
  }
  if (entity == "Edot") {
    return static_cast<char>(0xB0);
  }
  if (entity == "edot") {
    return static_cast<char>(0xB1);
  }
  if (entity == "Iogon") {
    return static_cast<char>(0xB6);
  }
  if (entity == "iogon") {
    return static_cast<char>(0xB7);
  }
  if (entity == "Uogon") {
    return static_cast<char>(0xB8);
  }
  if (entity == "uogon") {
    return static_cast<char>(0xB9);
  }
  if (entity == "Umacr") {
    return static_cast<char>(0xBA);
  }
  if (entity == "umacr") {
    return static_cast<char>(0xBB);
  }
  if (entity == "Dstrok") {
    return static_cast<char>(0xBC);
  }
  if (entity == "dstrok") {
    return static_cast<char>(0xBD);
  }
  if (entity == "ENG") {
    return static_cast<char>(0xBE);
  }
  if (entity == "eng") {
    return static_cast<char>(0xBF);
  }
  if (entity == "Tstrok") {
    return static_cast<char>(0xD7);
  }
  if (entity == "tstrok") {
    return static_cast<char>(0xF7);
  }

  uint32_t value = 0;
  if (parseNumericEntityCodepoint(entity, value)) {
    const char mapped = entityCodepointByte(value);
    if (mapped != ' ') {
      return mapped;
    }
    const char punctuation = entityPunctuationChar(value);
    if (punctuation != '\0') {
      return punctuation;
    }
  }

  return ' ';
}

String decodedEntityText(const String &entity) {
  if (entity == "ndash" || entity == "mdash") {
    return " - ";
  }
  if (entity == "hellip") {
    return "...";
  }

  uint32_t value = 0;
  if (parseNumericEntityCodepoint(entity, value)) {
    if (isSentenceDashCodepoint(value)) {
      return " - ";
    }
    if (value == 0x2026) {
      return "...";
    }
  }

  String decoded;
  decoded += decodedEntityChar(entity);
  return decoded;
}

bool decodeUtf8Codepoint(const String &text, size_t &index, uint32_t &codepoint) {
  const uint8_t first = static_cast<uint8_t>(text[index++]);
  if (first < 0x80) {
    codepoint = first;
    return true;
  }

  uint8_t continuationCount = 0;
  uint32_t minimumValue = 0;
  if ((first & 0xE0) == 0xC0) {
    codepoint = first & 0x1F;
    continuationCount = 1;
    minimumValue = 0x80;
  } else if ((first & 0xF0) == 0xE0) {
    codepoint = first & 0x0F;
    continuationCount = 2;
    minimumValue = 0x800;
  } else if ((first & 0xF8) == 0xF0) {
    codepoint = first & 0x07;
    continuationCount = 3;
    minimumValue = 0x10000;
  } else {
    return false;
  }

  if (index + continuationCount > text.length()) {
    return false;
  }

  for (uint8_t i = 0; i < continuationCount; ++i) {
    const uint8_t next = static_cast<uint8_t>(text[index]);
    if (!isUtf8Continuation(next)) {
      return false;
    }
    ++index;
    codepoint = (codepoint << 6) | (next & 0x3F);
  }

  if (codepoint < minimumValue || codepoint > 0x10FFFF ||
      (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
    return false;
  }

  return true;
}

String normalizeDisplayText(const String &text) {
  String normalized;
  normalized.reserve(text.length());

  size_t index = 0;
  while (index < text.length()) {
    const size_t before = index;
    uint32_t codepoint = 0;
    if (decodeUtf8Codepoint(text, index, codepoint)) {
      appendDisplayApproximation(normalized, codepoint);
      continue;
    }

    index = before + 1;
    normalized += static_cast<char>(text[before]);
  }

  String collapsed;
  collapsed.reserve(normalized.length());
  bool previousSpace = true;
  for (size_t i = 0; i < normalized.length(); ++i) {
    const uint8_t value = LatinText::byteValue(normalized[i]);
    if (value <= ' ' && !LatinText::isWordCharacter(value) &&
        !LatinText::isLowCustomSlotByte(value)) {
      if (!previousSpace) {
        collapsed += ' ';
        previousSpace = true;
      }
      continue;
    }

    collapsed += static_cast<char>(value);
    previousSpace = false;
  }

  if (!collapsed.isEmpty() && collapsed[collapsed.length() - 1] == ' ') {
    collapsed.remove(collapsed.length() - 1, 1);
  }
  return collapsed;
}

}  // namespace textdecode
