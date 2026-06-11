#include <unity.h>

#include "timer/SprintAccount.h"

using sprint::SprintAccount;
using sprint::netForwardWords;

void test_net_forward_words_basic() {
  TEST_ASSERT_EQUAL_UINT32(50, netForwardWords(100, 150));
  TEST_ASSERT_EQUAL_UINT32(0, netForwardWords(150, 150));
}

void test_net_forward_words_decrement_safe() {
  // Scrub-back: end < start contributes zero, never a negative.
  TEST_ASSERT_EQUAL_UINT32(0, netForwardWords(200, 100));
}

void test_block_started_playing_counts_forward_progress() {
  SprintAccount a;
  a.beginBlock(1000, /*playing=*/true);
  TEST_ASSERT_TRUE(a.blockActive());
  TEST_ASSERT_TRUE(a.segmentOpen());
  const uint32_t total = a.finishBlock(1300);
  TEST_ASSERT_EQUAL_UINT32(300, total);
  TEST_ASSERT_FALSE(a.blockActive());
}

void test_block_started_paused_then_play() {
  SprintAccount a;
  a.beginBlock(500, /*playing=*/false);
  TEST_ASSERT_FALSE(a.segmentOpen());
  a.enterPlaying(500);
  a.leavePlaying(560);  // read 60 words
  TEST_ASSERT_EQUAL_UINT32(60, a.wordsRead());
  const uint32_t total = a.finishBlock(560);  // no open segment, stays 60
  TEST_ASSERT_EQUAL_UINT32(60, total);
}

void test_multiple_play_segments_accumulate() {
  SprintAccount a;
  a.beginBlock(0, /*playing=*/false);
  a.enterPlaying(0);
  a.leavePlaying(40);  // +40
  a.enterPlaying(40);
  a.leavePlaying(100);  // +60
  TEST_ASSERT_EQUAL_UINT32(100, a.wordsRead());
  TEST_ASSERT_EQUAL_UINT32(100, a.finishBlock(100));
}

void test_scrub_back_within_segment_does_not_subtract() {
  SprintAccount a;
  a.beginBlock(100, /*playing=*/true);
  // Reader scrubbed backwards before the block ends.
  const uint32_t total = a.finishBlock(80);
  TEST_ASSERT_EQUAL_UINT32(0, total);
}

void test_scrub_back_between_segments_restarts_from_new_index() {
  SprintAccount a;
  a.beginBlock(0, /*playing=*/false);
  a.enterPlaying(0);
  a.leavePlaying(100);  // +100
  // User scrubs back to 50 while paused, then resumes and reads to 90.
  a.enterPlaying(50);
  a.leavePlaying(90);  // +40 (net forward within the new segment)
  TEST_ASSERT_EQUAL_UINT32(140, a.wordsRead());
}

void test_finish_block_with_open_segment_closes_it() {
  SprintAccount a;
  a.beginBlock(10, /*playing=*/true);  // segment open at 10
  const uint32_t total = a.finishBlock(35);
  TEST_ASSERT_EQUAL_UINT32(25, total);
  TEST_ASSERT_FALSE(a.segmentOpen());
}

void test_finish_block_idempotent() {
  SprintAccount a;
  a.beginBlock(0, /*playing=*/true);
  const uint32_t first = a.finishBlock(200);
  const uint32_t second = a.finishBlock(999);  // already finished
  TEST_ASSERT_EQUAL_UINT32(200, first);
  TEST_ASSERT_EQUAL_UINT32(200, second);
}

void test_enter_playing_without_block_is_noop() {
  SprintAccount a;
  a.enterPlaying(100);
  TEST_ASSERT_FALSE(a.segmentOpen());
  TEST_ASSERT_FALSE(a.blockActive());
}

void test_begin_block_resets_prior_total() {
  SprintAccount a;
  a.beginBlock(0, /*playing=*/true);
  a.finishBlock(500);
  a.beginBlock(1000, /*playing=*/true);
  TEST_ASSERT_EQUAL_UINT32(0, a.wordsRead());
  TEST_ASSERT_EQUAL_UINT32(50, a.finishBlock(1050));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_net_forward_words_basic);
  RUN_TEST(test_net_forward_words_decrement_safe);
  RUN_TEST(test_block_started_playing_counts_forward_progress);
  RUN_TEST(test_block_started_paused_then_play);
  RUN_TEST(test_multiple_play_segments_accumulate);
  RUN_TEST(test_scrub_back_within_segment_does_not_subtract);
  RUN_TEST(test_scrub_back_between_segments_restarts_from_new_index);
  RUN_TEST(test_finish_block_with_open_segment_closes_it);
  RUN_TEST(test_finish_block_idempotent);
  RUN_TEST(test_enter_playing_without_block_is_noop);
  RUN_TEST(test_begin_block_resets_prior_total);
  return UNITY_END();
}
