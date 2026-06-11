#pragma once

#include <Preferences.h>

#include <string>

#include "net/WifiCredentialStore.h"

// Device-side adapter binding net::WifiCredentialStore's abstract key-value
// interface to the Arduino NVS Preferences object App already owns. Kept out of
// the pure module (and the native test build) because it pulls in Preferences.
// The pure slot logic stays host-testable; this is the only Arduino glue.
namespace net {

class PreferencesKeyValueStore : public WifiKeyValueStore {
 public:
  explicit PreferencesKeyValueStore(Preferences &prefs) : prefs_(prefs) {}

  bool has(const char *key) const override { return prefs_.isKey(key); }

  std::string getString(const char *key, const std::string &fallback = "") const override {
    if (!prefs_.isKey(key)) {
      return fallback;
    }
    return std::string(prefs_.getString(key, String(fallback.c_str())).c_str());
  }

  void putString(const char *key, const std::string &value) override {
    prefs_.putString(key, String(value.c_str()));
  }

  uint32_t getUInt(const char *key, uint32_t fallback = 0) const override {
    return prefs_.getUInt(key, fallback);
  }

  void putUInt(const char *key, uint32_t value) override { prefs_.putUInt(key, value); }

  void remove(const char *key) override {
    if (prefs_.isKey(key)) {
      prefs_.remove(key);
    }
  }

 private:
  Preferences &prefs_;
};

}  // namespace net
