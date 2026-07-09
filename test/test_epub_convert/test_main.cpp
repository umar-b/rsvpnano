#include <unity.h>

#include <algorithm>
#include <cstring>

#include "text/EpubConvert.h"

namespace {

struct ConvertResult {
  String out;
  bool ok = true;
  bool limit = false;
  size_t words = 0;
};

// Streams html through an RsvpWriter in `chunk`-byte writes (0 = one write),
// collecting the .rsvp output. Mirrors the device flow: finish() only runs
// when every write succeeded.
ConvertResult convert(const char *html, size_t maxWords = 0, size_t chunk = 0) {
  ConvertResult r;
  String lastChapter;
  epubconvert::RsvpWriter writer(
      [&r](const char *data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
          r.out += data[i];
        }
      },
      r.words, maxWords, lastChapter);

  const size_t len = std::strlen(html);
  if (chunk == 0) {
    chunk = len > 0 ? len : 1;
  }
  bool ok = true;
  for (size_t offset = 0; ok && offset < len; offset += chunk) {
    ok = writer.write(reinterpret_cast<const uint8_t *>(html) + offset,
                      std::min(chunk, len - offset));
  }
  r.ok = ok && writer.finish();
  r.limit = writer.reachedWordLimit();
  return r;
}

}  // namespace

void test_paragraphs_become_lines(void) {
  const ConvertResult r = convert("<p>one two three</p><p>four</p>");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("one two three\r\nfour\r\n", r.out.c_str());
  TEST_ASSERT_EQUAL_UINT32(4, r.words);
}

void test_heading_becomes_chapter_marker_once(void) {
  const ConvertResult r =
      convert("<h1>Intro</h1><p>text</p><h2>Intro</h2><p>more</p><h2>Next</h2><p>end</p>");
  TEST_ASSERT_EQUAL_STRING("@chapter Intro\r\ntext\r\nmore\r\n@chapter Next\r\nend\r\n",
                           r.out.c_str());
}

void test_skip_tags_dropped_including_nested(void) {
  const ConvertResult r = convert(
      "<p>keep</p><style>body { color: red }</style>"
      "<svg><script>var x = 1;</script>hidden</svg><p>tail</p>");
  TEST_ASSERT_EQUAL_STRING("keep\r\ntail\r\n", r.out.c_str());
}

void test_entities_decoded(void) {
  const ConvertResult joined = convert("<p>Tom&amp;Jerry laughed</p>");
  TEST_ASSERT_EQUAL_STRING("Tom&Jerry laughed\r\n", joined.out.c_str());

  // A standalone non-readable token is dropped by design.
  const ConvertResult spaced = convert("<p>Tom &amp; Jerry</p>");
  TEST_ASSERT_EQUAL_STRING("Tom Jerry\r\n", spaced.out.c_str());
}

void test_comments_dropped(void) {
  const ConvertResult r = convert("<p>a<!-- <p>hidden words</p> -->b</p>");
  TEST_ASSERT_EQUAL_STRING("ab\r\n", r.out.c_str());
}

void test_byte_at_a_time_matches_single_write(void) {
  const char *html =
      "<h1>Ch &amp; One</h1><!-- note --><p>Tom &amp; Jerry went on... "
      "a well-known trip &#8212; twice</p>";
  const ConvertResult whole = convert(html);
  const ConvertResult split = convert(html, 0, 1);
  TEST_ASSERT_TRUE(whole.ok);
  TEST_ASSERT_TRUE(split.ok);
  TEST_ASSERT_EQUAL_STRING(whole.out.c_str(), split.out.c_str());
  TEST_ASSERT_EQUAL_UINT32(whole.words, split.words);
}

void test_word_limit_stops_output(void) {
  const ConvertResult r = convert("<p>alpha beta gamma delta</p>", 3);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.limit);
  TEST_ASSERT_EQUAL_UINT32(3, r.words);
  TEST_ASSERT_EQUAL_STRING("alpha beta gamma\r\n", r.out.c_str());
}

