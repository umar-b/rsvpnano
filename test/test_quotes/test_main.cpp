#include <unity.h>

#include <vector>

#include "quotes/Quote.h"

namespace {

// A tiny fixed corpus with two sentences:
//   "Hello brave new world."  -> indices 0..3 (word 3 ends sentence)
//   "Read on, friend!"        -> indices 4..6 (word 6 ends sentence)
const std::vector<String> kWords = {"Hello", "brave",   "new",
                                    "world.", "Read",    "on,",
                                    "friend!"};

quotes::WordAtFn wordAt() {
  return [](size_t i) { return i < kWords.size() ? kWords[i] : String(""); };
}

// Sentence-ending words: those ending in . ! ? (matching the corpus design).
quotes::EndsSentenceFn endsSentence() {
  return [](size_t i) {
    if (i >= kWords.size()) {
      return false;
    }
    const String &w = kWords[i];
    if (w.isEmpty()) {
      return false;
    }
    const char last = w[w.length() - 1];
    return last == '.' || last == '!' || last == '?';
  };
}

}  // namespace

void test_extract_from_middle_of_first_sentence() {
  const quotes::SentenceSpan span =
      quotes::extractSentence(2, kWords.size(), wordAt(), endsSentence());
  TEST_ASSERT_TRUE(span.found);
  TEST_ASSERT_EQUAL_UINT32(0, span.startIndex);
  TEST_ASSERT_EQUAL_UINT32(3, span.endIndex);
  TEST_ASSERT_EQUAL_STRING("Hello brave new world.", span.text.c_str());
}

void test_extract_from_word_that_ends_sentence() {
  // Current word is the sentence-ending word itself.
  const quotes::SentenceSpan span =
      quotes::extractSentence(3, kWords.size(), wordAt(), endsSentence());
  TEST_ASSERT_EQUAL_UINT32(0, span.startIndex);
  TEST_ASSERT_EQUAL_UINT32(3, span.endIndex);
  TEST_ASSERT_EQUAL_STRING("Hello brave new world.", span.text.c_str());
}

void test_extract_from_start_of_second_sentence() {
  const quotes::SentenceSpan span =
      quotes::extractSentence(4, kWords.size(), wordAt(), endsSentence());
  TEST_ASSERT_EQUAL_UINT32(4, span.startIndex);
  TEST_ASSERT_EQUAL_UINT32(6, span.endIndex);
  TEST_ASSERT_EQUAL_STRING("Read on, friend!", span.text.c_str());
}

void test_extract_empty_book_is_not_found() {
  const quotes::SentenceSpan span =
      quotes::extractSentence(0, 0, wordAt(), endsSentence());
  TEST_ASSERT_FALSE(span.found);
}

void test_extract_clamps_out_of_range_index() {
  const quotes::SentenceSpan span =
      quotes::extractSentence(999, kWords.size(), wordAt(), endsSentence());
  TEST_ASSERT_TRUE(span.found);
  TEST_ASSERT_EQUAL_UINT32(4, span.startIndex);
  TEST_ASSERT_EQUAL_UINT32(6, span.endIndex);
}

void test_encode_parse_round_trip() {
  quotes::Quote quote;
  quote.bookPath = "/books/books/moby.rsvp";
  quote.bookTitle = "Moby Dick";
  quote.wordIndex = 42;
  quote.sentence = "Call me Ishmael.";

  const String line = quotes::encodeJsonLine(quote);
  quotes::Quote decoded;
  TEST_ASSERT_TRUE(quotes::parseJsonLine(line, decoded));
  TEST_ASSERT_EQUAL_STRING(quote.bookPath.c_str(), decoded.bookPath.c_str());
  TEST_ASSERT_EQUAL_STRING(quote.bookTitle.c_str(), decoded.bookTitle.c_str());
  TEST_ASSERT_EQUAL_UINT32(quote.wordIndex, decoded.wordIndex);
  TEST_ASSERT_EQUAL_STRING(quote.sentence.c_str(), decoded.sentence.c_str());
}

