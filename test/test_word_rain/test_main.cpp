#include <unity.h>

#include <set>
#include <string>
#include <vector>

#include "standby/WordRain.h"

namespace {

// Runs a saver for `steps` frames and returns the concatenation of every head
// position seen, so two runs can be compared for determinism.
std::vector<int> headTrace(standby::WordRain &rain, int steps) {
  std::vector<int> trace;
  for (int i = 0; i < steps; ++i) {
    for (const standby::RainWord &w : rain.words()) {
      trace.push_back(w.x);
      trace.push_back(w.y);
      trace.push_back(w.dim);
    }
    rain.step();
  }
  return trace;
}

}  // namespace

void test_seed_produces_words() {
  standby::WordRain rain(120, 120);
  rain.seed(42);
  // With the built-in fallback list there is always something to render.
  TEST_ASSERT_TRUE(rain.words().size() > 0);
}

void test_same_seed_is_deterministic() {
  standby::WordRain a(120, 120);
  standby::WordRain b(120, 120);
  a.seed(7);
  b.seed(7);
  const std::vector<int> ta = headTrace(a, 30);
  const std::vector<int> tb = headTrace(b, 30);
  TEST_ASSERT_EQUAL_UINT32(ta.size(), tb.size());
  for (size_t i = 0; i < ta.size(); ++i) {
    TEST_ASSERT_EQUAL_INT(ta[i], tb[i]);
  }
}

void test_different_seed_diverges() {
  standby::WordRain a(120, 120);
  standby::WordRain b(120, 120);
  a.seed(1);
  b.seed(99999);
  const std::vector<int> ta = headTrace(a, 30);
  const std::vector<int> tb = headTrace(b, 30);
  bool differs = ta.size() != tb.size();
  for (size_t i = 0; i < ta.size() && i < tb.size() && !differs; ++i) {
    differs = ta[i] != tb[i];
  }
  TEST_ASSERT_TRUE(differs);
}

void test_columns_advance_downward() {
  standby::WordRain rain(120, 120);
  rain.seed(3);
  // Run enough frames for heads (which may start above the top and have spawn
  // delays) to be visibly falling, then confirm at least one head descends.
  int prevMaxY = -1000;
  bool sawDescent = false;
  for (int i = 0; i < 200; ++i) {
    int maxY = -1000;
    for (const standby::RainWord &w : rain.words()) {
      if (w.y > maxY) {
        maxY = w.y;
      }
    }
    if (maxY > prevMaxY && prevMaxY != -1000) {
      sawDescent = true;
    }
    prevMaxY = maxY;
    rain.step();
  }
  TEST_ASSERT_TRUE(sawDescent);
}

void test_words_stay_in_bounds() {
  const uint16_t rows = 90;
  const uint16_t cols = 120;
  standby::WordRain rain(cols, rows);
  rain.seed(123);
  for (int i = 0; i < 500; ++i) {
    for (const standby::RainWord &w : rain.words()) {
      TEST_ASSERT_TRUE(w.y >= 0);
      TEST_ASSERT_TRUE(w.y < static_cast<int>(rows));
      TEST_ASSERT_TRUE(w.x >= 0);
      TEST_ASSERT_TRUE(w.x < static_cast<int>(cols));
    }
    rain.step();
  }
}

void test_recycles_after_falling_off() {
  // A long run must keep producing words (columns recycle to the top rather
  // than draining to nothing).
  standby::WordRain rain(120, 120);
  rain.seed(55);
  for (int i = 0; i < 1000; ++i) {
    rain.step();
  }
  TEST_ASSERT_TRUE(rain.words().size() > 0);
}

void test_book_words_are_used() {
  standby::WordRain rain(120, 120);
  std::vector<std::string> pool = {"alpha", "beta", "gamma", "delta"};
  rain.setWords(pool);
  rain.seed(8);
  std::set<std::string> seen;
  for (int i = 0; i < 400; ++i) {
    for (const standby::RainWord &w : rain.words()) {
      seen.insert(w.text);
    }
    rain.step();
  }
  // Every rendered word must come from the supplied pool (no fallback leakage).
  for (const std::string &s : seen) {
    bool inPool = false;
    for (const std::string &p : pool) {
      if (p == s) {
        inPool = true;
      }
    }
    TEST_ASSERT_TRUE(inPool);
  }
  TEST_ASSERT_TRUE(seen.size() > 0);
}

void test_head_is_brightest() {
  standby::WordRain rain(120, 120);
  rain.seed(9);
  // Advance a while so trails exist.
  for (int i = 0; i < 80; ++i) {
    rain.step();
  }
  // The minimum dim value across all rendered words should be 0 (a head).
  bool sawHead = false;
  for (const standby::RainWord &w : rain.words()) {
    if (w.dim == 0) {
      sawHead = true;
    }
  }
  TEST_ASSERT_TRUE(sawHead);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_seed_produces_words);
  RUN_TEST(test_same_seed_is_deterministic);
  RUN_TEST(test_different_seed_diverges);
  RUN_TEST(test_columns_advance_downward);
  RUN_TEST(test_words_stay_in_bounds);
  RUN_TEST(test_recycles_after_falling_off);
  RUN_TEST(test_book_words_are_used);
  RUN_TEST(test_head_is_brightest);
  return UNITY_END();
}
