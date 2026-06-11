#include "net/WifiCredentialStore.h"

#include <algorithm>
#include <cctype>

namespace net {
namespace {

// Slot key builders. NVS keys cap at 15 chars; "wifi_ss" + one digit == 8.
const char *kSlotRecencySeqKey = "wifi_rseq";

// Small fixed tables avoid building key strings at runtime (NVS keys must be
// stable C strings and we never need more than kMaxSlots of each).
const char *ssidKey(int index) {
  static const char *const kKeys[WifiCredentialStore::kMaxSlots] = {
      "wifi_ss0", "wifi_ss1", "wifi_ss2", "wifi_ss3", "wifi_ss4"};
  return kKeys[index];
}

const char *passKey(int index) {
  static const char *const kKeys[WifiCredentialStore::kMaxSlots] = {
      "wifi_pw0", "wifi_pw1", "wifi_pw2", "wifi_pw3", "wifi_pw4"};
  return kKeys[index];
}

const char *recencyKey(int index) {
  static const char *const kKeys[WifiCredentialStore::kMaxSlots] = {
      "wifi_rc0", "wifi_rc1", "wifi_rc2", "wifi_rc3", "wifi_rc4"};
  return kKeys[index];
}

}  // namespace

WifiSlot WifiCredentialStore::readSlot(int index) const {
  WifiSlot slot;
  slot.index = index;
  slot.ssid = store_.getString(ssidKey(index), "");
  slot.password = store_.getString(passKey(index), "");
  slot.recency = store_.getUInt(recencyKey(index), 0);
  return slot;
}

void WifiCredentialStore::writeSlot(int index, const std::string &ssid,
                                    const std::string &password) {
  store_.putString(ssidKey(index), ssid);
  store_.putString(passKey(index), password);
}

void WifiCredentialStore::clearSlot(int index) {
  store_.remove(ssidKey(index));
  store_.remove(passKey(index));
  store_.remove(recencyKey(index));
}

uint32_t WifiCredentialStore::nextRecency() {
  uint32_t seq = store_.getUInt(kSlotRecencySeqKey, 0) + 1;
  store_.putUInt(kSlotRecencySeqKey, seq);
  return seq;
}

bool WifiCredentialStore::migrateLegacy(const char *legacySsidKey, const char *legacyPassKey) {
  if (!store_.has(legacySsidKey)) {
    return false;
  }
  std::string ssid = store_.getString(legacySsidKey, "");
  // Trim surrounding whitespace the way configuredWifiSsid() does on-device.
  const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  ssid.erase(ssid.begin(), std::find_if(ssid.begin(), ssid.end(), notSpace));
  ssid.erase(std::find_if(ssid.rbegin(), ssid.rend(), notSpace).base(), ssid.end());

  const std::string password = store_.getString(legacyPassKey, "");

  // Always clear legacy keys so they cannot shadow slots after the first run.
  store_.remove(legacySsidKey);
  store_.remove(legacyPassKey);

  if (ssid.empty()) {
    return false;
  }
  // save() updates in place if it already exists, else uses a free/LRU slot.
  save(ssid, password);
  return true;
}

std::vector<WifiSlot> WifiCredentialStore::list() const {
  std::vector<WifiSlot> slots;
  for (int i = 0; i < kMaxSlots; ++i) {
    WifiSlot slot = readSlot(i);
    if (!slot.isEmpty()) {
      slots.push_back(slot);
    }
  }
  return slots;
}

WifiSlot WifiCredentialStore::findBySsid(const std::string &ssid) const {
  for (int i = 0; i < kMaxSlots; ++i) {
    WifiSlot slot = readSlot(i);
    if (!slot.isEmpty() && slot.ssid == ssid) {
      return slot;
    }
  }
  return WifiSlot{};
}

int WifiCredentialStore::save(const std::string &ssid, const std::string &password) {
  if (ssid.empty()) {
    return -1;
  }

  int target = -1;
  int firstFree = -1;
  int lruIndex = 0;
  uint32_t lruRecency = 0;
  bool lruInit = false;

  for (int i = 0; i < kMaxSlots; ++i) {
    WifiSlot slot = readSlot(i);
    if (!slot.isEmpty() && slot.ssid == ssid) {
      target = i;  // Update in place.
      break;
    }
    if (slot.isEmpty()) {
      if (firstFree < 0) {
        firstFree = i;
      }
    } else if (!lruInit || slot.recency < lruRecency) {
      lruInit = true;
      lruRecency = slot.recency;
      lruIndex = i;
    }
  }

  if (target < 0) {
    target = firstFree >= 0 ? firstFree : lruIndex;
  }

  writeSlot(target, ssid, password);
  store_.putUInt(recencyKey(target), nextRecency());
  return target;
}

bool WifiCredentialStore::forget(const std::string &ssid) {
  for (int i = 0; i < kMaxSlots; ++i) {
    WifiSlot slot = readSlot(i);
    if (!slot.isEmpty() && slot.ssid == ssid) {
      clearSlot(i);
      return true;
    }
  }
  return false;
}

void WifiCredentialStore::markUsed(const std::string &ssid) {
  for (int i = 0; i < kMaxSlots; ++i) {
    WifiSlot slot = readSlot(i);
    if (!slot.isEmpty() && slot.ssid == ssid) {
      store_.putUInt(recencyKey(i), nextRecency());
      return;
    }
  }
}

int WifiCredentialStore::count() const {
  int n = 0;
  for (int i = 0; i < kMaxSlots; ++i) {
    if (!readSlot(i).isEmpty()) {
      ++n;
    }
  }
  return n;
}

std::vector<int> pickConnectionOrder(const std::vector<WifiSlot> &slots,
                                     const std::vector<WifiScanEntry> &scan) {
  // Strongest RSSI per visible SSID (a network can appear on multiple APs).
  struct Visible {
    int slotIndex;
    int rssi;
  };
  std::vector<Visible> visible;
  std::vector<int> unseen;

  for (const WifiSlot &slot : slots) {
    if (slot.isEmpty() || slot.index < 0) {
      continue;
    }
    int bestRssi = 0;
    bool seen = false;
    for (const WifiScanEntry &entry : scan) {
      if (entry.ssid == slot.ssid && (!seen || entry.rssi > bestRssi)) {
        seen = true;
        bestRssi = entry.rssi;
      }
    }
    if (seen) {
      visible.push_back({slot.index, bestRssi});
    } else {
      unseen.push_back(slot.index);
    }
  }

  // Visible: strongest RSSI first (stable on ties).
  std::stable_sort(visible.begin(), visible.end(),
                   [](const Visible &a, const Visible &b) { return a.rssi > b.rssi; });

  // Unseen fallback: most-recently-used first. Look recency up by slot index.
  std::stable_sort(unseen.begin(), unseen.end(), [&slots](int a, int b) {
    uint32_t ra = 0;
    uint32_t rb = 0;
    for (const WifiSlot &slot : slots) {
      if (slot.index == a) ra = slot.recency;
      if (slot.index == b) rb = slot.recency;
    }
    return ra > rb;
  });

  std::vector<int> order;
  order.reserve(visible.size() + unseen.size());
  for (const Visible &v : visible) {
    order.push_back(v.slotIndex);
  }
  for (int idx : unseen) {
    order.push_back(idx);
  }
  return order;
}

}  // namespace net
