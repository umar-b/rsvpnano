#include "timer/Orientation.h"

#include <math.h>

namespace orientation {
namespace {

constexpr uint32_t kStableMs = 700;
constexpr float kSideAxisThreshold = 0.78f;
constexpr float kCrossAxisLimit = 0.42f;
constexpr float kFlatAxisThreshold = 0.84f;

}  // namespace

Side classify(float x, float y, float z) {
  if (fabsf(z) >= kFlatAxisThreshold && fabsf(x) <= 0.30f && fabsf(y) <= 0.30f) {
    return Side::FlatBack;
  }

  if (x >= kSideAxisThreshold && fabsf(y) <= kCrossAxisLimit && fabsf(z) <= kCrossAxisLimit) {
    return Side::ShortSideA;
  }

  if (x <= -kSideAxisThreshold && fabsf(y) <= kCrossAxisLimit && fabsf(z) <= kCrossAxisLimit) {
    return Side::ShortSideB;
  }

  if (fabsf(y) >= kSideAxisThreshold && fabsf(x) <= kCrossAxisLimit && fabsf(z) <= kCrossAxisLimit) {
    return Side::LongSide;
  }

  return Side::Unknown;
}

void Stabilizer::reset() {
  raw_ = Side::Unknown;
  candidate_ = Side::Unknown;
  stable_ = Side::Unknown;
  candidateSinceMs_ = 0;
}

void Stabilizer::markUnavailable() {
  raw_ = Side::Unknown;
  stable_ = Side::Unknown;
}

void Stabilizer::update(uint32_t nowMs, Side raw) {
  raw_ = raw;
  if (raw_ != candidate_) {
    candidate_ = raw_;
    candidateSinceMs_ = nowMs;
    return;
  }

  if ((nowMs - candidateSinceMs_) >= kStableMs) {
    stable_ = candidate_;
  }
}

}  // namespace orientation
