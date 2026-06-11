#include <unity.h>

#include "storage/BookProgress.h"

void test_hash_is_deterministic_and_path_sensitive() {
  TEST_ASSERT_EQUAL_UINT32(bookprogress::hashPath("/books/books/a.rsvp"),
                           bookprogress::hashPath("/books/books/a.rsvp"));
  TEST_ASSERT_NOT_EQUAL(bookprogress::hashPath("/books/books/a.rsvp"),
                        bookprogress::hashPath("/books/books/b.rsvp"));
}

void test_key_format_and_prefixes() {
  const String path = "/books/books/moby.rsvp";
  const String pos = bookprogress::positionKey(path);
  const String cnt = bookprogress::wordCountKey(path);
  const String rec = bookprogress::recentKey(path);
  const String fin = bookprogress::finishedKey(path);
  // "x" + 8 hex chars = 9, within the 15-char NVS limit.
  TEST_ASSERT_EQUAL_UINT32(9, pos.length());
  TEST_ASSERT_EQUAL_UINT32(9, fin.length());
  TEST_ASSERT_EQUAL('p', pos[0]);
  TEST_ASSERT_EQUAL('c', cnt[0]);
  TEST_ASSERT_EQUAL('r', rec[0]);
  TEST_ASSERT_EQUAL('f', fin[0]);
  // Same path -> same 8-hex suffix across the keys.
  TEST_ASSERT_EQUAL_STRING(pos.substring(1).c_str(), cnt.substring(1).c_str());
  TEST_ASSERT_EQUAL_STRING(pos.substring(1).c_str(), rec.substring(1).c_str());
  TEST_ASSERT_EQUAL_STRING(pos.substring(1).c_str(), fin.substring(1).c_str());
}

void test_finished_key_distinct_from_other_prefixes() {
  const String path = "/books/books/moby.rsvp";
  TEST_ASSERT_TRUE(bookprogress::finishedKey(path) != bookprogress::positionKey(path));
  TEST_ASSERT_TRUE(bookprogress::finishedKey(path) != bookprogress::recentKey(path));
  TEST_ASSERT_TRUE(bookprogress::finishedKey("/a.rsvp") != bookprogress::finishedKey("/b.rsvp"));
}

void test_distinct_paths_yield_distinct_keys() {
  TEST_ASSERT_TRUE(bookprogress::positionKey("/books/books/a.rsvp") !=
                   bookprogress::positionKey("/books/books/b.rsvp"));
}

void test_progress_percent_math() {
  uint8_t pct = 255;
  // Not enough data.
  TEST_ASSERT_FALSE(bookprogress::progressPercent(0, 0, pct));
  TEST_ASSERT_FALSE(bookprogress::progressPercent(0, 1, pct));
  // Start of a 101-word book -> 0%.
  TEST_ASSERT_TRUE(bookprogress::progressPercent(0, 101, pct));
  TEST_ASSERT_EQUAL_UINT8(0, pct);
  // Halfway.
  TEST_ASSERT_TRUE(bookprogress::progressPercent(50, 101, pct));
  TEST_ASSERT_EQUAL_UINT8(50, pct);
  // Last word -> 100%.
  TEST_ASSERT_TRUE(bookprogress::progressPercent(100, 101, pct));
  TEST_ASSERT_EQUAL_UINT8(100, pct);
}

void test_progress_percent_clamps_overshoot() {
  uint8_t pct = 0;
  // Saved index past the (possibly shrunk) word count clamps to 100, never over.
  TEST_ASSERT_TRUE(bookprogress::progressPercent(9999, 101, pct));
  TEST_ASSERT_EQUAL_UINT8(100, pct);
}

void test_wpm_key_format_and_distinct_prefix() {
  const String path = "/books/books/moby.rsvp";
  const String wpm = bookprogress::wpmKey(path);
  // "w" + 8 hex chars = 9, within the 15-char NVS limit.
  TEST_ASSERT_EQUAL_UINT32(9, wpm.length());
  TEST_ASSERT_EQUAL('w', wpm[0]);
  // Same 8-hex suffix as the other per-book keys, distinct prefix from each.
  TEST_ASSERT_EQUAL_STRING(bookprogress::positionKey(path).substring(1).c_str(),
                           wpm.substring(1).c_str());
  TEST_ASSERT_TRUE(bookprogress::wpmKey(path) != bookprogress::positionKey(path));
  TEST_ASSERT_TRUE(bookprogress::wpmKey(path) != bookprogress::wordCountKey(path));
  TEST_ASSERT_TRUE(bookprogress::wpmKey(path) != bookprogress::bookmarkKey(path));
  TEST_ASSERT_TRUE(bookprogress::wpmKey("/a.rsvp") != bookprogress::wpmKey("/b.rsvp"));
}

void test_resolve_book_wpm_prefers_book_override() {
  // A saved per-book WPM wins over the global fallback.
  TEST_ASSERT_EQUAL_UINT16(450, bookprogress::resolveBookWpm(450, 300));
  // No override (sentinel) falls back to the global pref.
  TEST_ASSERT_EQUAL_UINT16(300, bookprogress::resolveBookWpm(bookprogress::kNoSavedWpm, 300));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hash_is_deterministic_and_path_sensitive);
  RUN_TEST(test_key_format_and_prefixes);
  RUN_TEST(test_finished_key_distinct_from_other_prefixes);
  RUN_TEST(test_distinct_paths_yield_distinct_keys);
  RUN_TEST(test_progress_percent_math);
  RUN_TEST(test_progress_percent_clamps_overshoot);
  RUN_TEST(test_wpm_key_format_and_distinct_prefix);
  RUN_TEST(test_resolve_book_wpm_prefers_book_override);
  return UNITY_END();
}
