#include "reader/WordPacing.h"

#include <algorithm>

#include "text/LatinText.h"

namespace wordpacing {
namespace {

constexpr uint8_t kLongWordAfterChars = 6;
constexpr uint8_t kLongWordPercentPerChar = 6;
constexpr uint8_t kVeryLongWordAfterChars = 10;
constexpr uint8_t kVeryLongWordPercentPerChar = 9;
constexpr uint8_t kUltraLongWordAfterChars = 14;
constexpr uint8_t kUltraLongWordPercentPerChar = 12;
constexpr uint8_t kLongWordMaxPercent = 170;
constexpr uint8_t kCompoundJoinerPercent = 14;
constexpr uint8_t kLongCompoundWordPercent = 18;
constexpr uint8_t kTechnicalConnectorPercent = 8;
constexpr uint8_t kSyllableBonusAfterCount = 2;
constexpr uint8_t kSyllableBonusPercentPerGroup = 10;
constexpr uint8_t kSyllableBonusMaxPercent = 50;
constexpr uint8_t kAllCapsComplexityPercent = 14;
constexpr uint8_t kMixedTokenComplexityPercent = 22;
constexpr uint8_t kNumericTokenComplexityPercent = 10;
constexpr uint8_t kDenseConnectorComplexityPercent = 12;
constexpr uint8_t kComplexWordMaxPercent = 85;
constexpr uint8_t kCommaPausePercent = 45;
constexpr uint8_t kDashPausePercent = 60;
constexpr uint8_t kClausePausePercent = 80;
constexpr uint8_t kEllipsisPausePercent = 110;
constexpr uint8_t kSentencePausePercent = 135;
constexpr uint8_t kStrongSentencePausePercent = 150;
constexpr uint16_t kMaxPacingDelayMs = 600;

bool isWordCharacter(char c) {
  return LatinText::isWordCharacter(static_cast<uint8_t>(c));
}

bool isLetterCharacter(char c) {
  return LatinText::isLetter(static_cast<uint8_t>(c));
}

bool isDigitCharacter(char c) {
  return LatinText::isDigit(static_cast<uint8_t>(c));
}

bool isLowercaseLetter(char c) {
  return LatinText::isLowercaseLetter(static_cast<uint8_t>(c));
}

bool isUppercaseLetter(char c) {
  return LatinText::isUppercaseLetter(static_cast<uint8_t>(c));
}

bool isSegmentSeparator(char c) {
  switch (c) {
    case '-':
    case '/':
    case '_':
      return true;
    default:
      return false;
  }
}

bool isTechnicalConnector(char c) {
  switch (c) {
    case '-':
    case '/':
    case '_':
    case '.':
    case '+':
    case '\\':
      return true;
    default:
      return false;
  }
}

bool isIgnoredTrailingChar(char c) {
  switch (c) {
    case '"':
    case '\'':
    case ')':
    case ']':
    case '}':
      return true;
    default:
      return false;
  }
}

int letterCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isLetterCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int digitCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isDigitCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int uppercaseLetterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isUppercaseLetter(word[i])) {
      ++count;
    }
  }
  return count;
}

int readableCharacterCount(const String &word) {
  int count = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordCharacter(word[i])) {
      ++count;
    }
  }
  return count;
}

int approximateSyllableGroupCount(const String &word) {
  int groups = 0;
  int letterCount = 0;
  bool previousWasVowel = false;
  String lettersOnly;
  lettersOnly.reserve(word.length());

  for (size_t i = 0; i < word.length(); ++i) {
    const char c = word[i];
    if (!isLetterCharacter(c)) {
      previousWasVowel = false;
      continue;
    }

    ++letterCount;
    const char lowered = static_cast<char>(LatinText::toLowercaseByte(static_cast<uint8_t>(c)));
    lettersOnly += lowered;

    const bool vowel = LatinText::isVowel(static_cast<uint8_t>(lowered));
    if (vowel && !previousWasVowel) {
      ++groups;
    }
    previousWasVowel = vowel;
  }

  if (groups > 1 && letterCount > 3 && lettersOnly.endsWith("e") && !lettersOnly.endsWith("le") &&
      !lettersOnly.endsWith("ye")) {
    --groups;
  }

  if (groups == 0 && letterCount > 0) {
    groups = 1;
  }

  return groups;
}

int compoundJoinerCount(const String &word) {
  int count = 0;
  for (size_t i = 1; i + 1 < word.length(); ++i) {
    if (!isSegmentSeparator(word[i])) {
      continue;
    }
    if (!isWordCharacter(word[i - 1]) || !isWordCharacter(word[i + 1])) {
      continue;
    }
    ++count;
  }
  return count;
}

int technicalConnectorCount(const String &word) {
  int count = 0;
  for (size_t i = 1; i + 1 < word.length(); ++i) {
    if (!isTechnicalConnector(word[i])) {
      continue;
    }
    if (!isWordCharacter(word[i - 1]) || !isWordCharacter(word[i + 1])) {
      continue;
    }
    ++count;
  }
  return count;
}

