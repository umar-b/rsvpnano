#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// A standby screensaver: an automaton that produces a packed-bit cell grid each
// frame. App seeds it (with entropy it gathers), steps it on a timer, and hands
// the resulting grid to DisplayManager::renderLifeScreensaver. Adding a new
// screensaver means adding an adapter here, not editing App.
namespace standby {

enum class Kind {
  Life,
  Maze,
  Voronoi,
  WordRain,
  DvdBounce,
};

// A single rendered piece of text in a frame's optional text overlay. Positions
// are in grid-cell units (same coordinate space as the packed grid); the
// renderer scales them to pixels. `dim` is 0 (brightest) .. 255 (invisible),
// matching the alpha convention DisplayManager::blendOverBackground expects when
// inverted (255 - dim). `bright` requests a one-frame highlight (corner flash).
struct TextSprite {
  std::string text;
  int16_t x = 0;
  int16_t y = 0;
  uint8_t dim = 0;
  bool bright = false;
};

// Plain aggregate (no default member initializers) so brace-init keeps working
// under the framework's gnu++11 build, mirroring DisplayManager::LibraryItem.
// Every construction site lists all four fields; `text` is null for grid-only
// savers and `cells` is null for text-overlay savers.
struct Frame {
  const std::vector<uint32_t> *cells;     // live cells (packed bits), or null
  const std::vector<uint32_t> *dimCells;  // dim cells, or null
  uint32_t generation;
  // Optional text overlay. Empty/null for grid-only savers; word-rain and
  // DVD-bounce populate it and leave `cells` null. Valid until next step/seed.
  const std::vector<TextSprite> *text;
};

class Screensaver {
 public:
  virtual ~Screensaver() = default;

  // (Re)initialize from a seed. The caller supplies entropy; the screensaver
  // owns all further randomness, so step() needs no outside state.
  virtual void seed(uint32_t rngSeed) = 0;

  // Optional content hook for text savers (word-rain). Grid savers ignore it.
  // App calls this after seed() with words sampled from the current book; a
  // saver that wants words but receives none falls back to its built-in list.
  virtual void seedWords(const std::vector<std::string> &words) { (void)words; }

  // Optional interaction hook: stamp a live pattern at a grid cell. Used by the
  // interactive-Life standby touch mode (tap drops a glider). Returns true when
  // the saver acted on it (Life does; other savers ignore and return false).
  virtual bool stampPatternAt(int cellX, int cellY) {
    (void)cellX;
    (void)cellY;
    return false;
  }

  // Advance one frame. May reseed itself when the pattern stagnates.
  virtual void step() = 0;

  // The current grid to render. Pointers stay valid until the next seed/step.
  virtual Frame frame() const = 0;
};

// Builds the screensaver for a kind, sized to the given grid. Never returns null
// for a known kind.
std::unique_ptr<Screensaver> makeScreensaver(Kind kind, uint16_t columns, uint16_t rows);

}  // namespace standby
