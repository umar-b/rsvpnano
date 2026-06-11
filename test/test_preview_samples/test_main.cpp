#include <unity.h>

#include <vector>

#include "text/PreviewSamples.h"

// Typography-preview sentence extraction: rejoin reader words into bounded
// sentences using the reader's sentence-boundary convention. Pure module.

namespace {

using previewsamples::Config;

std::vector<String> words(std::initializer_list<const char *> list) {
  std::vector<String> out;
  for (const char *w : list) {
    out.push_back(String(w));
  }
  return out;
}

}  // namespace

void test_empty_input_yields_nothing() {
  const auto out = previewsamples::extractSentences({}, Config{});
  TEST_ASSERT_TRUE(out.empty());
}

void test_splits_on_terminal_punctuation() {
  const auto in = words({"The", "cat", "sat.", "It", "purred", "loudly."});
  const auto out = previewsamples::extractSentences(in, Config{});
  TEST_ASSERT_EQUAL_UINT(2, out.size());
  TEST_ASSERT_EQUAL_STRING("The cat sat.", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("It purred loudly.", out[1].c_str());
}

void test_question_and_exclamation_close_sentences() {
  const auto in = words({"Who", "goes", "there?", "Halt", "now!"});
  const auto out = previewsamples::extractSentences(in, Config{});
  TEST_ASSERT_EQUAL_UINT(2, out.size());
  TEST_ASSERT_EQUAL_STRING("Who goes there?", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Halt now!", out[1].c_str());
}

void test_abbreviation_dot_does_not_split() {
  // "Dr." followed by a capitalised word is still treated as not a sentence end
  // by the reader's convention (known abbreviation), so the sentence runs on.
  const auto in = words({"Dr.", "Smith", "arrived", "early."});
  const auto out = previewsamples::extractSentences(in, Config{});
  TEST_ASSERT_EQUAL_UINT(1, out.size());
  TEST_ASSERT_EQUAL_STRING("Dr. Smith arrived early.", out[0].c_str());
}

void test_caps_at_max_sentences() {
  const auto in = words({"One.", "Two.", "Three.", "Four.", "Five.", "Six.", "Seven."});
  Config config;
  config.maxSentences = 3;
  config.minWordsPerSentence = 1;  // single-word sentences allowed for this test
  const auto out = previewsamples::extractSentences(in, config);
  TEST_ASSERT_EQUAL_UINT(3, out.size());
}

void test_drops_over_long_sentences() {
  std::vector<String> in;
  for (int i = 0; i < 20; ++i) {
    in.push_back(String("word"));
  }
  in.push_back(String("end."));
  in.push_back(String("Short"));
  in.push_back(String("one."));
  Config config;
  config.maxWordsPerSentence = 5;
  const auto out = previewsamples::extractSentences(in, config);
  // The 21-word sentence is dropped; the short trailing one survives.
  TEST_ASSERT_EQUAL_UINT(1, out.size());
  TEST_ASSERT_EQUAL_STRING("Short one.", out[0].c_str());
}

void test_drops_too_short_fragments() {
  // "Stop!" ends a sentence on its own ('!' always terminates); as a single
  // word it falls below minWordsPerSentence and is dropped, while the following
  // four-word sentence survives.
  const auto in = words({"Stop!", "A", "real", "sentence", "here."});
  Config config;
  config.minWordsPerSentence = 2;
  const auto out = previewsamples::extractSentences(in, config);
  TEST_ASSERT_EQUAL_UINT(1, out.size());
  TEST_ASSERT_EQUAL_STRING("A real sentence here.", out[0].c_str());
}

void test_trailing_unterminated_words_form_a_sentence() {
  // A window can end mid-sentence (no terminal punctuation); the trailing run
  // is still offered as a sample if it is within bounds.
  const auto in = words({"This", "has", "no", "full", "stop"});
  const auto out = previewsamples::extractSentences(in, Config{});
  TEST_ASSERT_EQUAL_UINT(1, out.size());
  TEST_ASSERT_EQUAL_STRING("This has no full stop", out[0].c_str());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_input_yields_nothing);
  RUN_TEST(test_splits_on_terminal_punctuation);
  RUN_TEST(test_question_and_exclamation_close_sentences);
  RUN_TEST(test_abbreviation_dot_does_not_split);
  RUN_TEST(test_caps_at_max_sentences);
  RUN_TEST(test_drops_over_long_sentences);
  RUN_TEST(test_drops_too_short_fragments);
  RUN_TEST(test_trailing_unterminated_words_form_a_sentence);
  return UNITY_END();
}