int lastMeaningfulCharIndex(const String &word) {
  for (int i = static_cast<int>(word.length()) - 1; i >= 0; --i) {
    if (!isIgnoredTrailingChar(word[static_cast<size_t>(i)])) {
      return i;
    }
  }
  return -1;
}

char trailingRhythmChar(const String &word) {
  const int index = lastMeaningfulCharIndex(word);
  if (index >= 0) {
    return word[static_cast<size_t>(index)];
  }
  return '\0';
}

int trailingRepeatedCharCount(const String &word, char target) {
  int count = 0;
  for (int i = lastMeaningfulCharIndex(word); i >= 0; --i) {
    const char c = word[static_cast<size_t>(i)];
    if (c != target) {
      break;
    }
    ++count;
  }
  return count;
}

bool endsWithEllipsis(const String &word) {
  return trailingRepeatedCharCount(word, '.') >= 3;
}

bool isDottedInitialism(const String &word) {
  const int end = lastMeaningfulCharIndex(word);
  if (end <= 0) {
    return false;
  }

  int letterCount = 0;
  bool expectLetter = true;
  for (int i = 0; i <= end; ++i) {
    const char c = word[static_cast<size_t>(i)];
    if (expectLetter) {
      if (!isLetterCharacter(c)) {
        return false;
      }
      ++letterCount;
      expectLetter = false;
    } else if (c == '.') {
      expectLetter = true;
    } else {
      return false;
    }
  }

  return expectLetter && letterCount >= 2;
}

bool looksLikeAbbreviation(const String &word, bool nextWordStartsLowercase) {
  String lowered = word;
  lowered.toLowerCase();

  constexpr const char *kKnownAbbreviations[] = {
      "mr.",  "mrs.",  "ms.",   "dr.",   "prof.", "sr.",   "jr.",  "st.",
      "vs.",  "etc.",  "e.g.",  "i.e.",  "cf.",   "no.",   "fig.", "eq.",
      "inc.", "ltd.",  "co.",   "dept.", "mt.",   "ft.",
  };

  for (const char *abbreviation : kKnownAbbreviations) {
    if (lowered == abbreviation) {
      return true;
    }
  }

  if (!lowered.endsWith(".")) {
    return false;
  }

  if (isDottedInitialism(word)) {
    return true;
  }

  if (readableCharacterCount(lowered) <= 2) {
    return true;
  }

  if (nextWordStartsLowercase && readableCharacterCount(lowered) <= 4) {
    return true;
  }

  return false;
}

uint16_t clampPacingDelayMs(uint16_t delayMs) {
  if (delayMs > kMaxPacingDelayMs) {
    return kMaxPacingDelayMs;
  }
  return delayMs;
}

uint8_t clampScalePercent(uint8_t percent) {
  if (percent < 25) {
    return 25;
  }
  return percent;
}

uint16_t scaledPercent(uint16_t basePercent, uint8_t scalePercent) {
  return static_cast<uint16_t>((static_cast<uint32_t>(basePercent) *
                                static_cast<uint32_t>(clampScalePercent(scalePercent))) /
                               100UL);
}

uint32_t scaledDelayMs(uint16_t bonusPercent, uint16_t delayMs) {
  return (static_cast<uint32_t>(bonusPercent) *
          static_cast<uint32_t>(clampPacingDelayMs(delayMs))) /
         100UL;
}

uint16_t lengthBonusPercentForWord(const String &word) {
  const int readableLength = readableCharacterCount(word);
  if (readableLength == 0) {
    return 0;
  }

  uint16_t bonusPercent = 0;
  if (readableLength > kLongWordAfterChars) {
    const int extraChars = readableLength - kLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kLongWordPercentPerChar));
  }

  if (readableLength > kVeryLongWordAfterChars) {
    const int extraChars = readableLength - kVeryLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kVeryLongWordPercentPerChar));
  }

  if (readableLength > kUltraLongWordAfterChars) {
    const int extraChars = readableLength - kUltraLongWordAfterChars;
    bonusPercent +=
        static_cast<uint16_t>(extraChars * static_cast<int>(kUltraLongWordPercentPerChar));
  }

  const int joinerCount = compoundJoinerCount(word);
  if (joinerCount > 0) {
    bonusPercent +=
        static_cast<uint16_t>(joinerCount * static_cast<int>(kCompoundJoinerPercent));
    if (readableLength >= kVeryLongWordAfterChars) {
      bonusPercent += kLongCompoundWordPercent;
    }
  }

  const int techConnectorCount = technicalConnectorCount(word);
  if (techConnectorCount > joinerCount) {
    bonusPercent += static_cast<uint16_t>((techConnectorCount - joinerCount) *
                                          static_cast<int>(kTechnicalConnectorPercent));
  }

  return std::min<uint16_t>(kLongWordMaxPercent, bonusPercent);
}

