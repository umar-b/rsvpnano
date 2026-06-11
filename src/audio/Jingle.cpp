#include "audio/Jingle.h"

#include <cmath>

namespace jingle {

namespace {

// A square-ish tone built from a sine, so the math is deterministic and
// host-testable. We synthesise a true sine; the small amplitude and short
// envelope keep it click-free.
int16_t sineSample(uint32_t frequencyHz, size_t frameIndex, int32_t scaleQ10) {
  if (frequencyHz == 0) {
    return 0;
  }
  const double phase = (2.0 * M_PI * static_cast<double>(frequencyHz) *
                        static_cast<double>(frameIndex)) /
                       static_cast<double>(kSampleRateHz);
  const double value = std::sin(phase) * static_cast<double>(kAmplitude);
  const int32_t scaled = static_cast<int32_t>(value) * scaleQ10 / 1024;
  if (scaled > 32767) {
    return 32767;
  }
  if (scaled < -32768) {
    return -32768;
  }
  return static_cast<int16_t>(scaled);
}

}  // namespace

Sequence completionArpeggio() {
  // C5, E5, G5, C6 -- bright, clearly "done".
  Sequence seq{};
  seq.count = 4;
  seq.notes[0] = {523, 90, 20};
  seq.notes[1] = {659, 90, 20};
  seq.notes[2] = {784, 90, 20};
  seq.notes[3] = {1047, 150, 0};
  return seq;
}

Sequence chapterChime() {
  // Soft two-note: G5 -> C6, gentle.
  Sequence seq{};
  seq.count = 2;
  seq.notes[0] = {784, 80, 30};
  seq.notes[1] = {1047, 130, 0};
  return seq;
}

Sequence bookFanfare() {
  // Triumphant 5-note run: C5 E5 G5 C6 G5.
  Sequence seq{};
  seq.count = 5;
  seq.notes[0] = {523, 80, 15};
  seq.notes[1] = {659, 80, 15};
  seq.notes[2] = {784, 80, 15};
  seq.notes[3] = {1047, 140, 25};
  seq.notes[4] = {784, 160, 0};
  return seq;
}

Sequence achievementPing() {
  // Single bright note (reserved for a later achievement feature).
  Sequence seq{};
  seq.count = 1;
  seq.notes[0] = {1319, 140, 0};
  return seq;
}

size_t framesForMs(uint32_t ms) {
  return (static_cast<size_t>(kSampleRateHz) * static_cast<size_t>(ms)) / 1000U;
}

size_t samplesForMs(uint32_t ms) { return framesForMs(ms) * kChannels; }

uint32_t sequenceDurationMs(const Sequence &seq) {
  uint32_t total = 0;
  const size_t count = seq.count > kMaxNotes ? kMaxNotes : seq.count;
  for (size_t i = 0; i < count; ++i) {
    total += seq.notes[i].durationMs;
    total += seq.notes[i].gapMs;
  }
  return total;
}

size_t sequenceSampleCount(const Sequence &seq) {
  size_t frames = 0;
  const size_t count = seq.count > kMaxNotes ? kMaxNotes : seq.count;
  for (size_t i = 0; i < count; ++i) {
    frames += framesForMs(seq.notes[i].durationMs);
    frames += framesForMs(seq.notes[i].gapMs);
  }
  return frames * kChannels;
}

bool isWithinBudget(const Sequence &seq) {
  if (seq.count == 0 || seq.count > kMaxNotes) {
    return false;
  }
  return sequenceDurationMs(seq) <= kMaxSequenceDurationMs;
}

int32_t envelopeScaleQ10(size_t frameInTone, size_t toneFrames) {
  if (toneFrames == 0) {
    return 0;
  }
  const size_t attackFrames = framesForMs(kEnvelopeAttackMs);
  const size_t releaseFrames = framesForMs(kEnvelopeReleaseMs);

  // Attack ramp.
  if (attackFrames > 0 && frameInTone < attackFrames) {
    return static_cast<int32_t>((frameInTone * 1024U) / attackFrames);
  }
  // Release ramp (only when the tone is long enough to host one).
  if (releaseFrames > 0 && toneFrames > releaseFrames &&
      frameInTone >= (toneFrames - releaseFrames)) {
    const size_t remaining = toneFrames - frameInTone;
    return static_cast<int32_t>((remaining * 1024U) / releaseFrames);
  }
  return 1024;
}

size_t renderNote(const Note &note, int16_t *buffer, size_t frameOffset,
                  size_t capacityFrames) {
  const size_t toneFrames = framesForMs(note.durationMs);
  const size_t gapFrames = framesForMs(note.gapMs);

  for (size_t f = 0; f < toneFrames; ++f) {
    const size_t frame = frameOffset + f;
    if (frame >= capacityFrames) {
      return frame - frameOffset;
    }
    const int32_t scale = envelopeScaleQ10(f, toneFrames);
    const int16_t sample = sineSample(note.frequencyHz, f, scale);
    buffer[frame * kChannels] = sample;
    buffer[frame * kChannels + 1] = sample;
  }

  for (size_t g = 0; g < gapFrames; ++g) {
    const size_t frame = frameOffset + toneFrames + g;
    if (frame >= capacityFrames) {
      return frame - frameOffset;
    }
    buffer[frame * kChannels] = 0;
    buffer[frame * kChannels + 1] = 0;
  }

  return toneFrames + gapFrames;
}

size_t renderSequence(const Sequence &seq, int16_t *buffer, size_t capacityFrames) {
  size_t frameOffset = 0;
  const size_t count = seq.count > kMaxNotes ? kMaxNotes : seq.count;
  for (size_t i = 0; i < count; ++i) {
    if (frameOffset >= capacityFrames) {
      break;
    }
    frameOffset += renderNote(seq.notes[i], buffer, frameOffset, capacityFrames);
  }
  return frameOffset;
}

}  // namespace jingle
