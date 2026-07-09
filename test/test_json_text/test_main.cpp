#include <unity.h>

#include "text/JsonText.h"

void test_escape_specials_and_control_chars(void) {
  TEST_ASSERT_EQUAL_STRING("say \\\"hi\\\"", jsontext::escape("say \"hi\"").c_str());
  TEST_ASSERT_EQUAL_STRING("a\\\\b", jsontext::escape("a\\b").c_str());
  TEST_ASSERT_EQUAL_STRING("line\\nbreak\\ttab", jsontext::escape("line\nbreak\ttab").c_str());
  TEST_ASSERT_EQUAL_STRING("bell\\u0007!", jsontext::escape("bell\x07!").c_str());
  TEST_ASSERT_EQUAL_STRING("\\b\\f\\r", jsontext::escape("\b\f\r").c_str());
}

void test_unescape_inverts_escape(void) {
  const String original = "say \"hi\"\\ line\nbreak\ttab \b\f\r";
  TEST_ASSERT_EQUAL_STRING(original.c_str(),
                           jsontext::unescape(jsontext::escape(original)).c_str());
  TEST_ASSERT_EQUAL_STRING("a/b", jsontext::unescape("a\\/b").c_str());
  // Unknown escape keeps the escaped char.
  TEST_ASSERT_EQUAL_STRING("axb", jsontext::unescape("a\\xb").c_str());
}

void test_read_int(void) {
  int value = 0;
  TEST_ASSERT_TRUE(jsontext::readInt("{\"wpm\": 420}", "wpm", value));
  TEST_ASSERT_EQUAL_INT(420, value);
  TEST_ASSERT_TRUE(jsontext::readInt("{\"tzOffsetMinutes\":-90}", "tzOffsetMinutes", value));
  TEST_ASSERT_EQUAL_INT(-90, value);
  TEST_ASSERT_FALSE(jsontext::readInt("{\"wpm\": \"fast\"}", "wpm", value));
  TEST_ASSERT_FALSE(jsontext::readInt("{\"other\": 1}", "wpm", value));
}

void test_read_int64_handles_epoch_millis(void) {
  int64_t value = 0;
  TEST_ASSERT_TRUE(jsontext::readInt64("{\"epochMs\": 1767225600123}", "epochMs", value));
  TEST_ASSERT_EQUAL(1767225600123LL, value);
  TEST_ASSERT_TRUE(jsontext::readInt64("{\"epochMs\":-5}", "epochMs", value));
  TEST_ASSERT_EQUAL(-5LL, value);
}

void test_read_bool(void) {
  bool value = false;
  TEST_ASSERT_TRUE(jsontext::readBool("{\"darkMode\": true}", "darkMode", value));
  TEST_ASSERT_TRUE(value);
  TEST_ASSERT_TRUE(jsontext::readBool("{\"darkMode\":false}", "darkMode", value));
  TEST_ASSERT_FALSE(value);
  TEST_ASSERT_FALSE(jsontext::readBool("{\"darkMode\": 1}", "darkMode", value));
}

void test_read_string_with_escapes(void) {
  String value;
  TEST_ASSERT_TRUE(jsontext::readString("{\"title\": \"A \\\"B\\\"\\nC\"}", "title", value));
  TEST_ASSERT_EQUAL_STRING("A \"B\"\nC", value.c_str());
  TEST_ASSERT_FALSE(jsontext::readString("{\"title\": 3}", "title", value));
  TEST_ASSERT_FALSE(jsontext::readString("{\"title\": \"unterminated", "title", value));
}

void test_read_string_from_scans_forward(void) {
  const String json =
      "{\"assets\":[{\"name\":\"a.bin\",\"url\":\"u1\"},{\"name\":\"b.bin\",\"url\":\"u2\"}]}";
  String value;
  int keyIndex = -1;
  TEST_ASSERT_TRUE(jsontext::readStringFrom(json, "name", 0, value, &keyIndex));
  TEST_ASSERT_EQUAL_STRING("a.bin", value.c_str());
  TEST_ASSERT_TRUE(keyIndex > 0);

  TEST_ASSERT_TRUE(jsontext::readStringFrom(json, "name", keyIndex + 1, value));
  TEST_ASSERT_EQUAL_STRING("b.bin", value.c_str());
}

void test_parse_string_at_reports_closing_quote(void) {
  const String json = "[\"a\\\"b\", \"c\"]";
  String value;
  int closing = -1;
  TEST_ASSERT_TRUE(jsontext::parseStringAt(json, 1, value, &closing));
  TEST_ASSERT_EQUAL_STRING("a\"b", value.c_str());
  TEST_ASSERT_EQUAL_CHAR('"', json[closing]);
  TEST_ASSERT_FALSE(jsontext::parseStringAt(json, 0, value, &closing));
}

void test_next_array_string_iterates(void) {
  const String body = "{\"feeds\": [\"https://a\", \"https://b\\\"c\" , \"https://d\"]}";
  const int colon = jsontext::findKeyColon(body, "feeds");
  TEST_ASSERT_TRUE(colon > 0);
  int index = jsontext::skipWhitespace(body, colon + 1);
  TEST_ASSERT_EQUAL_CHAR('[', body[index]);
  ++index;

  String value;
  TEST_ASSERT_TRUE(jsontext::nextArrayString(body, index, value));
  TEST_ASSERT_EQUAL_STRING("https://a", value.c_str());
  TEST_ASSERT_TRUE(jsontext::nextArrayString(body, index, value));
  TEST_ASSERT_EQUAL_STRING("https://b\"c", value.c_str());
  TEST_ASSERT_TRUE(jsontext::nextArrayString(body, index, value));
  TEST_ASSERT_EQUAL_STRING("https://d", value.c_str());
  TEST_ASSERT_FALSE(jsontext::nextArrayString(body, index, value));
}

void test_next_array_string_rejects_non_strings(void) {
  const String body = "[1, 2]";
  int index = 1;
  String value;
  TEST_ASSERT_FALSE(jsontext::nextArrayString(body, index, value));
}

void test_find_key_colon_missing_key(void) {
  TEST_ASSERT_EQUAL_INT(-1, jsontext::findKeyColon("{\"a\": 1}", "b"));
  TEST_ASSERT_EQUAL_INT(-1, jsontext::findKeyColon("{\"a\"}", "a"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_escape_specials_and_control_chars);
  RUN_TEST(test_unescape_inverts_escape);
  RUN_TEST(test_read_int);
  RUN_TEST(test_read_int64_handles_epoch_millis);
  RUN_TEST(test_read_bool);
  RUN_TEST(test_read_string_with_escapes);
  RUN_TEST(test_read_string_from_scans_forward);
  RUN_TEST(test_parse_string_at_reports_closing_quote);
  RUN_TEST(test_next_array_string_iterates);
  RUN_TEST(test_next_array_string_rejects_non_strings);
  RUN_TEST(test_find_key_colon_missing_key);
  return UNITY_END();
}
