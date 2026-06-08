#include "storage/SdCard.h"

#include <SD_MMC.h>
#include <driver/sdmmc_types.h>

#include "board/BoardConfig.h"

namespace {
constexpr const char *kMountPoint = "/sdcard";
constexpr int kSdFrequenciesKhz[] = {
    SDMMC_FREQ_DEFAULT,
    10000,
    SDMMC_FREQ_PROBING,
};
}  // namespace

bool SdCard::mount(StatusCallback callback, void *context) {
  if (!SD_MMC.setPins(BoardConfig::PIN_SD_CLK, BoardConfig::PIN_SD_CMD, BoardConfig::PIN_SD_D0)) {
    Serial.println("[sd] pin setup failed");
    return false;
  }

  for (int frequencyKhz : kSdFrequenciesKhz) {
    if (callback != nullptr) {
      callback(context, "SD", "Mounting card", "", 5);
    }
    Serial.printf("[sd] mount attempt at %d kHz\n", frequencyKhz);
    SD_MMC.end();
    if (SD_MMC.begin(kMountPoint, true, false, frequencyKhz, 5)) {
      Serial.printf("[sd] mounted (%llu MB) at %d kHz\n", sizeMb(), frequencyKhz);
      return true;
    }
  }

  Serial.println("[sd] mount failed after retries");
  return false;
}

void SdCard::unmount() { SD_MMC.end(); }

uint64_t SdCard::sizeMb() const { return SD_MMC.cardSize() / (1024ULL * 1024ULL); }

String SdCard::typeLabel() const {
  switch (SD_MMC.cardType()) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    default:
      return "Unknown";
  }
}

bool SdCard::directoryExists(const char *path) const {
  File dir = SD_MMC.open(path);
  const bool exists = dir && dir.isDirectory();
  if (dir) {
    dir.close();
  }
  return exists;
}

bool SdCard::ensureDirectory(const char *path) {
  if (directoryExists(path)) {
    Serial.printf("[sd] directory exists: %s\n", path);
    return true;
  }
  Serial.printf("[sd] creating directory: %s\n", path);
  const bool mkdirOk = SD_MMC.mkdir(path);
  const bool existsAfter = directoryExists(path);
  Serial.printf("[sd] mkdir path=%s ok=%u existsAfter=%u\n", path, mkdirOk ? 1 : 0,
                existsAfter ? 1 : 0);
  return mkdirOk || existsAfter;
}

bool SdCard::writeProbe(const char *directoryPath) {
  String path = String(directoryPath);
  if (!path.endsWith("/")) {
    path += "/";
  }
  path += ".sdcheck.tmp";
  Serial.printf("[sd] write probe path=%s\n", path.c_str());
  SD_MMC.remove(path);
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[sd] write probe open failed: %s\n", path.c_str());
    return false;
  }
  const size_t written = file.print("rsvp-nano sd check\n");
  file.close();
  const bool removed = SD_MMC.remove(path);
  Serial.printf("[sd] write probe result path=%s written=%u removed=%u\n", path.c_str(),
                static_cast<unsigned int>(written), removed ? 1 : 0);
  return written > 0 && removed;
}
