#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Multiple saved home Wi-Fi networks. The device used to store a single home
// network as one ssid/pass pair (kPrefWifiSsid/kPrefWifiPass). This module keeps
// up to kMaxSlots networks in numbered slots and decides, given a fresh scan,
// which saved network to try first.
//
// All slot bookkeeping is pure logic over an abstract key-value store so it is
// host-testable with no Arduino/NVS dependency. The device wires a thin adapter
// over the Arduino Preferences object (see WifiCredentialStorePrefs); host tests
// pass an in-memory map.
//
// Storage layout (NVS keys are <= 15 chars):
//   wifi_ss0..wifi_ss4  SSID per slot ("" means the slot is empty)
//   wifi_pw0..wifi_pw4  password per slot
//   wifi_rc0..wifi_rc4  recency counter per slot (higher == more recently used)
//   wifi_rseq           monotonically increasing recency sequence source
//
// Legacy single-network keys (wifi_ssid / wifi_pass) are migrated once into a
// free slot and then CLEARED, so the legacy pair never silently shadows the slot
// list afterwards. Callers run migrateLegacy() before reading slots.
namespace net {

// Abstract string/byte key-value store. The device backs this with NVS
// Preferences; tests back it with an in-memory map. Only the operations the
// credential store needs are exposed.
class WifiKeyValueStore {
 public:
  virtual ~WifiKeyValueStore() = default;

  virtual bool has(const char *key) const = 0;
  virtual std::string getString(const char *key, const std::string &fallback = "") const = 0;
  virtual void putString(const char *key, const std::string &value) = 0;
  virtual uint32_t getUInt(const char *key, uint32_t fallback = 0) const = 0;
  virtual void putUInt(const char *key, uint32_t value) = 0;
  virtual void remove(const char *key) = 0;
};

// One saved network as read out of the store.
struct WifiSlot {
  int index = -1;           // 0..kMaxSlots-1, or -1 for "not a real slot".
  std::string ssid;         // empty when the slot is unused.
  std::string password;     // may be empty (open networks).
  uint32_t recency = 0;     // higher == more recently used; 0 == never used.

  bool isEmpty() const { return ssid.empty(); }
};

class WifiCredentialStore {
 public:
  static constexpr int kMaxSlots = 5;

  explicit WifiCredentialStore(WifiKeyValueStore &store) : store_(store) {}

  // One-time fold of the legacy single ssid/pass pair into a slot. Idempotent:
  // after migration the legacy keys are removed, so a second call is a no-op.
  // Returns true if a migration actually happened. If the legacy SSID already
  // matches a saved slot, the legacy keys are just cleared (no duplicate slot).
  bool migrateLegacy(const char *legacySsidKey, const char *legacyPassKey);

  // All slots in storage order (index 0..kMaxSlots-1). Empty slots are skipped.
  std::vector<WifiSlot> list() const;

  // The slot holding ssid, or a slot with index == -1 if none.
  WifiSlot findBySsid(const std::string &ssid) const;

  // Save (or update) a network. If the SSID already occupies a slot, its
  // password is updated in place. Otherwise it takes the first free slot; when
  // all slots are full it replaces the least-recently-used slot. The saved slot
  // is marked most-recently-used. Returns the slot index written.
  int save(const std::string &ssid, const std::string &password);

  // Clear the slot holding ssid (no-op if absent). Returns true if removed.
  bool forget(const std::string &ssid);

  // Bump a slot to most-recently-used (called after a successful connect).
  void markUsed(const std::string &ssid);

  // Count of non-empty slots.
  int count() const;

 private:
  WifiSlot readSlot(int index) const;
  void writeSlot(int index, const std::string &ssid, const std::string &password);
  void clearSlot(int index);
  uint32_t nextRecency();

  WifiKeyValueStore &store_;
};

// A scanned network as seen during a Wi-Fi scan. Mirrors the device's scan
// result, reduced to what the ordering decision needs.
struct WifiScanEntry {
  std::string ssid;
  int rssi = 0;

  WifiScanEntry() = default;
  WifiScanEntry(std::string ssidValue, int rssiValue)
      : ssid(std::move(ssidValue)), rssi(rssiValue) {}
};

// Given the saved slots and the latest scan, return the slot indices to try in
// connection order: networks visible in the scan first (strongest RSSI first),
// then any saved-but-not-seen networks (most-recently-used first) as a
// best-effort fallback. Empty slots are never returned.
std::vector<int> pickConnectionOrder(const std::vector<WifiSlot> &slots,
                                     const std::vector<WifiScanEntry> &scan);

}  // namespace net
