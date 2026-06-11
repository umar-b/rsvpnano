#pragma once

#include <cstdint>

// Classic "DVD logo" bounce: a label travels diagonally and reflects off the
// edges of a box, flashing brighter for one frame when it strikes a corner.
// Pure (no Arduino, no display, no text): operates on an abstract box of size
// (width x height) within which a sprite of size (spriteWidth x spriteHeight)
// moves. Coordinates and velocities are integers in the same unit as the box
// (the App uses grid cells). Host-testable: reflection and corner detection are
// the whole behaviour.
namespace standby {

class DvdBounce {
 public:
  // The bounding box and the sprite's footprint inside it. The sprite's top-left
  // is kept within [0, width - spriteWidth] x [0, height - spriteHeight].
  DvdBounce(int16_t width, int16_t height, int16_t spriteWidth, int16_t spriteHeight);

  // (Re)initialize position and velocity from a seed. Deterministic.
  void seed(uint32_t rngSeed);

  // Advance one frame: move by the velocity, reflecting off any wall hit. Sets
  // the one-frame corner-flash flag when both axes reflect on the same step.
  void step();

  int16_t x() const { return x_; }
  int16_t y() const { return y_; }
  // True for exactly the frame on which a corner was struck (two simultaneous
  // reflections). Cleared on the next step.
  bool cornerFlash() const { return cornerFlash_; }
  uint32_t generation() const { return generation_; }

  // Exposed for the renderer/tests: how far the sprite may travel on each axis.
  int16_t maxX() const { return width_ - spriteWidth_; }
  int16_t maxY() const { return height_ - spriteHeight_; }

 private:
  int16_t width_;
  int16_t height_;
  int16_t spriteWidth_;
  int16_t spriteHeight_;
  int16_t x_ = 0;
  int16_t y_ = 0;
  int16_t vx_ = 1;
  int16_t vy_ = 1;
  bool cornerFlash_ = false;
  uint32_t rng_ = 1;
  uint32_t generation_ = 0;
};

}  // namespace standby
