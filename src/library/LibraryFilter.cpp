#include "library/LibraryFilter.h"

namespace library {

BookStatus classify(bool finished, bool hasProgress, uint8_t percent) {
  if (finished) {
    return BookStatus::Finished;
  }
  if (hasProgress && percent > 0) {
    return BookStatus::InProgress;
  }
  return BookStatus::Unread;
}

LibraryFilter nextFilter(LibraryFilter current) {
  const uint8_t next =
      static_cast<uint8_t>((static_cast<uint8_t>(current) + 1) % kLibraryFilterCount);
  return static_cast<LibraryFilter>(next);
}

LibraryFilter filterFromRaw(uint8_t raw) {
  if (raw >= kLibraryFilterCount) {
    return LibraryFilter::All;
  }
  return static_cast<LibraryFilter>(raw);
}

bool matches(LibraryFilter filter, BookStatus status) {
  switch (filter) {
    case LibraryFilter::All:
      return true;
    case LibraryFilter::InProgress:
      return status == BookStatus::InProgress;
    case LibraryFilter::Unread:
      return status == BookStatus::Unread;
    case LibraryFilter::Finished:
      return status == BookStatus::Finished;
  }
  return true;
}

std::vector<size_t> filterIndices(LibraryFilter filter, const std::vector<BookStatus> &statuses) {
  std::vector<size_t> result;
  result.reserve(statuses.size());
  for (size_t i = 0; i < statuses.size(); ++i) {
    if (matches(filter, statuses[i])) {
      result.push_back(i);
    }
  }
  return result;
}

EmptyReason emptyReason(LibraryFilter filter, size_t totalItems, size_t filteredCount) {
  if (filteredCount > 0) {
    return EmptyReason::NotEmpty;
  }
  if (totalItems == 0) {
    return EmptyReason::NoItemsAtAll;
  }
  switch (filter) {
    case LibraryFilter::InProgress:
      return EmptyReason::NoInProgress;
    case LibraryFilter::Unread:
      return EmptyReason::NoUnread;
    case LibraryFilter::Finished:
      return EmptyReason::NoFinished;
    case LibraryFilter::All:
      // All filter with zero results but non-zero items can't happen; be
      // defensive and report the generic empty-library reason.
      return EmptyReason::NoItemsAtAll;
  }
  return EmptyReason::NoItemsAtAll;
}

size_t surprisePick(const std::vector<BookStatus> &statuses, uint32_t randomValue) {
  if (statuses.empty()) {
    return kNoPick;
  }

  // Prefer unread items.
  std::vector<size_t> unread;
  unread.reserve(statuses.size());
  for (size_t i = 0; i < statuses.size(); ++i) {
    if (statuses[i] == BookStatus::Unread) {
      unread.push_back(i);
    }
  }

  if (!unread.empty()) {
    return unread[randomValue % unread.size()];
  }

  // Fall back to any item (everything has progress / is finished).
  return randomValue % statuses.size();
}

}  // namespace library