uint16_t complexityBonusPercentForWord(const String &word) {
  uint16_t bonusPercent = 0;
  const int syllableGroups = approximateSyllableGroupCount(word);
  if (syllableGroups > kSyllableBonusAfterCount) {
    const int extraGroups = syllableGroups - kSyllableBonusAfterCount;
    bonusPercent += static_cast<uint16_t>(std::min(
        static_cast<int>(kSyllableBonusMaxPercent),
        extraGroups * static_cast<int>(kSyllableBonusPercentPerGroup)));
  }

  const int letterCount = letterCharacterCount(word);
  const int digitCount = digitCharacterCount(word);
  const int uppercaseCount = uppercaseLetterCount(word);
  if (letterCount > 0 && digitCount > 0) {
    bonusPercent += kMixedTokenComplexityPercent;
  } else if (digitCount >= 3) {
    bonusPercent += kNumericTokenComplexityPercent;
  }

  if (uppercaseCount >= 2 && uppercaseCount == letterCount) {
    bonusPercent += kAllCapsComplexityPercent;
  }

  const int techConnectorCount = technicalConnectorCount(word);
  if (techConnectorCount >= 2) {
    bonusPercent += static_cast<uint16_t>((techConnectorCount - 1) *
                                          static_cast<int>(kDenseConnectorComplexityPercent));
  }

  return std::min<uint16_t>(kComplexWordMaxPercent, bonusPercent);
}

// A punctuation pause resolves to a percentage bonus and which base delay it
// draws from: sentence-end punctuation ('.' '!' '?', ellipsis) uses the
// punctuation delay; clause punctuation (',' ';' ':' '-') uses the clause
// delay. A zero percent means no pause (and the kind is irrelevant).
enum class PunctuationKind { None, Clause, SentenceEnd };

struct PunctuationPause {
  uint16_t percent;
  PunctuationKind kind;
};

PunctuationPause punctuationPauseForWord(const String &word, bool nextWordStartsLowercase) {
  if (endsWithEllipsis(word)) {
    return {kEllipsisPausePercent, PunctuationKind::SentenceEnd};
  }

  switch (trailingRhythmChar(word)) {
    case ',':
      return {kCommaPausePercent, PunctuationKind::Clause};
    case '-':
      return {kDashPausePercent, PunctuationKind::Clause};
    case ';':
    case ':':
      return {kClausePausePercent, PunctuationKind::Clause};
    case '.':
      if (!looksLikeAbbreviation(word, nextWordStartsLowercase)) {
        return {kSentencePausePercent, PunctuationKind::SentenceEnd};
      }
      return {0, PunctuationKind::None};
    case '!':
    case '?':
      return {kStrongSentencePausePercent, PunctuationKind::SentenceEnd};
    default:
      return {0, PunctuationKind::None};
  }
}

}  // namespace

PacingConfig clampConfig(const PacingConfig &config) {
  PacingConfig out;
  out.longWordDelayMs = clampPacingDelayMs(config.longWordDelayMs);
  out.complexWordDelayMs = clampPacingDelayMs(config.complexWordDelayMs);
  out.punctuationDelayMs = clampPacingDelayMs(config.punctuationDelayMs);
  out.clausePauseDelayMs = clampPacingDelayMs(config.clausePauseDelayMs);
  out.longWordScalePercent = clampScalePercent(config.longWordScalePercent);
  out.complexWordScalePercent = clampScalePercent(config.complexWordScalePercent);
  out.punctuationScalePercent = clampScalePercent(config.punctuationScalePercent);
  return out;
}

bool startsWithLowercaseLetter(const String &word) {
  for (size_t i = 0; i < word.length(); ++i) {
    if (isLowercaseLetter(word[i])) {
      return true;
    }
    if (isLetterCharacter(word[i])) {
      return false;
    }
  }
  return false;
}

bool wordEndsSentence(const String &word, bool nextWordStartsLowercase) {
  switch (trailingRhythmChar(word)) {
    case '!':
    case '?':
      return true;
    case '.':
      return !looksLikeAbbreviation(word, nextWordStartsLowercase);
    default:
      return false;
  }
}

uint32_t bonusMsForWord(const String &word, bool nextWordStartsLowercase,
                        const PacingConfig &config) {
  if (word.isEmpty()) {
    return 0;
  }

  uint32_t totalBonusMs = 0;
  totalBonusMs += scaledDelayMs(
      scaledPercent(lengthBonusPercentForWord(word), config.longWordScalePercent),
      config.longWordDelayMs);
  totalBonusMs += scaledDelayMs(
      scaledPercent(complexityBonusPercentForWord(word), config.complexWordScalePercent),
      config.complexWordDelayMs);
  const PunctuationPause pause = punctuationPauseForWord(word, nextWordStartsLowercase);
  if (pause.percent > 0) {
    const uint16_t baseDelayMs = pause.kind == PunctuationKind::Clause
                                     ? config.clausePauseDelayMs
                                     : config.punctuationDelayMs;
    totalBonusMs +=
        scaledDelayMs(scaledPercent(pause.percent, config.punctuationScalePercent), baseDelayMs);
  }
  return totalBonusMs;
}

uint32_t durationForWord(const String &word, bool nextWordStartsLowercase, uint32_t baseIntervalMs,
                         const PacingConfig &config) {
  if (baseIntervalMs == 0) {
    return 0;
  }
  return baseIntervalMs + bonusMsForWord(word, nextWordStartsLowercase, config);
}

}  // namespace wordpacing
