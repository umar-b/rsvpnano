#include "reader/RampIn.h"

namespace rampin {

uint16_t intervalScalePercent(uint32_t wordsSinceResume, bool enabled) {
  if (!enabled || wordsSinceResume >= kRampWords) {
    return 100;
  }

  // Effective speed as a percent of full WPM, ramping linearly from
  // kStartPercent at word 0 to 100 at word kRampWords.
  const uint32_t speedPercent =
      static_cast<uint32_t>(kStartPercent) +
      (static_cast<uint32_t>(100 - kStartPercent) * wordsSinceResume) / kRampWords;

  // Interval is inversely proportional to speed: a 60%-speed word lingers
  // 100/60 as long. Round to nearest to avoid a systematic downward bias.
  const uint32_t scale = (100UL * 100UL + speedPercent / 2) / speedPercent;
  return static_cast<uint16_t>(scale);
}

}  // namespace rampin
