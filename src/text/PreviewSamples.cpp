#include "text/PreviewSamples.h"

#include "reader/WordPacing.h"

namespace previewsamples {

namespace {

// True when the word at nextIndex starts with a lowercase letter, matching the
// reader's abbreviation test ("Dr." followed by a lowercase word is not a
// sentence end). Out-of-range (no following word) reads as uppercase, so a
// trailing "." closes the sentence.
bool nextStartsLowercase(const std::vector<String> &words, size_t nextIndex) {
  if (nextIndex >= words.size()) {
    return false;
  }
  return wordpacing::startsWithLowercaseLetter(words[nextIndex]);
}

String joinRange(const std::vector<String> &words, size_t begin, size_t end) {
  String out;
  for (size_t i = begin; i < end; ++i) {
    if (!out.isEmpty()) {
      out += ' ';
    }
    out += words[i];
  }
  out.trim();
  return out;
}

}  // namespace

std::vector<String> extractSentences(const std::vector<String> &words, const Config &config) {
  std::vector<String> sentences;
  if (words.empty() || config.maxSentences == 0) {
    return sentences;
  }

  size_t sentenceStart = 0;
  for (size_t i = 0; i < words.size(); ++i) {
    const bool endsSentence =
        wordpacing::wordEndsSentence(words[i], nextStartsLowercase(words, i + 1));
    const bool lastWord = (i + 1 == words.size());
    if (!endsSentence && !lastWord) {
      continue;
    }

    const size_t wordCount = i + 1 - sentenceStart;
    if (wordCount >= config.minWordsPerSentence && wordCount <= config.maxWordsPerSentence) {
      const String sentence = joinRange(words, sentenceStart, i + 1);
      if (!sentence.isEmpty()) {
        sentences.push_back(sentence);
        if (sentences.size() >= config.maxSentences) {
          break;
        }
      }
    }
    sentenceStart = i + 1;
  }

  return sentences;
}

}  // namespace previewsamples
