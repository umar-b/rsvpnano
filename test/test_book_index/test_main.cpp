#include <unity.h>

#include <cstring>
#include <vector>

#include "storage/BookIndex.h"

using bookindex::Builder;
using bookindex::BuilderConfig;
using bookindex::WordRecord;

namespace {

struct Captured {
  std::vector<WordRecord> records;
  String data;
  BookMetadata metadata;
};

struct BuildResult {
  Captured out;
  bool keepGoing = true;
  uint32_t wordCount = 0;
  uint32_t dataSize = 0;
  bool failed = false;
  String failure;
  booktext::ParseStats stats;
};

// Runs `text` through a Builder in `chunk`-byte feeds (0 = one feed),
// capturing records + data bytes the way StorageManager writes them to SD.
BuildResult build(const char *text, bool rsvpFormat, size_t chunk = 0,
                  BuilderConfig config = BuilderConfig(),
                  booktext::MemoryLowFn memoryLow = booktext::MemoryLowFn(),
                  bool failWrites = false) {
  BuildResult r;
  Builder builder(
      rsvpFormat, r.out.metadata,
      [&r, failWrites](const WordRecord &record, const char *bytes, size_t length) {
        if (failWrites) {
          return false;
        }
        r.out.records.push_back(record);
        for (size_t i = 0; i < length; ++i) {
          r.out.data += bytes[i];
        }
        return true;
      },
      memoryLow, config);

  const size_t len = std::strlen(text);
  if (chunk == 0) {
    chunk = len > 0 ? len : 1;
  }
  bool ok = true;
  for (size_t offset = 0; ok && offset < len; offset += chunk) {
    ok = builder.feed(reinterpret_cast<const uint8_t *>(text) + offset,
                      std::min(chunk, len - offset));
  }
  r.keepGoing = builder.finish();
  r.wordCount = builder.wordCount();
  r.dataSize = builder.dataSize();
  r.failed = builder.failed();
  r.failure = builder.failure();
  r.stats = builder.stats();
  return r;
}

String wordAt(const Captured &out, size_t index) {
  const WordRecord &record = out.records[index];
  String word;
  for (uint16_t i = 0; i < record.length; ++i) {
    word += out.data[record.offset + i];
  }
  return word;
}

}  // namespace

