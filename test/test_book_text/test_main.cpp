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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ascii_passthrough);
  RUN_TEST(test_collapses_runs_of_whitespace_and_trims);
  RUN_TEST(test_utf8_umlaut_folds_to_device_byte);
  RUN_TEST(test_smart_punctuation_approximated_to_ascii);
  RUN_TEST(test_stats_count_non_ascii_and_malformed);
  return UNITY_END();
}
