#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Matrix-style falling-words screensaver logic. Pure (no Arduino, no display):
// the column advancement, recycling, and dim-trail computation are all
// host-testable. The grid is described in cell units (columns x rows, the same
// space the packed-grid savers use); each "drop" is the head of a vertical
// trail of words falling down one column. App samples words from the current
// book and hands them in via setWords(); with no words a small built-in list is
// used so the saver always has something to rain.
namespace standby {

// One word placed in the grid for this frame, with a dim level (0 = brightest
// head, increasing = fainter trail). x/y are in cell units; the head sits at y,
// the trail extends upward (smaller y).
struct RainWord {
  std::string text;
  int16_t x;
  int16_t y;
  uint8_t dim;
};

class WordRain {
 public:
  WordRain(uint16_t columns, uint16_t rows);

  // Replace the word pool. Empty input keeps the built-in fallback list. The
  // pool is sampled (with the saver's own RNG) as drops are spawned/recycled.
  void setWords(const std::vector<std::string> &words);

  // (Re)initialize the falling columns from a seed. Deterministic: the same
  // seed + same word pool yields the same sequence of frames.
  void seed(uint32_t rngSeed);

  // Advance every active column by its speed; recycle a column once its head
  // falls past the bottom (plus its trail), respawning it at the top with a
  // fresh word, speed, and a small random delay.
  void step();

  // The words to draw this frame, computed from the current column state.
  const std::vector<RainWord> &words() const { return rendered_; }

  uint32_t generation() const { return generation_; }

  // Spacing (in cell rows) between stacked words in a column's trail. Public so
  // the renderer and tests agree on the vertical pitch.
  static constexpr int kRowPitch = 9;

  // Maximum number of trailing words drawn behind a head (head + trail).
  static constexpr int kTrailLength = 4;

 private:
  struct Column {
    int16_t x = 0;          // cell x of this column
    int32_t headY16 = 0;    // head position in 1/16 cell rows (fixed point)
    int16_t speed16 = 0;    // fall speed in 1/16 cell rows per step
    int16_t delay = 0;      // frames to wait before falling (0 = falling)
    uint8_t wordIndex = 0;  // index into pool_ for this column's head word
    uint8_t trailWords[kTrailLength] = {0, 0, 0, 0};  // word index per trail slot
  };

  const std::string &poolWord(uint8_t index) const;
  uint8_t pickWordIndex();
  void respawn(Column &column, bool atTop);
  void render();

  uint16_t columns_;
  uint16_t rows_;
  uint32_t rng_ = 1;
  uint32_t generation_ = 0;
  std::vector<std::string> pool_;
  std::vector<Column> activeColumns_;
  std::vector<RainWord> rendered_;
};

}  // namespace standby
