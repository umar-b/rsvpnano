#include <unity.h>

#include "text/TextDecode.h"

namespace {

// Builds a String from raw bytes so tests can express UTF-8 sequences directly.
String bytes(const uint8_t *data, size_t len) {
  String s;
  for (size_t i = 0; i < len; ++i) {
    s += static_cast<char>(data[i]);
  }
  return s;
}

}  // namespace

void test_named_xml_entities() {
  TEST_ASSERT_EQUAL_STRING("&", textdecode::decodedEntityText("amp").c_str());
  TEST_ASSERT_EQUAL_STRING("<", textdecode::decodedEntityText("lt").c_str());
  TEST_ASSERT_EQUAL_STRING(">", textdecode::decodedEntityText("gt").c_str());
  TEST_ASSERT_EQUAL_STRING("\"", textdecode::decodedEntityText("quot").c_str());
  TEST_ASSERT_EQUAL_STRING("'", textdecode::decodedEntityText("apos").c_str());
  TEST_ASSERT_EQUAL_STRING(" ", textdecode::decodedEntityText("nbsp").c_str());
}

void test_numeric_entities_decimal_and_hex() {
  TEST_ASSERT_EQUAL_STRING("A", textdecode::decodedEntityText("#65").c_str());
  TEST_ASSERT_EQUAL_STRING("A", textdecode::decodedEntityText("#x41").c_str());
  TEST_ASSERT_EQUAL_STRING("A", textdecode::decodedEntityText("#X41").c_str());
}

void test_em_dash_entity_expands_to_spaced_hyphen() {
  // U+2014 EM DASH -> " - " so sentence dashes keep their breathing room.
  TEST_ASSERT_EQUAL_STRING(" - ", textdecode::decodedEntityText("#8212").c_str());
  TEST_ASSERT_EQUAL_STRING(" - ", textdecode::decodedEntityText("#x2014").c_str());
}

void test_unmapped_entity_falls_back_to_space() {
  TEST_ASSERT_EQUAL_STRING(" ", textdecode::decodedEntityText("totallyunknown").c_str());
}

void test_decode_utf8_two_byte_sequence() {
  const uint8_t e_acute[] = {0xC3, 0xA9};  // U+00E9 é
  const String s = bytes(e_acute, sizeof(e_acute));
  size_t index = 0;
  uint32_t cp = 0;
  TEST_ASSERT_TRUE(textdecode::decodeUtf8Codepoint(s, index, cp));
  TEST_ASSERT_EQUAL_UINT32(0x00E9, cp);
  TEST_ASSERT_EQUAL_UINT32(2, index);
}

void test_decode_utf8_rejects_lone_continuation_byte() {
  const uint8_t bad[] = {0x80};  // continuation byte with no leader
  const String s = bytes(bad, sizeof(bad));
  size_t index = 0;
  uint32_t cp = 0;
  TEST_ASSERT_FALSE(textdecode::decodeUtf8Codepoint(s, index, cp));
}

void test_normalize_collapses_and_trims_whitespace() {
  TEST_ASSERT_EQUAL_STRING("hello world",
                           textdecode::normalizeDisplayText("  hello   world  ").c_str());
}

void test_normalize_approximates_utf8_punctuation() {
  // U+2014 em dash -> " - "
  const uint8_t emdash[] = {'a', 0xE2, 0x80, 0x94, 'b'};
  TEST_ASSERT_EQUAL_STRING("a - b",
                           textdecode::normalizeDisplayText(bytes(emdash, sizeof(emdash))).c_str());

  // U+2026 horizontal ellipsis -> "..."
  const uint8_t ellipsis[] = {'w', 'a', 'i', 't', 0xE2, 0x80, 0xA6};
  TEST_ASSERT_EQUAL_STRING(
      "wait...", textdecode::normalizeDisplayText(bytes(ellipsis, sizeof(ellipsis))).c_str());

  // U+2019 right single quote -> '
  const uint8_t quote[] = {'d', 'o', 'n', 0xE2, 0x80, 0x99, 't'};
  TEST_ASSERT_EQUAL_STRING(
      "don't", textdecode::normalizeDisplayText(bytes(quote, sizeof(quote))).c_str());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_named_xml_entities);
  RUN_TEST(test_numeric_entities_decimal_and_hex);
  RUN_TEST(test_em_dash_entity_expands_to_spaced_hyphen);
  RUN_TEST(test_unmapped_entity_falls_back_to_space);
  RUN_TEST(test_decode_utf8_two_byte_sequence);
  RUN_TEST(test_decode_utf8_rejects_lone_continuation_byte);
  RUN_TEST(test_normalize_collapses_and_trims_whitespace);
  RUN_TEST(test_normalize_approximates_utf8_punctuation);
  return UNITY_END();
}
