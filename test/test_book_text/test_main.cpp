#include <unity.h>

#include "text/BookText.h"
#include "text/LatinText.h"

void test_ascii_passthrough() {
  TEST_ASSERT_EQUAL_STRING("Hello world",
                           booktext::normalizeDisplayText("Hello world").c_str());
}

void test_collapses_runs_of_whitespace_and_trims() {
  // Tabs/newlines/multiple spaces collapse to single spaces; trailing trimmed.
  TEST_ASSERT_EQUAL_STRING("a b c",
                           booktext::normalizeDisplayText("  a\t\tb \n c  ").c_str());
}

void test_utf8_umlaut_folds_to_device_byte() {
  // "Cafe" + U+00E9 (é, UTF-8 C3 A9). The renderer keeps accented Latin as the
  // device's custom storage byte rather than stripping the accent.
  const char eacute[] = {static_cast<char>(0xC3), static_cast<char>(0xA9), 0};
  const String out = booktext::normalizeDisplayText(String("Caf") + eacute);
  uint8_t expectedByte = 0;
  TEST_ASSERT_TRUE(LatinText::storageByteForCodepoint(0x00E9, expectedByte));
  String expected = "Caf";
  expected += static_cast<char>(expectedByte);
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), out.c_str());
}

void test_smart_punctuation_approximated_to_ascii() {
  // U+2019 right single quote (UTF-8 E2 80 99) -> ASCII apostrophe.
  const char rsquo[] = {static_cast<char>(0xE2), static_cast<char>(0x80),
                        static_cast<char>(0x99), 0};
  const String out = booktext::normalizeDisplayText(String("it") + rsquo + "s");
  TEST_ASSERT_EQUAL_STRING("it's", out.c_str());
  // U+2014 em dash (E2 80 94) -> spaced hyphen.
  const char mdash[] = {static_cast<char>(0xE2), static_cast<char>(0x80),
                        static_cast<char>(0x94), 0};
  const String dash = booktext::normalizeDisplayText(String("a") + mdash + "b");
  TEST_ASSERT_EQUAL_STRING("a - b", dash.c_str());
}

void test_stats_count_non_ascii_and_malformed() {
  booktext::ParseStats stats;
  const char eacute[] = {static_cast<char>(0xC3), static_cast<char>(0xA9), 0};
  booktext::normalizeDisplayText(String("a") + eacute, &stats);
  TEST_ASSERT_EQUAL_UINT32(1, stats.nonAsciiCodepoints);

  booktext::ParseStats bad;
  // Lone continuation byte 0x80 is malformed UTF-8.
  const char loneCont[] = {static_cast<char>(0x80), 0};
  booktext::normalizeDisplayText(String("a") + loneCont, &bad);
  TEST_ASSERT_GREATER_THAN_UINT32(0, bad.malformedUtf8);
}

void test_chapter_title_detection() {
  String title;
  TEST_ASSERT_TRUE(booktext::chapterTitleFromLine("# The Beginning", title));
  TEST_ASSERT_EQUAL_STRING("The Beginning", title.c_str());
  TEST_ASSERT_TRUE(booktext::chapterTitleFromLine("Chapter 3", title));
  TEST_ASSERT_EQUAL_STRING("Chapter 3", title.c_str());
  TEST_ASSERT_TRUE(booktext::chapterTitleFromLine("Part II", title));
  TEST_ASSERT_FALSE(booktext::chapterTitleFromLine("just an ordinary sentence here", title));
}

void test_tokenizer_handles_hyphens_and_ellipsis() {
  std::vector<String> tokens;
  booktext::appendTokenizedLineWords(
      "well-being and ... a stand-alone -- dash",
      [&](const String &t) { tokens.push_back(t); return true; },
      [&]() { return tokens.size(); }, nullptr);
  // Inline hyphen kept; standalone "--" becomes a "-" token; ellipsis attaches.
  TEST_ASSERT_EQUAL_STRING("well-being", tokens[0].c_str());
  bool sawStandaloneDash = false;
  for (const String &t : tokens) {
    if (t == "-") sawStandaloneDash = true;
  }
  TEST_ASSERT_TRUE(sawStandaloneDash);
}

void test_rsvp_directives_words_chapters_paragraphs() {
  BookContent book;
  bool paragraphPending = true;
  const booktext::MemoryLowFn noLimit = nullptr;
  const char *lines[] = {"@title My Book", "@author Jane Doe", "@chapter One",
                         "Hello world",    "",                 "Next line"};
  for (const char *l : lines) {
    booktext::processRsvpLine(l, book, paragraphPending, nullptr, noLimit);
  }
  TEST_ASSERT_EQUAL_STRING("My Book", book.title.c_str());
  TEST_ASSERT_EQUAL_STRING("Jane Doe", book.author.c_str());
  TEST_ASSERT_EQUAL_UINT32(4, book.words.size());
  TEST_ASSERT_EQUAL_STRING("Hello", book.words[0].c_str());
  TEST_ASSERT_EQUAL_STRING("line", book.words[3].c_str());
  TEST_ASSERT_EQUAL_UINT32(1, book.chapters.size());
  TEST_ASSERT_EQUAL_STRING("One", book.chapters[0].title.c_str());
  TEST_ASSERT_EQUAL_UINT32(0, book.chapters[0].wordIndex);
  // Paragraph starts at word 0 (first line) and word 2 (after the blank line).
  TEST_ASSERT_EQUAL_UINT32(2, book.paragraphStarts.size());
  TEST_ASSERT_EQUAL_UINT32(0, book.paragraphStarts[0]);
  TEST_ASSERT_EQUAL_UINT32(2, book.paragraphStarts[1]);
}

void test_plain_book_detects_chapter_and_paragraphs() {
  BookContent book;
  bool paragraphPending = true;
  const booktext::MemoryLowFn noLimit = nullptr;
  const char *lines[] = {"Chapter 1", "alpha beta", "", "gamma"};
  for (const char *l : lines) {
    booktext::processBookLine(l, book, paragraphPending, nullptr, noLimit);
  }
  TEST_ASSERT_EQUAL_UINT32(1, book.chapters.size());
  TEST_ASSERT_EQUAL_STRING("Chapter 1", book.chapters[0].title.c_str());
  // In plain text the heading line is also read, so its words count too:
  // "Chapter" "1" "alpha" "beta" "gamma".
  TEST_ASSERT_EQUAL_UINT32(5, book.words.size());
  TEST_ASSERT_EQUAL_STRING("Chapter", book.words[0].c_str());
  TEST_ASSERT_EQUAL_UINT32(2, book.paragraphStarts.size());
  TEST_ASSERT_EQUAL_UINT32(0, book.paragraphStarts[0]);
  TEST_ASSERT_EQUAL_UINT32(4, book.paragraphStarts[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ascii_passthrough);
  RUN_TEST(test_collapses_runs_of_whitespace_and_trims);
  RUN_TEST(test_utf8_umlaut_folds_to_device_byte);
  RUN_TEST(test_smart_punctuation_approximated_to_ascii);
  RUN_TEST(test_stats_count_non_ascii_and_malformed);
  RUN_TEST(test_chapter_title_detection);
  RUN_TEST(test_tokenizer_handles_hyphens_and_ellipsis);
  RUN_TEST(test_rsvp_directives_words_chapters_paragraphs);
  RUN_TEST(test_plain_book_detects_chapter_and_paragraphs);
  return UNITY_END();
}
