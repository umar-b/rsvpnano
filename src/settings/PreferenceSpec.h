#pragma once

#include <stdint.h>

// Canonical numeric domains for stored device preferences. Before this header,
// the valid range of each setting was written twice -- once where App reads NVS
// and clamps tolerantly, once where CompanionSyncManager parses JSON and rejects
// strictly -- in two files, two naming conventions, and (for the typography
// anchor) two different values. This is the single source of truth: the device
// clamps stored bytes into a range; the companion rejects JSON outside the same
// range. Both ask this table, so the two can never disagree.
//
// Pure, header-only, no Arduino/NVS dependency -- safe to unit test on the host.
// Domains here are stored-value domains, not on-screen effective values: e.g.
// the typography anchor is stored 30..40; DisplayManager's wider post-handedness
// clamp is a separate concern and stays in DisplayManager.
namespace settings {

// An inclusive numeric domain [min, max]. Plain aggregate (no default member
// initializers) so it stays usable under the framework's gnu++11 build.
struct IntRange {
  long min;
  long max;
};

// True when value lies within the domain. Used by the strict (companion) path.
constexpr bool inRange(long value, IntRange range) {
  return value >= range.min && value <= range.max;
}

// value pulled into the domain. Used by the tolerant (device) path.
constexpr long clampToRange(long value, IntRange range) {
  return value < range.min ? range.min : (value > range.max ? range.max : value);
}

// Reader + pacing.
constexpr IntRange kWpmRange{10, 1000};
constexpr IntRange kPacingDelayMsRange{0, 600};

// Audio.
constexpr IntRange kAudioVolumeRange{0, 100};

// Typography (stored-value domains).
constexpr IntRange kTypographyTrackingRange{-2, 3};
constexpr IntRange kTypographyAnchorRange{30, 40};
constexpr IntRange kTypographyGuideWidthRange{12, 30};
constexpr IntRange kTypographyGuideGapRange{2, 8};

// Gesture sensitivity (raw pixels / milliseconds).
constexpr IntRange kGestureSwipePxRange{12, 120};
constexpr IntRange kGestureTapPxRange{8, 80};
constexpr IntRange kGestureScrubPxRange{1, 120};
constexpr IntRange kGestureHoldMsRange{120, 1500};

}  // namespace settings
