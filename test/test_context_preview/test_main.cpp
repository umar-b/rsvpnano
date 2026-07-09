#include <unity.h>

#include <vector>

#include "reader/ContextPreview.h"

namespace {

// A small book: "w0" .. "w49", paragraphs starting at 0, 10, 30.
std::vector<String> makeWords(size_t count) {
  std::vector<String> words;
  for (size_t i = 0; i < count; ++i) {
    words.push_back(String("w") + String(static_cast<unsigned int>(i)));
  }
  return words;
}

const std::vector<String> kWords = makeWords(50);
const std::vector<size_t> kParagraphs = {0, 10, 30};

String wordAt(size_t i) { return kWords[i]; }

// Small window so tests don't need 288-word fixtures.
contextpreview::Config smallConfig() {
  contextpreview::Config config;
  config.windowWords = 8;
  config.anchorLeadWords = 3;
  config.maxParagraphSnapWords = 2;
  return config;
}

size_t currentIndexIn(const contextpreview::Window &window) {
  const auto &words = window.words();
  for (size_t i = 0; i < words.size(); ++i) {
    if (words[i].current) {
      return window.startIndex() + i;
    }
  }
  return static_cast<size_t>(-1);
}

}  // namespace

void test_paragraph_start_index_zero_always() {
  TEST_ASSERT_TRUE(contextpreview::isParagraphStart({}, 0));
  TEST_ASSERT_TRUE(contextpreview::isParagraphStart(kParagraphs, 10));
  TEST_ASSERT_FALSE(contextpreview::isParagraphStart(kParagraphs, 11));
}

void test_paragraph_start_at_or_before() {
  TEST_ASSERT_EQUAL_UINT32(0, contextpreview::paragraphStartAtOrBefore(kParagraphs, 5));
  TEST_ASSERT_EQUAL_UINT32(10, contextpreview::paragraphStartAtOrBefore(kParagraphs, 10));
  TEST_ASSERT_EQUAL_UINT32(10, contextpreview::paragraphStartAtOrBefore(kParagraphs, 29));
  TEST_ASSERT_EQUAL_UINT32(30, contextpreview::paragraphStartAtOrBefore(kParagraphs, 49));
  TEST_ASSERT_EQUAL_UINT32(0, contextpreview::paragraphStartAtOrBefore({}, 20));
}

void test_anchor_below_lead_is_zero() {
  TEST_ASSERT_EQUAL_UINT32(0, contextpreview::anchorIndex(3, kParagraphs, smallConfig()));
}

void test_anchor_snaps_to_nearby_paragraph() {
  // current=14 -> target=11, paragraph 10 is 1 word back (<= snap 2): snap.
  TEST_ASSERT_EQUAL_UINT32(10, contextpreview::anchorIndex(14, kParagraphs, smallConfig()));
}

void test_anchor_keeps_target_when_paragraph_far() {
  // current=23 -> target=20, paragraph 10 is 10 words back (> snap 2): no snap.
  TEST_ASSERT_EQUAL_UINT32(20, contextpreview::anchorIndex(23, kParagraphs, smallConfig()));
}

void test_collect_before_fills_budget_in_order() {
  // Budget 5 chars: pulls w4 (2 chars), then w3 (+1 separator +2 = 5): stop.
  TEST_ASSERT_EQUAL_STRING("w3 w4", contextpreview::collectBefore(5, 5, wordAt).c_str());
  TEST_ASSERT_EQUAL_STRING("", contextpreview::collectBefore(0, 10, wordAt).c_str());
  TEST_ASSERT_EQUAL_STRING("", contextpreview::collectBefore(5, 0, wordAt).c_str());
}

void test_collect_after_fills_budget_in_order() {
  TEST_ASSERT_EQUAL_STRING("w6 w7", contextpreview::collectAfter(5, 50, 5, wordAt).c_str());
  TEST_ASSERT_EQUAL_STRING("", contextpreview::collectAfter(49, 50, 10, wordAt).c_str());
  TEST_ASSERT_EQUAL_STRING("", contextpreview::collectAfter(5, 0, 10, wordAt).c_str());
}

