#pragma once

#include <cstddef>
#include <cstdint>

// Pure note-sequence model for the speaker. AudioManager owns the I2S/codec
// path; this module owns *what* to play and the sample math behind it -- so the
// math (sample counts, envelope bounds, total sequence duration) stays
// host-testable with no Arduino / driver dependencies.
//
// A Jingle is a short, bounded sequence of tones. Each Note is a sine tone of a
// given frequency and duration, optionally followed by a silent gap. The whole
// sequence is capped (see kMaxSequenceDurationMs) so a stray definition can't
// hold the audio path for an unbounded time.
namespace jingle {

// One tone plus a trailing silent gap.
struct Note {
  uint16_t frequencyHz;  // 0 = rest (silence for durationMs, then gap)
  uint16_t durationMs;   // tone length
  uint16_t gapMs;        // trailing silence before the next note
};

// A named, fixed-size sequence. Sequences are small (<= kMaxNotes) so they can
// be stored as plain value types / constants with no heap allocation.
constexpr size_t kMaxNotes = 6;

struct Sequence {
  Note notes[kMaxNotes];
  size_t count;
};

// The codec runs at this fixed rate (matches AudioManager::kSampleRateHz). The
// jingle math is parameterised on it so tests don't depend on Arduino headers.
constexpr uint32_t kSampleRateHz = 16000;

// Output is stereo 16-bit: two int16 samples (L, R) per frame.
constexpr size_t kChannels = 2;

// Peak amplitude for tone samples (matches the legacy beep amplitude so volume
// behaviour is unchanged).
constexpr int16_t kAmplitude = 12000;

// Per-note attack/release envelope, in milliseconds, to avoid clicks. Applied
// inside each tone's duration (so a tone shorter than attack+release simply
// ramps up then down without a sustain plateau).
constexpr uint32_t kEnvelopeAttackMs = 4;
constexpr uint32_t kEnvelopeReleaseMs = 8;

// Hard upper bound on a whole sequence's audible+gap duration. Definitions that
// would exceed this are rejected by isWithinBudget(); the device should treat
// such a sequence as a programming error, not play it.
constexpr uint32_t kMaxSequenceDurationMs = 1000;

// --- Named jingles ---------------------------------------------------------
// CompletionArpeggio: 4 ascending notes -- focus-block / reading-sprint done.
Sequence completionArpeggio();
// ChapterChime: a soft two-note cue at a chapter boundary.
Sequence chapterChime();
// BookFanfare: a short triumphant 5-note run when a book is finished.
Sequence bookFanfare();
// AchievementPing: reserved for a later feature; defined now so the catalogue
// is complete. A single bright note.
Sequence achievementPing();

// --- Sample math (pure) ----------------------------------------------------

// Frames (one frame == one L/R pair) for a millisecond span at kSampleRateHz.
size_t framesForMs(uint32_t ms);

// int16 samples emitted for a millisecond span (frames * kChannels).
size_t samplesForMs(uint32_t ms);

// Total tone+gap duration of a sequence in milliseconds.
uint32_t sequenceDurationMs(const Sequence &seq);

// Total stereo int16 samples a sequence renders to.
size_t sequenceSampleCount(const Sequence &seq);

// True when the sequence fits within kMaxSequenceDurationMs and kMaxNotes.
bool isWithinBudget(const Sequence &seq);

// Envelope scale (0..1, fixed-point per 1024) for a frame within a tone of
// `toneFrames` total frames. Ramps up over the attack window, down over the
// release window, full-scale in between. Returns a value in [0, 1024].
int32_t envelopeScaleQ10(size_t frameInTone, size_t toneFrames);

// Render one note's tone into a caller-provided stereo buffer starting at
// `frameOffset` frames. Writes the gap as silence too. Returns the number of
// frames written (tone + gap). Writes nothing past `capacityFrames`.
// `buffer` is interleaved L/R int16 with `capacityFrames * kChannels` entries.
size_t renderNote(const Note &note, int16_t *buffer, size_t frameOffset,
                  size_t capacityFrames);

// Render a whole sequence into `buffer`. Returns frames written (== clamped
// sequenceFrameCount). Stops at capacity.
size_t renderSequence(const Sequence &seq, int16_t *buffer, size_t capacityFrames);

}  // namespace jingle