void test_records_are_contiguous_and_match_words(void) {
  const BuildResult r = build("one two\nthree\n", false);
  TEST_ASSERT_EQUAL_UINT32(3, r.wordCount);
  TEST_ASSERT_EQUAL_UINT32(3, r.out.records.size());
  TEST_ASSERT_EQUAL_STRING("one", wordAt(r.out, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("two", wordAt(r.out, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("three", wordAt(r.out, 2).c_str());

  uint32_t offset = 0;
  for (const WordRecord &record : r.out.records) {
    TEST_ASSERT_EQUAL_UINT32(offset, record.offset);
    offset += record.length;
  }
  TEST_ASSERT_EQUAL_UINT32(offset, r.dataSize);
  TEST_ASSERT_EQUAL_UINT32(r.wordCount, r.out.metadata.wordCount);
}

void test_paragraph_markers_on_blank_lines(void) {
  const BuildResult r = build("alpha beta\n\ngamma\n\n\ndelta\n", false);
  TEST_ASSERT_EQUAL_UINT32(4, r.wordCount);
  TEST_ASSERT_EQUAL_UINT32(3, r.out.metadata.paragraphStarts.size());
  TEST_ASSERT_EQUAL_UINT32(0, r.out.metadata.paragraphStarts[0]);
  TEST_ASSERT_EQUAL_UINT32(2, r.out.metadata.paragraphStarts[1]);
  TEST_ASSERT_EQUAL_UINT32(3, r.out.metadata.paragraphStarts[2]);
}

void test_rsvp_directives(void) {
  const BuildResult r = build(
      "@rsvp 1\n"
      "@title  My Book \n"
      "@author Jane\n"
      "@unknown thing\n"
      "\n"
      "first words here\n"
      "@chapter Part One\n"
      "more text\n"
      "@para\n"
      "after break\n",
      true);
  TEST_ASSERT_EQUAL_STRING("My Book", r.out.metadata.title.c_str());
  TEST_ASSERT_EQUAL_STRING("Jane", r.out.metadata.author.c_str());
  // "first words here" (3) + "more text" (2) + "after break" (2)
  TEST_ASSERT_EQUAL_UINT32(7, r.wordCount);
  TEST_ASSERT_EQUAL_UINT32(1, r.out.metadata.chapters.size());
  TEST_ASSERT_EQUAL_STRING("Part One", r.out.metadata.chapters[0].title.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, r.out.metadata.chapters[0].wordIndex);
  // Paragraphs: word 0, word 3 (after @chapter), word 5 (after @para).
  TEST_ASSERT_EQUAL_UINT32(3, r.out.metadata.paragraphStarts.size());
  TEST_ASSERT_EQUAL_UINT32(5, r.out.metadata.paragraphStarts[2]);
}

void test_rsvp_escaped_at_line(void) {
  const BuildResult r = build("@@chapter literal\n", true);
  TEST_ASSERT_EQUAL_UINT32(2, r.wordCount);
  TEST_ASSERT_EQUAL_STRING("@chapter", wordAt(r.out, 0).c_str());
  TEST_ASSERT_TRUE(r.out.metadata.chapters.empty());
}

void test_empty_chapter_directive_gets_default_title(void) {
  const BuildResult r = build("@chapter\nwords go here\n", true);
  TEST_ASSERT_EQUAL_UINT32(1, r.out.metadata.chapters.size());
  TEST_ASSERT_EQUAL_STRING("Chapter", r.out.metadata.chapters[0].title.c_str());
}

void test_consecutive_chapters_at_same_word_replace(void) {
  const BuildResult r = build("@chapter First\n@chapter Second\nbody\n", true);
  TEST_ASSERT_EQUAL_UINT32(1, r.out.metadata.chapters.size());
  TEST_ASSERT_EQUAL_STRING("Second", r.out.metadata.chapters[0].title.c_str());
}

void test_byte_at_a_time_matches_single_feed(void) {
  const char *text = "@title Split\n\nalpha beta\r\n@chapter One\ngamma...\n";
  const BuildResult whole = build(text, true);
  const BuildResult split = build(text, true, 1);
  TEST_ASSERT_EQUAL_UINT32(whole.wordCount, split.wordCount);
  TEST_ASSERT_EQUAL_STRING(whole.out.data.c_str(), split.out.data.c_str());
  TEST_ASSERT_EQUAL_UINT32(whole.out.metadata.paragraphStarts.size(),
                           split.out.metadata.paragraphStarts.size());
  TEST_ASSERT_EQUAL_STRING(whole.out.metadata.title.c_str(),
                           split.out.metadata.title.c_str());
}

void test_long_lines_split_and_counted(void) {
  BuilderConfig config;
  config.maxLineChars = 8;
  const BuildResult r = build("aaa bbb ccc ddd\n", false, 0, config);
  TEST_ASSERT_TRUE(r.stats.longLineSplits > 0);
  TEST_ASSERT_TRUE(r.wordCount >= 4);
  TEST_ASSERT_FALSE(r.failed);
}

void test_trailing_line_without_newline_is_processed(void) {
  const BuildResult r = build("no newline at end", false);
  TEST_ASSERT_EQUAL_UINT32(4, r.wordCount);
  TEST_ASSERT_EQUAL_STRING("end", wordAt(r.out, 3).c_str());
}

void test_memory_low_stops_build(void) {
  BuilderConfig config;
  config.memoryCheckWordInterval = 2;
  const BuildResult r =
      build("one two three four five\n", false, 0, config, [] { return true; });
  TEST_ASSERT_TRUE(r.failed);
  TEST_ASSERT_FALSE(r.keepGoing);
  TEST_ASSERT_TRUE(r.stats.memoryLow);
  TEST_ASSERT_EQUAL_STRING("Memory limit reached", r.failure.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, r.wordCount);
}

void test_write_failure_reports_sd_error(void) {
  const BuildResult r = build("some words\n", false, 0, BuilderConfig(),
                              booktext::MemoryLowFn(), true);
  TEST_ASSERT_TRUE(r.failed);
  TEST_ASSERT_EQUAL_STRING("SD write failed", r.failure.c_str());
  TEST_ASSERT_EQUAL_UINT32(0, r.wordCount);
}

void test_unreadable_tokens_are_skipped(void) {
  const BuildResult r = build("word & ) ( word2\n", false);
  TEST_ASSERT_EQUAL_UINT32(2, r.wordCount);
  TEST_ASSERT_EQUAL_STRING("word", wordAt(r.out, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("word2", wordAt(r.out, 1).c_str());
}

void test_layout_header_offsets(void) {
  const bookindex::Header header = bookindex::layoutHeader(1234, 0xABCD, 10, 3, 2, 55);
  TEST_ASSERT_EQUAL_UINT32(bookindex::kMagic, header.magic);
  TEST_ASSERT_EQUAL_UINT32(bookindex::kVersion, header.version);
  TEST_ASSERT_EQUAL_UINT32(sizeof(bookindex::Header), header.headerSize);
  TEST_ASSERT_EQUAL_UINT32(sizeof(WordRecord), header.recordSize);
  TEST_ASSERT_EQUAL_UINT32(sizeof(bookindex::Header), header.recordsOffset);
  TEST_ASSERT_EQUAL_UINT32(header.recordsOffset + 10 * sizeof(WordRecord),
                           header.paragraphsOffset);
  TEST_ASSERT_EQUAL_UINT32(header.paragraphsOffset + 3 * sizeof(uint32_t),
                           header.chaptersOffset);
  TEST_ASSERT_EQUAL_UINT32(55, header.dataSize);
  TEST_ASSERT_EQUAL_UINT32(10, header.wordCount);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_records_are_contiguous_and_match_words);
  RUN_TEST(test_paragraph_markers_on_blank_lines);
  RUN_TEST(test_rsvp_directives);
  RUN_TEST(test_rsvp_escaped_at_line);
  RUN_TEST(test_empty_chapter_directive_gets_default_title);
  RUN_TEST(test_consecutive_chapters_at_same_word_replace);
  RUN_TEST(test_byte_at_a_time_matches_single_feed);
  RUN_TEST(test_long_lines_split_and_counted);
  RUN_TEST(test_trailing_line_without_newline_is_processed);
  RUN_TEST(test_memory_low_stops_build);
  RUN_TEST(test_write_failure_reports_sd_error);
  RUN_TEST(test_unreadable_tokens_are_skipped);
  RUN_TEST(test_layout_header_offsets);
  return UNITY_END();
}