void test_window_builds_around_current() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  // anchor(20) -> target 17, paragraph 10 too far: start 17, 8 words.
  TEST_ASSERT_EQUAL_UINT32(17, window.startIndex());
  TEST_ASSERT_EQUAL_UINT32(8, window.words().size());
  TEST_ASSERT_EQUAL_STRING("w17", window.words()[0].text.c_str());
  TEST_ASSERT_EQUAL_UINT32(20, currentIndexIn(window));
}

void test_window_advances_highlight_without_rebuild() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  const size_t start = window.startIndex();
  window.update(21, 50, kParagraphs, wordAt, smallConfig());
  TEST_ASSERT_EQUAL_UINT32(start, window.startIndex());
  TEST_ASSERT_EQUAL_UINT32(21, currentIndexIn(window));
}

void test_window_rebuilds_when_current_reaches_trailing_edge() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  // Window is [17, 25); reaching 24 (last word, more book ahead) rebuilds.
  window.update(24, 50, kParagraphs, wordAt, smallConfig());
  TEST_ASSERT_EQUAL_UINT32(21, window.startIndex());
  TEST_ASSERT_EQUAL_UINT32(24, currentIndexIn(window));
}

void test_window_rebuilds_when_current_moves_before_start() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  window.update(5, 50, kParagraphs, wordAt, smallConfig());
  TEST_ASSERT_EQUAL_UINT32(5, currentIndexIn(window));
  TEST_ASSERT_TRUE(window.startIndex() <= 5);
}

void test_window_marks_paragraph_starts() {
  contextpreview::Window window;
  window.update(11, 50, kParagraphs, wordAt, smallConfig());
  // anchor(11) -> target 8, paragraph 10 not <= 8... target 8, paragraphStartAtOrBefore(8)=0,
  // 8-0 > 2: start 8. Window [8,16) contains word 10.
  TEST_ASSERT_EQUAL_UINT32(8, window.startIndex());
  TEST_ASSERT_FALSE(window.words()[0].paragraphStart);
  TEST_ASSERT_TRUE(window.words()[2].paragraphStart);  // word 10
}

void test_window_empty_book_invalidates() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  window.update(0, 0, kParagraphs, wordAt, smallConfig());
  TEST_ASSERT_EQUAL_UINT32(0, window.words().size());
}

void test_window_invalidate_forces_rebuild() {
  contextpreview::Window window;
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  window.invalidate();
  TEST_ASSERT_EQUAL_UINT32(0, window.words().size());
  window.update(20, 50, kParagraphs, wordAt, smallConfig());
  TEST_ASSERT_EQUAL_UINT32(8, window.words().size());
  TEST_ASSERT_EQUAL_UINT32(20, currentIndexIn(window));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_paragraph_start_index_zero_always);
  RUN_TEST(test_paragraph_start_at_or_before);
  RUN_TEST(test_anchor_below_lead_is_zero);
  RUN_TEST(test_anchor_snaps_to_nearby_paragraph);
  RUN_TEST(test_anchor_keeps_target_when_paragraph_far);
  RUN_TEST(test_collect_before_fills_budget_in_order);
  RUN_TEST(test_collect_after_fills_budget_in_order);
  RUN_TEST(test_window_builds_around_current);
  RUN_TEST(test_window_advances_highlight_without_rebuild);
  RUN_TEST(test_window_rebuilds_when_current_reaches_trailing_edge);
  RUN_TEST(test_window_rebuilds_when_current_moves_before_start);
  RUN_TEST(test_window_marks_paragraph_starts);
  RUN_TEST(test_window_empty_book_invalidates);
  RUN_TEST(test_window_invalidate_forces_rebuild);
  return UNITY_END();
}
