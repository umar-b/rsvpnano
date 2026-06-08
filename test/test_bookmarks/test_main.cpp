#include <unity.h>

#include <algorithm>
#include <vector>

#include "storage/BookProgress.h"

void test_bookmark_key_format() {
  const String path = "/books/books/moby.rsvp";
  const String k = bookprogress::bookmarkKey(path);
  TEST_ASSERT_EQUAL_UINT32(9, k.length());
  TEST_ASSERT_EQUAL('k', k[0]);
  // Same path -> same 8-hex suffix as the position key.
  TEST_ASSERT_EQUAL_STRING(bookprogress::positionKey(path).substring(1).c_str(),
                           k.substring(1).c_str());
}

void test_encode_decode_round_trip() {
  std::vector<uint32_t> marks = {0, 1, 42, 1000000, 0xFFFFFFFEUL};
  const std::vector<uint8_t> bytes = bookprogress::encodeBookmarks(marks);
  TEST_ASSERT_EQUAL_UINT32(marks.size() * 4, bytes.size());
  const std::vector<uint32_t> decoded = bookprogress::decodeBookmarks(bytes.data(), bytes.size());
  TEST_ASSERT_EQUAL_UINT32(marks.size(), decoded.size());
  for (size_t i = 0; i < marks.size(); ++i) {
    TEST_ASSERT_EQUAL_UINT32(marks[i], decoded[i]);
  }
}

void test_decode_empty_and_null() {
  TEST_ASSERT_EQUAL_UINT32(0, bookprogress::decodeBookmarks(nullptr, 0).size());
  const uint8_t bytes[] = {1, 2, 3, 4};
  TEST_ASSERT_EQUAL_UINT32(0, bookprogress::decodeBookmarks(nullptr, 4).size());
  // A trailing partial entry (here 2 stray bytes) is ignored.
  const uint8_t partial[] = {1, 0, 0, 0, 9, 9};
  const std::vector<uint32_t> decoded = bookprogress::decodeBookmarks(partial, sizeof(partial));
  TEST_ASSERT_EQUAL_UINT32(1, decoded.size());
  TEST_ASSERT_EQUAL_UINT32(1, decoded[0]);
}

void test_insert_keeps_sorted_and_dedupes() {
  std::vector<uint32_t> marks;
  TEST_ASSERT_TRUE(bookprogress::insertBookmark(marks, 50));
  TEST_ASSERT_TRUE(bookprogress::insertBookmark(marks, 10));
  TEST_ASSERT_TRUE(bookprogress::insertBookmark(marks, 30));
  TEST_ASSERT_EQUAL_UINT32(3, marks.size());
  TEST_ASSERT_EQUAL_UINT32(10, marks[0]);
  TEST_ASSERT_EQUAL_UINT32(30, marks[1]);
  TEST_ASSERT_EQUAL_UINT32(50, marks[2]);
  // Duplicate is rejected, no change.
  TEST_ASSERT_FALSE(bookprogress::insertBookmark(marks, 30));
  TEST_ASSERT_EQUAL_UINT32(3, marks.size());
}

void test_insert_caps_at_max() {
  std::vector<uint32_t> marks;
  // Fill with widely-spaced marks: 0,100,200,... up to the cap.
  for (size_t i = 0; i < bookprogress::kMaxBookmarks; ++i) {
    TEST_ASSERT_TRUE(bookprogress::insertBookmark(marks, static_cast<uint32_t>(i * 100)));
  }
  TEST_ASSERT_EQUAL_UINT32(bookprogress::kMaxBookmarks, marks.size());
  // Inserting one near the top should evict the farthest (index 0), staying capped.
  const uint32_t nearTop = static_cast<uint32_t>((bookprogress::kMaxBookmarks - 1) * 100 + 1);
  TEST_ASSERT_TRUE(bookprogress::insertBookmark(marks, nearTop));
  TEST_ASSERT_EQUAL_UINT32(bookprogress::kMaxBookmarks, marks.size());
  // The farthest entry (0) was dropped; the new mark is present and sorted.
  TEST_ASSERT_NOT_EQUAL(0, marks.front());
  TEST_ASSERT_TRUE(std::is_sorted(marks.begin(), marks.end()));
  TEST_ASSERT_TRUE(std::find(marks.begin(), marks.end(), nearTop) != marks.end());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bookmark_key_format);
  RUN_TEST(test_encode_decode_round_trip);
  RUN_TEST(test_decode_empty_and_null);
  RUN_TEST(test_insert_keeps_sorted_and_dedupes);
  RUN_TEST(test_insert_caps_at_max);
  return UNITY_END();
}
