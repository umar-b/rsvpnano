#pragma once

#include <stdint.h>

// Ramp-in pacing helper for the RSVP reader. Every transition into Playing
// (resume after a pause, a fresh start, a scrub-then-play) feels gentler if the
// first words flash a little slower than the set WPM and then accelerate to the
// full speed. This is pure timing geometry: given how many words have been
// shown since playback resumed, it returns the factor to stretch the base
// word interval by. No state, no IO -- safe to unit test on the host.
//
// The reader counts whole words advanced since the last resume and asks this
// module to slow the base interval. The ramp starts at kStartPercent of full
// speed (so a longer interval) and climbs linearly to 100% over kRampWords
// words, after which it is a no-op.
namespace rampin {

// Word at which the ramp finishes: from this many words past the resume point
// onward the base interval is unscaled.
constexpr uint8_t kRampWords = 12;

// Speed at the very first word after a resume, as a percent of the set WPM.
// 60% speed means the first interval is stretched to ~167% of its length.
constexpr uint8_t kStartPercent = 60;

// Interval scale (in percent) for the word `wordsSinceResume` words past the
// resume point, when ramp-in is enabled. Word 0 returns the slowest scale
// (100 * 100 / kStartPercent, i.e. the interval is longest); the scale falls
// linearly to 100 at and after kRampWords. Returns 100 when disabled.
//
// Scaling the *interval* (not the WPM) keeps the math integer-friendly for the
// reader: durationMs = baseIntervalMs * scalePercent / 100 + pacingBonus.
uint16_t intervalScalePercent(uint32_t wordsSinceResume, bool enabled);

}  // namespace rampin