void test_word_limit_during_buffered_flush_reports_limit(void) {
  // >220 chars of unbroken text forces the mid-line word-aligned flush; the
  // limit hit inside it must still read as a word-limit stop, not a failure.
  String html = "<p>";
  for (int i = 0; i < 60; ++i) {
    html += "word ";
  }
  html += "</p>";
  const ConvertResult r = convert(html.c_str(), 2);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_TRUE(r.limit);
  TEST_ASSERT_EQUAL_UINT32(2, r.words);
  TEST_ASSERT_EQUAL_STRING("word word\r\n", r.out.c_str());
}

void test_directive_lookalike_line_is_escaped(void) {
  const ConvertResult r = convert("<p>@chapter fake</p>");
  TEST_ASSERT_EQUAL_STRING("@@chapter fake\r\n", r.out.c_str());
}

void test_ellipsis_attaches_to_previous_word(void) {
  const ConvertResult r = convert("<p>wait... what</p>");
  TEST_ASSERT_EQUAL_STRING("wait... what\r\n", r.out.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, r.words);
}

void test_hyphen_handling(void) {
  const ConvertResult inlineHyphen = convert("<p>well-known fact</p>");
  TEST_ASSERT_EQUAL_STRING("well-known fact\r\n", inlineHyphen.out.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, inlineHyphen.words);

  const ConvertResult dash = convert("<p>one -- two</p>");
  TEST_ASSERT_EQUAL_STRING("one - two\r\n", dash.out.c_str());
}

void test_output_wraps_before_96_columns(void) {
  String a;
  String b;
  for (int i = 0; i < 50; ++i) {
    a += 'a';
    b += 'b';
  }
  const String html = String("<p>") + a + " " + b + "</p>";
  const ConvertResult r = convert(html.c_str());
  const String expected = a + "\r\n" + b + "\r\n";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), r.out.c_str());
}

void test_write_rsvp_header(void) {
  String out;
  auto sink = [&out](const char *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
      out += data[i];
    }
  };

  epubconvert::writeRsvpHeader(sink, "test-v1", "My Book", "Jane", "/books/x.epub");
  TEST_ASSERT_EQUAL_STRING(
      "@rsvp 1\r\n@converter test-v1\r\n@title My Book\r\n@author Jane\r\n"
      "@source /books/x.epub\r\n\r\n",
      out.c_str());

  out = "";
  epubconvert::writeRsvpHeader(sink, "test-v1", "My Book", "", "/books/x.epub");
  TEST_ASSERT_TRUE(out.indexOf("@author") < 0);
}

void test_container_rootfile_path(void) {
  const String container =
      "<?xml version=\"1.0\"?><container><rootfiles>"
      "<rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>"
      "</rootfiles></container>";
  TEST_ASSERT_EQUAL_STRING("OEBPS/content.opf",
                           epubconvert::parseRootfilePath(container).c_str());
  TEST_ASSERT_EQUAL_STRING("", epubconvert::parseRootfilePath("<container/>").c_str());
}

void test_opf_metadata_manifest_and_spine(void) {
  const String opf =
      "<package><metadata>"
      "<dc:title>Tom &amp; Jerry</dc:title>"
      "<dc:creator>Jane <span>Q</span> Doe</dc:creator>"
      "</metadata><manifest>"
      "<item id=\"ch1\" href=\"text/ch1.xhtml\" media-type=\"application/xhtml+xml\"/>"
      "<item id=\"css\" href=\"styles.css\" media-type=\"text/css\"/>"
      "<item id=\"ch2\" href=\"text/ch%202.xhtml\" media-type=\"application/xhtml+xml\"/>"
      "<itemref-not-an-item/>"
      "</manifest><spine>"
      "<itemref idref=\"ch2\"/><itemref idref=\"ch1\"/>"
      "</spine></package>";

  TEST_ASSERT_EQUAL_STRING("Tom & Jerry", epubconvert::parseBookTitle(opf).c_str());
  TEST_ASSERT_EQUAL_STRING("Jane Q Doe", epubconvert::parseBookAuthor(opf).c_str());

  const std::vector<epubconvert::ManifestItem> manifest =
      epubconvert::parseManifestItems(opf, "OEBPS/");
  TEST_ASSERT_EQUAL_UINT32(3, manifest.size());
  TEST_ASSERT_EQUAL_STRING("OEBPS/text/ch1.xhtml", manifest[0].path.c_str());
  TEST_ASSERT_EQUAL_STRING("OEBPS/text/ch 2.xhtml", manifest[2].path.c_str());

  const std::vector<String> spine = epubconvert::parseSpineIds(opf);
  TEST_ASSERT_EQUAL_UINT32(2, spine.size());
  TEST_ASSERT_EQUAL_STRING("ch2", spine[0].c_str());

  const epubconvert::ManifestItem *item = epubconvert::findManifestItem(manifest, "ch1");
  TEST_ASSERT_NOT_NULL(item);
  TEST_ASSERT_TRUE(epubconvert::isContentDocument(*item));
  const epubconvert::ManifestItem *css = epubconvert::findManifestItem(manifest, "css");
  TEST_ASSERT_NOT_NULL(css);
  TEST_ASSERT_FALSE(epubconvert::isContentDocument(*css));
  TEST_ASSERT_NULL(epubconvert::findManifestItem(manifest, "nope"));
}

