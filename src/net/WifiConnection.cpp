#include "net/WifiConnection.h"

#include <WiFi.h>
#include <time.h>

namespace net {
namespace {

constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiConnectPollMs = 250;
// 2020-01-01T00:00:00Z. Below this the system clock is still the boot-default
// (a few seconds since power-on), not an SNTP answer.
constexpr int64_t kMinValidEpochSec = 1577836800;

}  // namespace

bool connectStation(const String &ssid, const String &password, const WifiProgress &progress) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < kWifiConnectTimeoutMs) {
    if (progress) {
      const uint32_t elapsedMs = millis() - startMs;
      progress(5 + static_cast<int>((elapsedMs * 15) / kWifiConnectTimeoutMs));
    }
    delay(kWifiConnectPollMs);
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    // Piggyback an SNTP sync on every home-Wi-Fi link. configTime is async and
    // returns immediately; the daemon resolves over the same connection while
    // RSS/OTA do their work. tz offset is companion-set, so sync in UTC here.
    beginSntpSync();
  }
  return connected;
}

void beginSntpSync() { configTime(0, 0, "pool.ntp.org", "time.nist.gov"); }

int64_t systemEpochIfValid() {
  const time_t now = time(nullptr);
  const int64_t epoch = static_cast<int64_t>(now);
  return epoch >= kMinValidEpochSec ? epoch : 0;
}

void disconnect() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

}  // namespace net
