#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <functional>
#include <vector>

// Pure context-preview logic for the RSVP reader: the transient multi-word
// view shown around the current word during a scrub or browse, and the
// phantom before/after text drawn beside the RSVP word. Owns the windowing
// decisions -- where the window anchors (paragraph-snapped), when it
// rebuilds versus just moving the current-word highlight, and how many
// words fit a character budget. Extracted from App, where this logic was
// spread over eight methods reading the Reader directly. No IO, no
// hardware -- host-testable with the Arduino String shim.
namespace contextpreview {

// One word of the preview window, as the display draws it.
struct Word {
  String text;
  bool paragraphStart = false;
  bool current = false;
};

struct Config {
  // Words held in the preview window.
  size_t windowWords = 288;
  // How many words of lead-in the anchor sits behind the current word.
  size_t anchorLeadWords = 112;
  // Snap the anchor back to a paragraph start at most this many words away.
  size_t maxParagraphSnapWords = 48;
};

// Word text provider, index -> word.
using WordAt = std::function<String(size_t)>;

// True when wordIndex begins a paragraph. Index 0 always does.
// paragraphStarts must be sorted ascending.
bool isParagraphStart(const std::vector<size_t> &paragraphStarts, size_t wordIndex);

// The nearest paragraph start at or before wordIndex (0 when none).
size_t paragraphStartAtOrBefore(const std::vector<size_t> &paragraphStarts, size_t wordIndex);

// Where the preview window should start for currentIndex: anchorLeadWords
// back, snapped to a nearby paragraph start.
size_t anchorIndex(size_t currentIndex, const std::vector<size_t> &paragraphStarts,
                   const Config &config);

// Space-joined words before currentIndex filling roughly charTarget chars.
String collectBefore(size_t currentIndex, size_t charTarget, const WordAt &wordAt);

// Space-joined words after currentIndex filling roughly charTarget chars.
String collectAfter(size_t currentIndex, size_t wordCount, size_t charTarget,
                    const WordAt &wordAt);

// The preview window itself. update() rebuilds the word list only when the
// current word leaves (or reaches the trailing edge of) the window;
// otherwise it just moves the current-word highlight.
class Window {
 public:
  void invalidate();
  void update(size_t currentIndex, size_t wordCount,
              const std::vector<size_t> &paragraphStarts, const WordAt &wordAt,
              const Config &config = Config());

  const std::vector<Word> &words() const { return words_; }
  size_t startIndex() const { return startIndex_; }

 private:
  std::vector<Word> words_;
  size_t startIndex_ = 0;
  size_t currentLocalIndex_ = static_cast<size_t>(-1);
  bool valid_ = false;
};

}  // namespace contextpreview
