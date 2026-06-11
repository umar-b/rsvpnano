#include <unity.h>

#include "library/LibraryFilter.h"

using library::BookStatus;
using library::EmptyReason;
using library::LibraryFilter;

// ---- classification ----

void test_classify_finished_wins() {
  // Finished flag always wins, regardless of progress.
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::Finished),
                    static_cast<int>(library::classify(true, false, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::Finished),
                    static_cast<int>(library::classify(true, true, 42)));
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::Finished),
                    static_cast<int>(library::classify(true, true, 100)));
}

void test_classify_in_progress() {
  // Saved progress > 0 and not finished.
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::InProgress),
                    static_cast<int>(library::classify(false, true, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::InProgress),
                    static_cast<int>(library::classify(false, true, 99)));
}

void test_classify_unread() {
  // No progress at all.
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::Unread),
                    static_cast<int>(library::classify(false, false, 0)));
  // hasProgress but percent 0 still reads as unread (just opened, never moved).
  TEST_ASSERT_EQUAL(static_cast<int>(BookStatus::Unread),
                    static_cast<int>(library::classify(false, true, 0)));
}

// ---- filter cycling + raw normalization ----

void test_next_filter_cycles() {
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::InProgress),
                    static_cast<int>(library::nextFilter(LibraryFilter::All)));
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::Unread),
                    static_cast<int>(library::nextFilter(LibraryFilter::InProgress)));
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::Finished),
                    static_cast<int>(library::nextFilter(LibraryFilter::Unread)));
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::All),
                    static_cast<int>(library::nextFilter(LibraryFilter::Finished)));
}

void test_filter_from_raw_normalizes() {
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::All),
                    static_cast<int>(library::filterFromRaw(0)));
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::Finished),
                    static_cast<int>(library::filterFromRaw(3)));
  // Out-of-range raw -> All (corrupted NVS defense).
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::All),
                    static_cast<int>(library::filterFromRaw(4)));
  TEST_ASSERT_EQUAL(static_cast<int>(LibraryFilter::All),
                    static_cast<int>(library::filterFromRaw(255)));
}

// ---- matches + filterIndices ----

void test_matches() {
  TEST_ASSERT_TRUE(library::matches(LibraryFilter::All, BookStatus::Unread));
  TEST_ASSERT_TRUE(library::matches(LibraryFilter::All, BookStatus::Finished));
  TEST_ASSERT_TRUE(library::matches(LibraryFilter::InProgress, BookStatus::InProgress));
  TEST_ASSERT_FALSE(library::matches(LibraryFilter::InProgress, BookStatus::Unread));
  TEST_ASSERT_TRUE(library::matches(LibraryFilter::Unread, BookStatus::Unread));
  TEST_ASSERT_FALSE(library::matches(LibraryFilter::Unread, BookStatus::Finished));
  TEST_ASSERT_TRUE(library::matches(LibraryFilter::Finished, BookStatus::Finished));
  TEST_ASSERT_FALSE(library::matches(LibraryFilter::Finished, BookStatus::InProgress));
}

void test_filter_indices_preserves_order() {
  std::vector<BookStatus> statuses = {
      BookStatus::Unread,      // 0
      BookStatus::Finished,    // 1
      BookStatus::InProgress,  // 2
      BookStatus::Unread,      // 3
      BookStatus::Finished,    // 4
  };

  const std::vector<size_t> all = library::filterIndices(LibraryFilter::All, statuses);
  TEST_ASSERT_EQUAL_UINT(5, all.size());

  const std::vector<size_t> unread = library::filterIndices(LibraryFilter::Unread, statuses);
  TEST_ASSERT_EQUAL_UINT(2, unread.size());
  TEST_ASSERT_EQUAL_UINT(0, unread[0]);
  TEST_ASSERT_EQUAL_UINT(3, unread[1]);

  const std::vector<size_t> finished = library::filterIndices(LibraryFilter::Finished, statuses);
  TEST_ASSERT_EQUAL_UINT(2, finished.size());
  TEST_ASSERT_EQUAL_UINT(1, finished[0]);
  TEST_ASSERT_EQUAL_UINT(4, finished[1]);

  const std::vector<size_t> inProgress =
      library::filterIndices(LibraryFilter::InProgress, statuses);
  TEST_ASSERT_EQUAL_UINT(1, inProgress.size());
  TEST_ASSERT_EQUAL_UINT(2, inProgress[0]);
}

