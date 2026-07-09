#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <functional>

// The device's only HTTP GET kernel: client setup (TLS by scheme), redirect
// policy, and the capped/timed body drain. RSS feed checks and OTA release
// checks each carried their own copy of this loop with matching 15 s
// timeouts before this module; the fetch policy now lives here in one place
// (the same consolidation WifiConnection did for the station lifecycle).
// Callers keep their own error wording, logging, and redirect-URL
// resolution. Hardware-bound (HTTPClient) -- not host-testable.
namespace net {

struct FetchOptions {
  String userAgent;
  // Extra Accept header when non-null.
  const char *accept = nullptr;
  // PEM root bundle to verify the server against. nullptr = no verification
  // (arbitrary-host fetches like RSS). Verification needs a plausible system
  // clock for the certificate dates; until SNTP has synced, the fetch falls
  // back to unverified rather than failing on a 1970 clock.
  const char *caCert = nullptr;
  // true: HTTPClient follows redirects itself (strict). false: a redirect
  // returns FetchStatus::Redirect with the Location header for the caller.
  bool followRedirects = false;
  // Body byte cap. 0 = do not read the body at all (redirect/HEAD-like
  // resolution); the result is Ok with an empty body.
  size_t maxBodyBytes = 0;
  // Initial body reservation when the server does not report a size.
  size_t reserveBytes = 1024;
  // Whole-download and no-data timeouts for the drain. 0 = unlimited.
  uint32_t totalTimeoutMs = 0;
  uint32_t idleTimeoutMs = 0;
  // Called with total bytes read so far, roughly once per second of drain.
  std::function<void(size_t bytesRead)> progress;
  // When set, body bytes stream here (e.g. straight to an SD file) instead of
  // accumulating in result.body -- required for payloads larger than RAM.
  // Return false to abort the fetch (status becomes SinkFailed).
  std::function<bool(const uint8_t *data, size_t length)> bodySink;
};

enum class FetchStatus : uint8_t {
  Ok = 0,
  BeginFailed,
  HttpError,
  Redirect,
  NoStream,
  TotalTimeout,
  IdleTimeout,
  SinkFailed,
};

struct FetchResult {
  FetchStatus status = FetchStatus::BeginFailed;
  int httpCode = 0;
  // Location header when status == Redirect (may be empty).
  String location;
  String body;
  bool capped = false;
};

FetchResult httpGet(const String &url, const FetchOptions &options);

}  // namespace net