void test_encode_escapes_quotes_and_newlines() {
  quotes::Quote quote;
  quote.bookPath = "/books/books/a.rsvp";
  quote.bookTitle = "He said \"hi\"";
  quote.wordIndex = 1;
  quote.sentence = "Line one\nand a \"quote\".";

  const String line = quotes::encodeJsonLine(quote);
  // The raw newline must not appear literally (would break JSON-lines framing).
  TEST_ASSERT_TRUE(line.indexOf('\n') < 0);
  TEST_ASSERT_TRUE(line.indexOf("\\n") >= 0);
  TEST_ASSERT_TRUE(line.indexOf("\\\"") >= 0);

  quotes::Quote decoded;
  TEST_ASSERT_TRUE(quotes::parseJsonLine(line, decoded));
  TEST_ASSERT_EQUAL_STRING(quote.bookTitle.c_str(), decoded.bookTitle.c_str());
  TEST_ASSERT_EQUAL_STRING(quote.sentence.c_str(), decoded.sentence.c_str());
}

void test_parse_rejects_blank_and_malformed() {
  quotes::Quote quote;
  TEST_ASSERT_FALSE(quotes::parseJsonLine("", quote));
  TEST_ASSERT_FALSE(quotes::parseJsonLine("   ", quote));
  TEST_ASSERT_FALSE(quotes::parseJsonLine("not json", quote));
  // Missing sentence -> rejected.
  TEST_ASSERT_FALSE(
      quotes::parseJsonLine("{\"bookPath\":\"/a.rsvp\",\"wordIndex\":1}", quote));
  // Missing bookPath -> rejected.
  TEST_ASSERT_FALSE(
      quotes::parseJsonLine("{\"sentence\":\"hi.\",\"wordIndex\":1}", quote));
}

void test_parse_defaults_optional_fields() {
  quotes::Quote quote;
  TEST_ASSERT_TRUE(quotes::parseJsonLine(
      "{\"bookPath\":\"/a.rsvp\",\"sentence\":\"Hi there.\"}", quote));
  TEST_ASSERT_EQUAL_UINT32(0, quote.wordIndex);
  TEST_ASSERT_TRUE(quote.bookTitle.isEmpty());
}

void test_contains_dedupe_key_is_path_plus_index() {
  std::vector<quotes::Quote> records;
  quotes::Quote q;
  q.bookPath = "/a.rsvp";
  q.wordIndex = 10;
  q.sentence = "One.";
  records.push_back(q);

  TEST_ASSERT_TRUE(quotes::containsQuote(records, "/a.rsvp", 10));
  TEST_ASSERT_FALSE(quotes::containsQuote(records, "/a.rsvp", 11));
  TEST_ASSERT_FALSE(quotes::containsQuote(records, "/b.rsvp", 10));
}

void test_title_case() {
  TEST_ASSERT_EQUAL_STRING("Moby Dick", quotes::titleCase("moby dick").c_str());
  TEST_ASSERT_EQUAL_STRING("Moby Dick", quotes::titleCase("MOBY DICK").c_str());
  TEST_ASSERT_EQUAL_STRING("War And Peace", quotes::titleCase("war and peace").c_str());
}

void test_quote_list_label_clips_and_titlecases() {
  quotes::Quote q;
  q.bookPath = "/books/books/moby.rsvp";
  q.bookTitle = "moby dick";
  q.wordIndex = 0;
  q.sentence = "Call me Ishmael and read this very very long sentence here.";

  const String label = quotes::quoteListLabel(q, 20);
  TEST_ASSERT_TRUE(label.startsWith("Moby Dick: "));
  TEST_ASSERT_TRUE(label.endsWith("..."));
}

void test_quote_list_label_falls_back_to_filename() {
  quotes::Quote q;
  q.bookPath = "/books/books/some-title.rsvp";
  q.bookTitle = "";
  q.wordIndex = 0;
  q.sentence = "Short.";

  const String label = quotes::quoteListLabel(q, 48);
  TEST_ASSERT_TRUE(label.startsWith("Some-Title"));
  TEST_ASSERT_TRUE(label.indexOf(".rsvp") < 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_extract_from_middle_of_first_sentence);
  RUN_TEST(test_extract_from_word_that_ends_sentence);
  RUN_TEST(test_extract_from_start_of_second_sentence);
  RUN_TEST(test_extract_empty_book_is_not_found);
  RUN_TEST(test_extract_clamps_out_of_range_index);
  RUN_TEST(test_encode_parse_round_trip);
  RUN_TEST(test_encode_escapes_quotes_and_newlines);
  RUN_TEST(test_parse_rejects_blank_and_malformed);
  RUN_TEST(test_parse_defaults_optional_fields);
  RUN_TEST(test_contains_dedupe_key_is_path_plus_index);
  RUN_TEST(test_title_case);
  RUN_TEST(test_quote_list_label_clips_and_titlecases);
  RUN_TEST(test_quote_list_label_falls_back_to_filename);
  return UNITY_END();
}
