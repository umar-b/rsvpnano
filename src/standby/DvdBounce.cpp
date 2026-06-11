#include "standby/DvdBounce.h"

#include <algorithm>

#include "standby/LifeGrid.h"  // advanceRng

namespace standby {

DvdBounce::DvdBounce(int16_t width, int16_t height, int16_t spriteWidth, int16_t spriteHeight)
    : width_(width),
      height_(height),
      spriteWidth_(std::min<int16_t>(spriteWidth, width)),
      spriteHeight_(std::min<int16_t>(spriteHeight, height)) {}

void DvdBounce::seed(uint32_t rngSeed) {
  rng_ = rngSeed;
  generation_ = 0;
  cornerFlash_ = false;

  const int16_t mx = std::max<int16_t>(0, maxX());
  const int16_t my = std::max<int16_t>(0, maxY());
  x_ = mx == 0 ? 0 : static_cast<int16_t>((advanceRng(rng_) >> 16) % (mx + 1));
  y_ = my == 0 ? 0 : static_cast<int16_t>((advanceRng(rng_) >> 16) % (my + 1));
  // Speed 1..3 cells/step on each axis, random initial sign.
  const int16_t sx = static_cast<int16_t>(1 + ((advanceRng(rng_) >> 24) % 3));
  const int16_t sy = static_cast<int16_t>(1 + ((advanceRng(rng_) >> 24) % 3));
  vx_ = (advanceRng(rng_) & 1U) ? sx : static_cast<int16_t>(-sx);
  vy_ = (advanceRng(rng_) & 1U) ? sy : static_cast<int16_t>(-sy);
}

void DvdBounce::step() {
  const int16_t mx = std::max<int16_t>(0, maxX());
  const int16_t my = std::max<int16_t>(0, maxY());

  int nextX = x_ + vx_;
  int nextY = y_ + vy_;
  bool hitX = false;
  bool hitY = false;

  if (nextX <= 0) {
    nextX = 0;
    vx_ = static_cast<int16_t>(-vx_);
    hitX = true;
  } else if (nextX >= mx) {
    nextX = mx;
    vx_ = static_cast<int16_t>(-vx_);
    hitX = true;
  }

  if (nextY <= 0) {
    nextY = 0;
    vy_ = static_cast<int16_t>(-vy_);
    hitY = true;
  } else if (nextY >= my) {
    nextY = my;
    vy_ = static_cast<int16_t>(-vy_);
    hitY = true;
  }

  x_ = static_cast<int16_t>(nextX);
  y_ = static_cast<int16_t>(nextY);
  // A corner is both walls reflecting on the same step.
  cornerFlash_ = hitX && hitY;
  ++generation_;
}

}  // namespace standby