void test_zip_path_helpers(void) {
  TEST_ASSERT_EQUAL_STRING("My Book%2x",
                           epubconvert::percentDecodePath("My%20Book%2x").c_str());
  TEST_ASSERT_EQUAL_STRING("a/c", epubconvert::collapseZipPath("a/./b/../c").c_str());
  TEST_ASSERT_EQUAL_STRING("OEBPS/",
                           epubconvert::directoryForPath("OEBPS/content.opf").c_str());
  TEST_ASSERT_EQUAL_STRING("", epubconvert::directoryForPath("content.opf").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "images/x.xhtml",
      epubconvert::resolveZipPath("OEBPS/", "../images/x.xhtml#frag?q=1").c_str());
  TEST_ASSERT_EQUAL_STRING("root/x.html",
                           epubconvert::resolveZipPath("OEBPS/", "/root/x.html").c_str());
  TEST_ASSERT_EQUAL_STRING("a/b", epubconvert::normalizeZipName("//a\\b").c_str());
  TEST_ASSERT_EQUAL_STRING("My.Novel",
                           epubconvert::basenameWithoutExtension("/books/My.Novel.epub").c_str());
  TEST_ASSERT_EQUAL_STRING("Untitled", epubconvert::basenameWithoutExtension("/books/").c_str());
}

void test_attribute_value(void) {
  TEST_ASSERT_EQUAL_STRING("2",
                           epubconvert::attributeValue("<item xid=\"1\" id=\"2\">", "id").c_str());
  TEST_ASSERT_EQUAL_STRING("v", epubconvert::attributeValue("<a href='v'>", "href").c_str());
  TEST_ASSERT_EQUAL_STRING("bare",
                           epubconvert::attributeValue("<a href=bare next=1>", "href").c_str());
  TEST_ASSERT_EQUAL_STRING("", epubconvert::attributeValue("<a>", "href").c_str());
}

void test_reached_word_limit_zero_is_unlimited(void) {
  TEST_ASSERT_FALSE(epubconvert::reachedWordLimit(1000000, 0));
  TEST_ASSERT_FALSE(epubconvert::reachedWordLimit(2, 3));
  TEST_ASSERT_TRUE(epubconvert::reachedWordLimit(3, 3));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_paragraphs_become_lines);
  RUN_TEST(test_heading_becomes_chapter_marker_once);
  RUN_TEST(test_skip_tags_dropped_including_nested);
  RUN_TEST(test_entities_decoded);
  RUN_TEST(test_comments_dropped);
  RUN_TEST(test_byte_at_a_time_matches_single_write);
  RUN_TEST(test_word_limit_stops_output);
  RUN_TEST(test_word_limit_during_buffered_flush_reports_limit);
  RUN_TEST(test_directive_lookalike_line_is_escaped);
  RUN_TEST(test_ellipsis_attaches_to_previous_word);
  RUN_TEST(test_hyphen_handling);
  RUN_TEST(test_output_wraps_before_96_columns);
  RUN_TEST(test_write_rsvp_header);
  RUN_TEST(test_container_rootfile_path);
  RUN_TEST(test_opf_metadata_manifest_and_spine);
  RUN_TEST(test_zip_path_helpers);
  RUN_TEST(test_attribute_value);
  RUN_TEST(test_reached_word_limit_zero_is_unlimited);
  return UNITY_END();
}
