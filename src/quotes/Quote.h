#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Pure starred-sentence logic, shared by the device reader (App), the on-device
// Starred picker, and the web companion. None of this touches Arduino I/O, the
// SD card, or the ReadingLoop, so it is host-testable like booktext/wordpacing.
//
// A starred sentence is a "quote": the full sentence containing a word, the book
// it came from, and the word index it starts at. Quotes are persisted as JSON
// lines in /config/quotes.jsonl (one object per line); SD is authoritative and
// there are no NVS copies. The SD read/write wrapper lives in QuoteStore (which
// depends on this module), keeping the encode / parse / dedupe parts pure here.
namespace quotes {

// One starred sentence. wordIndex is the index of the FIRST word of the
// sentence (the sentence start), so re-starring the same sentence from any word
// inside it dedupes to one record and jumping lands at the sentence start.
struct Quote {
  String bookPath;
  String bookTitle;
  uint32_t wordIndex = 0;
  String sentence;
};

// Callback giving the word at an index, and whether that word ends a sentence.
// App supplies these from ReadingLoop so the sentence boundary convention is
// identical to flick-rewind / previous-sentence; the host tests supply vectors.
using WordAtFn = std::function<String(size_t)>;
using EndsSentenceFn = std::function<bool(size_t)>;

// Result of locating the sentence around a word: its start index and the joined
// sentence text. found is false only for an empty book.
struct SentenceSpan {
  bool found = false;
  size_t startIndex = 0;
  size_t endIndex = 0;  // inclusive
  String text;
};

// Walk back to the sentence start (the word after the previous sentence end)
// and forward to the sentence end (the first word that ends a sentence at or
// after the current word), then join [start..end] with single spaces. Mirrors
// ReadingLoop::sentenceStartAtOrBefore + wordEndsSentenceAt.
SentenceSpan extractSentence(size_t currentIndex, size_t wordCount,
                             const WordAtFn &wordAt, const EndsSentenceFn &endsSentence);

// Encode a quote as one JSON object (no trailing newline). Keys:
// {"bookPath","bookTitle","wordIndex","sentence"}. Strings are JSON-escaped.
String encodeJsonLine(const Quote &quote);

// Parse one JSON-lines record back into a quote. Tolerant hand parser (our own
// file): returns false on a blank line or when bookPath/sentence are missing.
bool parseJsonLine(const String &line, Quote &quote);

// True when records already hold a quote for this book path + sentence-start
// word index. Starring an already-starred sentence is a no-op.
bool containsQuote(const std::vector<Quote> &records, const String &bookPath,
                   uint32_t wordIndex);

// A short, human label for a quote in a list: a title-cased book name followed
// by a clipped sentence prefix. maxSentenceChars clips the sentence (adding an
// ellipsis) so list rows stay one line.
String quoteListLabel(const Quote &quote, size_t maxSentenceChars = 48);

// Title-case a book title/name for display: first letter of each word upper,
// the rest lower. Leaves digits and punctuation alone. Pure ASCII fold.
String titleCase(const String &text);

}  // namespace quotes
