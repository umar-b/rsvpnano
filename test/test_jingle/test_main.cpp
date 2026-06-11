#include <unity.h>

#include <vector>

#include "audio/Jingle.h"

using namespace jingle;

void test_frames_for_ms_matches_sample_rate() {
  // 16000 Hz -> 16 frames per ms.
  TEST_ASSERT_EQUAL_UINT32(16, framesForMs(1));
  TEST_ASSERT_EQUAL_UINT32(1600, framesForMs(100));
  TEST_ASSERT_EQUAL_UINT32(0, framesForMs(0));
}

void test_samples_for_ms_is_stereo() {
  TEST_ASSERT_EQUAL_UINT32(framesForMs(50) * 2U, samplesForMs(50));
}

void test_named_jingles_are_within_budget() {
  TEST_ASSERT_TRUE(isWithinBudget(completionArpeggio()));
  TEST_ASSERT_TRUE(isWithinBudget(chapterChime()));
  TEST_ASSERT_TRUE(isWithinBudget(bookFanfare()));
  TEST_ASSERT_TRUE(isWithinBudget(achievementPing()));
}

void test_named_jingles_under_one_second() {
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(kMaxSequenceDurationMs,
                                   sequenceDurationMs(completionArpeggio()));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(kMaxSequenceDurationMs,
                                   sequenceDurationMs(bookFanfare()));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(kMaxSequenceDurationMs,
                                   sequenceDurationMs(chapterChime()));
}

void test_completion_arpeggio_note_count_and_ascending() {
  Sequence seq = completionArpeggio();
  TEST_ASSERT_EQUAL_UINT32(4, seq.count);
  for (size_t i = 1; i < seq.count; ++i) {
    TEST_ASSERT_TRUE(seq.notes[i].frequencyHz > seq.notes[i - 1].frequencyHz);
  }
}

void test_chapter_chime_is_two_notes() {
  TEST_ASSERT_EQUAL_UINT32(2, chapterChime().count);
}

void test_book_fanfare_is_five_notes() {
  TEST_ASSERT_EQUAL_UINT32(5, bookFanfare().count);
}

void test_achievement_ping_is_single_note() {
  TEST_ASSERT_EQUAL_UINT32(1, achievementPing().count);
}

void test_sequence_duration_sums_tone_and_gap() {
  Sequence seq{};
  seq.count = 2;
  seq.notes[0] = {440, 100, 50};
  seq.notes[1] = {880, 100, 0};
  TEST_ASSERT_EQUAL_UINT32(250, sequenceDurationMs(seq));
}

void test_sequence_sample_count_matches_frames() {
  Sequence seq{};
  seq.count = 1;
  seq.notes[0] = {440, 100, 50};  // 150ms -> 2400 frames -> 4800 samples
  TEST_ASSERT_EQUAL_UINT32(framesForMs(150) * 2U, sequenceSampleCount(seq));
}

void test_over_budget_sequence_rejected() {
  Sequence seq{};
  seq.count = 2;
  seq.notes[0] = {440, 600, 0};
  seq.notes[1] = {880, 600, 0};  // 1200ms > 1000ms cap
  TEST_ASSERT_FALSE(isWithinBudget(seq));
}

void test_empty_sequence_rejected() {
  Sequence seq{};
  seq.count = 0;
  TEST_ASSERT_FALSE(isWithinBudget(seq));
}

void test_envelope_ramps_up_from_zero() {
  const size_t toneFrames = framesForMs(100);
  // First frame of the attack starts at (or near) zero.
  TEST_ASSERT_EQUAL_INT32(0, envelopeScaleQ10(0, toneFrames));
  // Middle of the tone is full-scale.
  TEST_ASSERT_EQUAL_INT32(1024, envelopeScaleQ10(toneFrames / 2, toneFrames));
}

void test_envelope_ramps_down_to_near_zero() {
  const size_t toneFrames = framesForMs(100);
  // The very last frame's release is near zero (release window ends at 0).
  const int32_t last = envelopeScaleQ10(toneFrames - 1, toneFrames);
  TEST_ASSERT_TRUE(last >= 0);
  TEST_ASSERT_TRUE(last < 1024);
}

void test_envelope_bounded_zero_to_one() {
  const size_t toneFrames = framesForMs(120);
  for (size_t f = 0; f < toneFrames; ++f) {
    const int32_t s = envelopeScaleQ10(f, toneFrames);
    TEST_ASSERT_TRUE(s >= 0);
    TEST_ASSERT_TRUE(s <= 1024);
  }
}

void test_zero_tone_frames_envelope_is_zero() {
  TEST_ASSERT_EQUAL_INT32(0, envelopeScaleQ10(0, 0));
}

void test_render_sequence_fills_expected_frames() {
  Sequence seq = chapterChime();
  const size_t totalFrames = sequenceSampleCount(seq) / kChannels;
  std::vector<int16_t> buffer(sequenceSampleCount(seq) + 8, 0);
  const size_t written = renderSequence(seq, buffer.data(), totalFrames);
  TEST_ASSERT_EQUAL_UINT32(totalFrames, written);
  // Both stereo channels carry the same sample at the first tone's peak region.
  const size_t midFrame = framesForMs(seq.notes[0].durationMs) / 2;
  TEST_ASSERT_EQUAL_INT16(buffer[midFrame * 2], buffer[midFrame * 2 + 1]);
}

void test_render_respects_capacity() {
  Sequence seq = completionArpeggio();
  const size_t cap = framesForMs(10);  // far smaller than the sequence
  std::vector<int16_t> buffer(cap * kChannels, 123);
  const size_t written = renderSequence(seq, buffer.data(), cap);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(cap, written);
}

void test_rest_note_is_silent() {
  Note rest{0, 20, 0};
  const size_t frames = framesForMs(20);
  std::vector<int16_t> buffer(frames * kChannels, 99);
  const size_t written = renderNote(rest, buffer.data(), 0, frames);
  TEST_ASSERT_EQUAL_UINT32(frames, written);
  for (size_t i = 0; i < frames * kChannels; ++i) {
    TEST_ASSERT_EQUAL_INT16(0, buffer[i]);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_frames_for_ms_matches_sample_rate);
  RUN_TEST(test_samples_for_ms_is_stereo);
  RUN_TEST(test_named_jingles_are_within_budget);
  RUN_TEST(test_named_jingles_under_one_second);
  RUN_TEST(test_completion_arpeggio_note_count_and_ascending);
  RUN_TEST(test_chapter_chime_is_two_notes);
  RUN_TEST(test_book_fanfare_is_five_notes);
  RUN_TEST(test_achievement_ping_is_single_note);
  RUN_TEST(test_sequence_duration_sums_tone_and_gap);
  RUN_TEST(test_sequence_sample_count_matches_frames);
  RUN_TEST(test_over_budget_sequence_rejected);
  RUN_TEST(test_empty_sequence_rejected);
  RUN_TEST(test_envelope_ramps_up_from_zero);
  RUN_TEST(test_envelope_ramps_down_to_near_zero);
  RUN_TEST(test_envelope_bounded_zero_to_one);
  RUN_TEST(test_zero_tone_frames_envelope_is_zero);
  RUN_TEST(test_render_sequence_fills_expected_frames);
  RUN_TEST(test_render_respects_capacity);
  RUN_TEST(test_rest_note_is_silent);
  return UNITY_END();
}