void test_filter_indices_empty_input() {
  std::vector<BookStatus> empty;
  TEST_ASSERT_EQUAL_UINT(0, library::filterIndices(LibraryFilter::All, empty).size());
  TEST_ASSERT_EQUAL_UINT(0, library::filterIndices(LibraryFilter::Finished, empty).size());
}

// ---- empty-result reasoning ----

void test_empty_reason_not_empty() {
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NotEmpty),
                    static_cast<int>(library::emptyReason(LibraryFilter::All, 5, 5)));
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NotEmpty),
                    static_cast<int>(library::emptyReason(LibraryFilter::Finished, 5, 2)));
}

void test_empty_reason_no_items_at_all() {
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NoItemsAtAll),
                    static_cast<int>(library::emptyReason(LibraryFilter::All, 0, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NoItemsAtAll),
                    static_cast<int>(library::emptyReason(LibraryFilter::Finished, 0, 0)));
}

void test_empty_reason_per_filter() {
  // Library has items, but the filter matched none.
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NoInProgress),
                    static_cast<int>(library::emptyReason(LibraryFilter::InProgress, 4, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NoUnread),
                    static_cast<int>(library::emptyReason(LibraryFilter::Unread, 4, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(EmptyReason::NoFinished),
                    static_cast<int>(library::emptyReason(LibraryFilter::Finished, 4, 0)));
}

// ---- surprise pick ----

void test_surprise_pick_empty_returns_no_pick() {
  std::vector<BookStatus> empty;
  TEST_ASSERT_EQUAL_UINT(library::kNoPick, library::surprisePick(empty, 12345));
}

void test_surprise_pick_prefers_unread() {
  std::vector<BookStatus> statuses = {
      BookStatus::Finished,    // 0
      BookStatus::Unread,      // 1
      BookStatus::InProgress,  // 2
      BookStatus::Unread,      // 3
  };
  // Two unread at indices 1 and 3. randomValue % 2 selects within them.
  TEST_ASSERT_EQUAL_UINT(1, library::surprisePick(statuses, 0));   // 0 % 2 -> unread[0] = 1
  TEST_ASSERT_EQUAL_UINT(3, library::surprisePick(statuses, 1));   // 1 % 2 -> unread[1] = 3
  TEST_ASSERT_EQUAL_UINT(1, library::surprisePick(statuses, 2));   // 2 % 2 -> unread[0] = 1
  TEST_ASSERT_EQUAL_UINT(3, library::surprisePick(statuses, 99));  // 99 % 2 -> unread[1] = 3
}

void test_surprise_pick_falls_back_to_any() {
  // No unread items -> fall back to any item across the whole pool.
  std::vector<BookStatus> statuses = {
      BookStatus::Finished,    // 0
      BookStatus::InProgress,  // 1
      BookStatus::Finished,    // 2
  };
  TEST_ASSERT_EQUAL_UINT(0, library::surprisePick(statuses, 0));
  TEST_ASSERT_EQUAL_UINT(1, library::surprisePick(statuses, 1));
  TEST_ASSERT_EQUAL_UINT(2, library::surprisePick(statuses, 2));
  TEST_ASSERT_EQUAL_UINT(0, library::surprisePick(statuses, 3));  // wraps
}

void test_surprise_pick_single_item() {
  std::vector<BookStatus> one = {BookStatus::Finished};
  TEST_ASSERT_EQUAL_UINT(0, library::surprisePick(one, 0));
  TEST_ASSERT_EQUAL_UINT(0, library::surprisePick(one, 777));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_classify_finished_wins);
  RUN_TEST(test_classify_in_progress);
  RUN_TEST(test_classify_unread);
  RUN_TEST(test_next_filter_cycles);
  RUN_TEST(test_filter_from_raw_normalizes);
  RUN_TEST(test_matches);
  RUN_TEST(test_filter_indices_preserves_order);
  RUN_TEST(test_filter_indices_empty_input);
  RUN_TEST(test_empty_reason_not_empty);
  RUN_TEST(test_empty_reason_no_items_at_all);
  RUN_TEST(test_empty_reason_per_filter);
  RUN_TEST(test_surprise_pick_empty_returns_no_pick);
  RUN_TEST(test_surprise_pick_prefers_unread);
  RUN_TEST(test_surprise_pick_falls_back_to_any);
  RUN_TEST(test_surprise_pick_single_item);
  return UNITY_END();
}
