#include <unity.h>

#include <cstring>
#include <vector>

#include "text/Dictionary.h"

namespace {

std::vector<dict::Entry> gEntries;

void setEntries(const std::vector<const char *> &words) {
  gEntries.clear();
  uint32_t offset = 0;
  for (const char *word : words) {
    dict::Entry entry{};
    std::strncpy(entry.word, word, dict::kWordBytes - 1);
    entry.offset = offset;
    entry.length = 10;
    offset += 10;
    gEntries.push_back(entry);
  }
}

bool readAt(size_t index, dict::Entry &out) {
  if (index >= gEntries.size()) {
    return false;
  }
  out = gEntries[index];
  return true;
}

bool find(const char *word, dict::Entry &out) {
  return dict::findEntry(gEntries.size(), readAt, String(word), out);
}

}  // namespace

void test_normalize_lowercases_and_trims_punctuation(void) {
  TEST_ASSERT_EQUAL_STRING("reading", dict::normalizeLookupWord("\"Reading,\"").c_str());
  TEST_ASSERT_EQUAL_STRING("well-known", dict::normalizeLookupWord("(well-known)").c_str());
  TEST_ASSERT_EQUAL_STRING("don't", dict::normalizeLookupWord("Don't!").c_str());
  TEST_ASSERT_EQUAL_STRING("", dict::normalizeLookupWord("...").c_str());
}

void test_exact_match_and_edges(void) {
  setEntries({"apple", "banana", "cherry", "date", "elderberry"});
  dict::Entry entry;
  TEST_ASSERT_TRUE(find("apple", entry));      // first
  TEST_ASSERT_EQUAL_UINT32(0, entry.offset);
  TEST_ASSERT_TRUE(find("elderberry", entry));  // last
  TEST_ASSERT_EQUAL_UINT32(40, entry.offset);
  TEST_ASSERT_TRUE(find("cherry", entry));      // middle
  TEST_ASSERT_FALSE(find("fig", entry));
  TEST_ASSERT_FALSE(find("aardvark", entry));
}

void test_empty_dictionary_and_oversized_word(void) {
  setEntries({});
  dict::Entry entry;
  TEST_ASSERT_FALSE(find("anything", entry));
  setEntries({"word"});
  TEST_ASSERT_FALSE(find("a-word-far-longer-than-twenty-four-bytes", entry));
}

void test_stem_fallbacks(void) {
  setEntries({"carry", "read", "run", "smile", "word"});
  dict::Entry entry;
  TEST_ASSERT_TRUE(find("words", entry));     // plural s
  TEST_ASSERT_TRUE(find("reads", entry));
  TEST_ASSERT_TRUE(find("smiles", entry));
  TEST_ASSERT_TRUE(find("smiled", entry));    // ed -> smile (keep trailing e)
  TEST_ASSERT_TRUE(find("reading", entry));   // ing
  TEST_ASSERT_TRUE(find("smiling", entry));   // ing -> +e
  TEST_ASSERT_TRUE(find("carries", entry));   // ies -> y
  TEST_ASSERT_TRUE(find("word's", entry));    // possessive
  TEST_ASSERT_FALSE(find("running", entry));  // doubled consonant: out of scope
}

void test_word_field_comparison_is_bounded(void) {
  // A word exactly kWordBytes-1 long exercises the zero-padding bound.
  setEntries({"abcdefghijklmnopqrstuvw"});  // 23 chars
  dict::Entry entry;
  TEST_ASSERT_TRUE(find("abcdefghijklmnopqrstuvw", entry));
  TEST_ASSERT_FALSE(find("abcdefghijklmnopqrstuv", entry));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_normalize_lowercases_and_trims_punctuation);
  RUN_TEST(test_exact_match_and_edges);
  RUN_TEST(test_empty_dictionary_and_oversized_word);
  RUN_TEST(test_stem_fallbacks);
  RUN_TEST(test_word_field_comparison_is_bounded);
  return UNITY_END();
}
