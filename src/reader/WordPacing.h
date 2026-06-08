#pragma once

#include <Arduino.h>
#include <stdint.h>

// Pure word-timing heuristics for the RSVP reader. Given a word, the next
// word's leading case, and a pacing configuration, decide how long the word
// should linger on screen. No state, no IO, no hardware -- safe to unit test
// on the host with the Arduino String shim.
//
// The heuristics combine three independent bonuses on top of the base WPM
// interval: a length bonus (longer words read slower), a complexity bonus
// (syllables, mixed alphanumerics, dense connectors), and a punctuation pause
// (commas, clauses, sentence ends -- with abbreviation detection so "Dr." does
// not trigger a full stop). Each bonus is scaled and clamped by PacingConfig.
namespace wordpacing {

struct PacingConfig {
  uint16_t longWordDelayMs = 200;
  uint16_t complexWordDelayMs = 200;
  uint16_t punctuationDelayMs = 200;
  uint8_t longWordScalePercent = 100;
  uint8_t complexWordScalePercent = 100;
  uint8_t punctuationScalePercent = 100;
};

// Clamps each field of the config to the range the heuristics honour
// (delays capped, scales floored), so a stored config matches what timing
// will actually apply.
PacingConfig clampConfig(const PacingConfig &config);

// True if the first letter of the word is lowercase. Used by the reader to
// decide whether a preceding "." is a sentence end or an abbreviation.
bool startsWithLowercaseLetter(const String &word);

// True if the word terminates a sentence: ends in '!' or '?', or in a '.' that
// does not look like an abbreviation given the following word's leading case.
bool wordEndsSentence(const String &word, bool nextWordStartsLowercase);

// Extra milliseconds beyond the base interval that this word should linger.
uint32_t bonusMsForWord(const String &word, bool nextWordStartsLowercase,
                        const PacingConfig &config);

// baseIntervalMs + bonusMsForWord(...). Returns 0 when baseIntervalMs is 0.
uint32_t durationForWord(const String &word, bool nextWordStartsLowercase,
                         uint32_t baseIntervalMs, const PacingConfig &config);

}  // namespace wordpacing
