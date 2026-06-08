#pragma once

#include <Arduino.h>
#include <stdint.h>

// Pure byte/character decoding for imported markup. Turns HTML/XHTML entities
// and raw UTF-8 into the firmware's display character set, then normalises
// whitespace for the reader. No ZIP handling, no file IO, no hardware -- safe
// to unit test on the host with the Arduino String shim.
//
// Extracted from EpubConverter, where this logic was welded to the ZIP and SD
// plumbing; the importer now calls these functions and keeps the IO to itself.
namespace textdecode {

// Decodes a single entity body (the text between '&' and ';', without the
// delimiters) to one display character. Handles the named XML entities, the
// HTML5/Latin-1 names, and numeric forms (&#NNN; / &#xHH;). Returns ' ' when
// nothing better maps.
char decodedEntityChar(const String &entity);

// Like decodedEntityChar but may expand to a short run -- e.g. an em-dash to
// " - " or a horizontal ellipsis to "...".
String decodedEntityText(const String &entity);

// Decodes the UTF-8 sequence starting at text[index]. On success advances
// index past the sequence, writes the code point, and returns true. On an
// invalid/truncated sequence returns false with index already advanced past
// the lead byte (and possibly further) -- callers must restart from their own
// saved offset, as normalizeDisplayText does.
bool decodeUtf8Codepoint(const String &text, size_t &index, uint32_t &codepoint);

// Decodes UTF-8 to the display character set, approximating punctuation that
// has no exact glyph (smart quotes, dashes, ellipsis), then collapses runs of
// whitespace and trims a trailing space.
String normalizeDisplayText(const String &text);

}  // namespace textdecode
