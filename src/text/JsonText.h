#pragma once

#include <Arduino.h>
#include <stdint.h>

// The device's one hand-rolled JSON codec: key lookup + scalar reads for the
// Companion sync request bodies, and string extraction with escape handling
// for GitHub release payloads. CompanionSyncManager and releaseparser each
// carried their own copy of this before; the escape/unescape rules now live
// here in one tested place. Flat-object scanning by key, not a document
// parser -- nesting is the caller's problem, which no current payload has.
// (quotes:: keeps its own deliberately lossier escaper: it drops control
// chars so its JSONL lines always roundtrip through its own reader.)
namespace jsontext {

// Escapes a value for embedding inside a JSON string literal. Control chars
// become \uXXXX.
String escape(const String &value);
// Decodes backslash escapes (\" \\ \/ \b \f \n \r \t; unknown escapes keep
// the escaped char). \uXXXX is not decoded -- no device payload sends it.
String unescape(const String &raw);

int skipWhitespace(const String &body, int index);
// Index of the ':' following the first "key" occurrence at or past `from`;
// -1 when absent. keyIndex receives where the quoted key starts.
int findKeyColon(const String &body, const char *key, size_t from = 0, int *keyIndex = nullptr);

bool readInt(const String &body, const char *key, int &value);
// For values that overflow int (e.g. epoch milliseconds).
bool readInt64(const String &body, const char *key, int64_t &value);
bool readBool(const String &body, const char *key, bool &value);
bool readString(const String &body, const char *key, String &value);
// Scanning variant for arrays of objects (e.g. GitHub release assets):
// starts at `from`, reports where the key was found via keyIndex.
bool readStringFrom(const String &body, const char *key, size_t from, String &value,
                    int *keyIndex = nullptr);
// Decodes the string literal opening at body[quoteIndex] == '"'.
// closingQuote receives the index of the terminating quote.
bool parseStringAt(const String &body, int quoteIndex, String &value,
                   int *closingQuote = nullptr);
// Iterates the string items of a JSON array. index enters past '[' and on
// success advances past the item's closing quote. Returns false at ']', on
// a non-string item, or at end of input.
bool nextArrayString(const String &body, int &index, String &value);

}  // namespace jsontext
