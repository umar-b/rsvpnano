#include "net/HttpFetch.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "net/WifiConnection.h"

namespace net {

namespace {

constexpr uint32_t kHttpTimeoutMs = 15000;
constexpr int kTlsHandshakeTimeoutS = 15;
constexpr uint32_t kProgressIntervalMs = 1000;

bool isRedirectStatus(int statusCode) {
  return statusCode == HTTP_CODE_MOVED_PERMANENTLY || statusCode == HTTP_CODE_FOUND ||
         statusCode == HTTP_CODE_SEE_OTHER || statusCode == HTTP_CODE_TEMPORARY_REDIRECT ||
         statusCode == HTTP_CODE_PERMANENT_REDIRECT;
}

// Fills result.body from the response stream, honouring the byte cap and the
// total/idle timeouts. Returns the final status.
FetchStatus drainBody(HTTPClient &http, const FetchOptions &options, FetchResult &result) {
  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    return FetchStatus::NoStream;
  }

  const int reportedSize = http.getSize();
  result.body = "";
  if (!options.bodySink) {
    result.body.reserve(reportedSize > 0
                            ? std::min(static_cast<size_t>(reportedSize), options.maxBodyBytes)
                            : options.reserveBytes);
  }

  uint8_t buffer[512];
  size_t totalRead = 0;
  const uint32_t startedMs = millis();
  uint32_t lastByteMs = startedMs;
  uint32_t lastReportMs = 0;
  while (http.connected() || stream->available()) {
    const uint32_t nowMs = millis();
    if (options.totalTimeoutMs > 0 && nowMs - startedMs > options.totalTimeoutMs) {
      return FetchStatus::TotalTimeout;
    }
    if (options.idleTimeoutMs > 0 && nowMs - lastByteMs > options.idleTimeoutMs) {
      return FetchStatus::IdleTimeout;
    }
    if (options.progress && nowMs - lastReportMs >= kProgressIntervalMs) {
      lastReportMs = nowMs;
      options.progress(totalRead);
    }
    if (reportedSize > 0 && totalRead >= static_cast<size_t>(reportedSize)) {
      break;
    }

    const int available = stream->available();
    if (available <= 0) {
      delay(1);
      continue;
    }

    const size_t remaining = options.maxBodyBytes - totalRead;
    if (remaining == 0) {
      break;
    }

    const size_t chunkSize =
        std::min(remaining, std::min(sizeof(buffer), static_cast<size_t>(available)));
    const int bytesRead = stream->readBytes(buffer, chunkSize);
    if (bytesRead <= 0) {
      break;
    }

    lastByteMs = millis();
    totalRead += static_cast<size_t>(bytesRead);
    if (options.bodySink) {
      if (!options.bodySink(buffer, static_cast<size_t>(bytesRead))) {
        return FetchStatus::SinkFailed;
      }
    } else {
      for (int i = 0; i < bytesRead; ++i) {
        result.body += static_cast<char>(buffer[i]);
      }
    }
  }

  result.capped = totalRead >= options.maxBodyBytes;
  return FetchStatus::Ok;
}

}  // namespace

FetchResult httpGet(const String &url, const FetchOptions &options) {
  FetchResult result;

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  const bool secure = url.startsWith("https://");
  if (secure) {
    if (options.caCert != nullptr && systemEpochIfValid() > 0) {
      secureClient.setCACert(options.caCert);
    } else {
      if (options.caCert != nullptr) {
        Serial.println("[net] TLS: clock not synced, skipping CA verification");
      }
      secureClient.setInsecure();
    }
    secureClient.setHandshakeTimeout(kTlsHandshakeTimeoutS);
  }

  HTTPClient http;
  http.setUserAgent(options.userAgent);
  http.setFollowRedirects(options.followRedirects ? HTTPC_STRICT_FOLLOW_REDIRECTS
                                                  : HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(kHttpTimeoutMs);
  if (!options.followRedirects) {
    const char *headers[] = {"Location"};
    http.collectHeaders(headers, 1);
  }

  if (!(secure ? http.begin(secureClient, url) : http.begin(plainClient, url))) {
    result.status = FetchStatus::BeginFailed;
    return result;
  }

  if (options.accept != nullptr) {
    http.addHeader("Accept", options.accept);
  }

  result.httpCode = http.GET();
  if (!options.followRedirects && isRedirectStatus(result.httpCode)) {
    result.location = http.header("Location");
    result.status = FetchStatus::Redirect;
    http.end();
    return result;
  }
  if (result.httpCode != HTTP_CODE_OK) {
    result.status = FetchStatus::HttpError;
    http.end();
    return result;
  }

  if (options.maxBodyBytes == 0) {
    result.status = FetchStatus::Ok;
    http.end();
    return result;
  }

  result.status = drainBody(http, options, result);
  http.end();
  return result;
}

}  // namespace net
