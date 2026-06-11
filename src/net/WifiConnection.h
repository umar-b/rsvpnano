#pragma once

#include <Arduino.h>

#include <functional>

// The device's only station-mode WiFi lifecycle: bring the radio up to reach
// the internet (RSS feed checks, OTA update checks) and tear it back down.
// Both callers shared a byte-identical connect/poll/timeout loop before this
// module; the timeout policy now lives here in one place.
namespace net {

// Reports association progress as a percentage in [0, 100] while connecting.
using WifiProgress = std::function<void(int percent)>;

// Brings up WIFI_STA and blocks until associated or the connect timeout
// elapses. Returns true only when connected. progress may be null.
bool connectStation(const String &ssid, const String &password,
                    const WifiProgress &progress = nullptr);

// Disconnects and powers the radio off (WIFI_OFF).
void disconnect();

// Opportunistic SNTP: while associated (any station connect for RSS/OTA), start
// the SNTP daemon via configTime so the ESP32 system clock can be set from the
// network at no extra connection cost. Non-blocking -- it only kicks the daemon;
// the daemon keeps running until disconnect(). Safe to call repeatedly.
void beginSntpSync();

// Reads the ESP32 system clock if SNTP has produced a plausible wall-clock time
// (>= 2020-01-01 UTC). Returns epoch seconds, or 0 when not yet synced. Call
// after a network op so the in-flight SNTP daemon has had time to answer.
int64_t systemEpochIfValid();

}  // namespace net
