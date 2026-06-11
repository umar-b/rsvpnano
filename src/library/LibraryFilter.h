#pragma once

// LibraryFilter -- pure, host-testable logic for the Books/Articles pickers.
//
// This module deliberately depends on nothing from Arduino or the rest of the
// firmware. It works entirely in terms of small value types (enums, indices,
// vectors) so it can be unit-tested natively. App.cpp owns the Arduino/NVS/UI
// glue and translates these results into displayed strings.
//
// The classification mirrors the device's existing finished-flag + saved-
// progress plumbing (App::bookProgressPercent, BookProgress::isFinished).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace library {

// Reading state of a single library item, derived from the finished flag and
// saved progress.
enum class BookStatus : uint8_t {
  Unread = 0,      // no saved progress, not finished
  InProgress = 1,  // saved progress > 0 and not finished
  Finished = 2,    // finished flag set (regardless of progress)
};

// The filter applied to a picker. Cycles All -> In progress -> Unread ->
// Finished -> All. Persisted per picker as the raw uint8_t value, so the
// numeric order MUST stay stable.
enum class LibraryFilter : uint8_t {
  All = 0,
  InProgress = 1,
  Unread = 2,
  Finished = 3,
};

constexpr uint8_t kLibraryFilterCount = 4;

// Reason a filtered list is empty, so the UI can show a readable line rather
// than a blank screen. App maps these to displayed strings.
enum class EmptyReason : uint8_t {
  NotEmpty = 0,      // list has at least one item; no message needed
  NoItemsAtAll = 1,  // the library itself is empty
  NoInProgress = 2,  // filter=InProgress matched nothing
  NoUnread = 3,      // filter=Unread matched nothing
  NoFinished = 4,    // filter=Finished matched nothing
};

// Classify one item from its finished flag and saved progress percent.
//
// `hasProgress` mirrors App::bookProgressPercent returning true; `percent` is
// the saved progress 0..100. A finished item is always Finished. Progress is
// "started" when percent > 0 (the brief: saved progress > 0 and not finished =
// In progress).
BookStatus classify(bool finished, bool hasProgress, uint8_t percent);

// Advance the filter one step in the cycle (used on header-row tap).
LibraryFilter nextFilter(LibraryFilter current);

// Normalize a persisted/raw value into a valid LibraryFilter (defends against
// corrupted NVS data).
LibraryFilter filterFromRaw(uint8_t raw);

// True when an item with the given status should be shown under the filter.
bool matches(LibraryFilter filter, BookStatus status);

// Given the status of every candidate item (in picker order), return the
// subset of indices (into `statuses`) that pass the filter, preserving order.
std::vector<size_t> filterIndices(LibraryFilter filter, const std::vector<BookStatus> &statuses);

// Why is the (possibly filtered) result empty? `totalItems` is the number of
// candidates before filtering; `filteredCount` is the number after filtering.
EmptyReason emptyReason(LibraryFilter filter, size_t totalItems, size_t filteredCount);

// "Surprise me" pick.
//
// Picks a random item, preferring Unread items. If no Unread items exist it
// falls back to any item. The active filter is intentionally IGNORED so the
// row always opens something when the library is non-empty (documented
// behavior -- see README/feature notes). `randomValue` is an opaque uniform
// value (e.g. esp_random()); the function reduces it modulo the candidate pool
// size. Returns the chosen index into `statuses`, or kNoPick when empty.
constexpr size_t kNoPick = static_cast<size_t>(-1);
size_t surprisePick(const std::vector<BookStatus> &statuses, uint32_t randomValue);

}  // namespace library
