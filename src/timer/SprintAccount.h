#pragma once

#include <cstddef>
#include <cstdint>

// Pure word-accounting for a "reading sprint": the words a reader advances
// while a Focus-Timer work block is running and the reader is Playing.
//
// The device has no separate notion of sprint words -- it reuses the same
// "words advanced while Playing, scrub excluded" rule as reading stats (see
// App::endStatsSession). This module isolates that arithmetic so it stays
// host-testable and decrement-safe, with no Arduino / reader dependencies.
//
// Model: a sprint runs across one work block. While the reader is Playing it is
// in a "segment"; the caller opens a segment with the current word index when
// Playing begins and closes it with the current word index when Playing ends
// (pause, menu, standby, etc.). Each segment contributes only its net-forward
// progress, so scrubbing backwards never subtracts from -- nor, on the next
// forward pass, double-counts before -- the segment's own start.
namespace sprint {

class SprintAccount {
 public:
  // Begin a fresh sprint for a work block. Clears any prior accounting and, if
  // the reader is already Playing, opens the first segment at `wordIndex`.
  // `playing` lets the caller start a sprint mid-read.
  void beginBlock(size_t wordIndex, bool playing);

  // The reader entered Playing: open a segment at the current word index. A
  // no-op if a segment is already open or no block is active.
  void enterPlaying(size_t wordIndex);

  // The reader left Playing: fold this segment's net-forward words into the
  // running total. A no-op if no segment is open.
  void leavePlaying(size_t wordIndex);

  // Finish the block. If a segment is still open (block completed while the
  // reader was Playing), close it at `wordIndex` first. Returns total words
  // read during the block. Idempotent: the running total is preserved so a
  // second call (e.g. defensive) returns the same value.
  uint32_t finishBlock(size_t wordIndex);

  // Words folded so far (excludes any still-open segment).
  uint32_t wordsRead() const { return wordsRead_; }

  bool blockActive() const { return blockActive_; }
  bool segmentOpen() const { return segmentOpen_; }

 private:
  uint32_t wordsRead_ = 0;
  size_t segmentStartIndex_ = 0;
  bool blockActive_ = false;
  bool segmentOpen_ = false;
};

// Net-forward words between a segment's start and end index. Decrement-safe:
// scrub-back (end < start) contributes zero, never a negative.
uint32_t netForwardWords(size_t startIndex, size_t endIndex);

}  // namespace sprint
