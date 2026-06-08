#pragma once

#include <Arduino.h>

// Low-level SD card operations -- mounting and folder/health primitives over
// SD_MMC -- isolated from the library scanning and book loading that orchestrate
// them. StorageManager keeps its begin / diagnoseSdCard / repairSdCardFolders
// API and delegates the raw card access here. Stateless: it wraps the global
// SD_MMC, so the caller owns the "is mounted" bookkeeping.
class SdCard {
 public:
  using StatusCallback = void (*)(void *context, const char *title, const char *line1,
                                  const char *line2, int progressPercent);

  // Set the board's SD pins and mount, trying descending bus frequencies.
  // Reports "Mounting card" through callback when non-null. Returns true if a
  // frequency mounted successfully.
  bool mount(StatusCallback callback = nullptr, void *context = nullptr);
  void unmount();

  uint64_t sizeMb() const;   // card size in MB
  String typeLabel() const;  // "MMC" / "SDSC" / "SDHC/SDXC" / "Unknown"

  bool directoryExists(const char *path) const;
  bool ensureDirectory(const char *path);
  // Write then delete a temp file under directoryPath; true if both succeed.
  bool writeProbe(const char *directoryPath);
};
