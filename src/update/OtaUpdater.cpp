#include "update/OtaUpdater.h"

#include <algorithm>

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "net/HttpFetch.h"
#include "net/WifiConnection.h"
#include "update/ReleaseParser.h"

#ifndef RSVP_FIRMWARE_VERSION
#define RSVP_FIRMWARE_VERSION "dev"
#endif

namespace {

constexpr const char *kConfigPaths[] = {
    "/config/ota.conf",
    "/ota.conf",
};
constexpr size_t kMaxReleaseJsonBytes = 32768;
constexpr const char *kStatusTitle = "OTA";
// Bound a stalled asset download so it fails with a reason instead of hanging
// on a half-open TLS connection that never delivers more bytes.
constexpr uint32_t kDownloadStallTimeoutSecs = 20;
// Marginal Wi-Fi can stall or corrupt the multi-MB asset download; retry the
// whole download a few times before giving up.
constexpr int kMaxDownloadAttempts = 3;
String trimCopy(String value) {
  value.trim();
  return value;
}

bool parseBoolValue(const String &value) {
  String lowered = trimCopy(value);
  lowered.toLowerCase();
  return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

String userAgentForVersion(const String &version) {
  return String("RSVP-Nano/") + (version.isEmpty() ? "dev" : version);
}

String versionDetail(const String &currentVersion, const String &latestVersion) {
  if (latestVersion.isEmpty()) {
    return currentVersion;
  }
  if (currentVersion.isEmpty()) {
    return latestVersion;
  }
  return currentVersion + " -> " + latestVersion;
}

}  // namespace

bool OtaUpdater::loadConfig(Config &config) const {
  config = Config();
  for (const char *path : kConfigPaths) {
    if (loadConfigFromPath(path, config)) {
      return true;
    }
  }

  return false;
}

bool OtaUpdater::isConfigured(const Config &config) const {
  return !trimCopy(config.wifiSsid).isEmpty();
}

String OtaUpdater::currentVersion() const { return RSVP_FIRMWARE_VERSION; }

bool OtaUpdater::loadConfigFromPath(const char *path, Config &config) const {
  File file = SD_MMC.open(path);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line.startsWith("#")) {
      continue;
    }

    const int equalsIndex = line.indexOf('=');
    if (equalsIndex <= 0) {
      continue;
    }

    String key = line.substring(0, equalsIndex);
    String value = line.substring(equalsIndex + 1);
    key.trim();
    value.trim();
    key.toLowerCase();

    if (key == "wifi_ssid") {
      config.wifiSsid = value;
    } else if (key == "wifi_password") {
      config.wifiPassword = value;
    } else if (key == "github_owner") {
      config.githubOwner = value;
    } else if (key == "github_repo") {
      config.githubRepo = value;
    } else if (key == "asset_name") {
      config.assetName = value;
    } else if (key == "auto_check") {
      config.autoCheck = parseBoolValue(value);
    }
  }

  file.close();
  return true;
}

bool OtaUpdater::connectWiFi(const Config &config, StatusCallback callback,
                             void *context) const {
  return net::connectStation(config.wifiSsid, config.wifiPassword, [&](int percent) {
    reportStatus(callback, context, kStatusTitle, "Connecting Wi-Fi", config.wifiSsid, percent);
  });
}

void OtaUpdater::disconnectWiFi() const { net::disconnect(); }

bool OtaUpdater::fetchLatestRelease(const Config &config, LatestRelease &release,
                                    String &errorDetail, StatusCallback callback,
                                    void *context) const {
  const String version = currentVersion();
  const String url = "https://api.github.com/repos/" + config.githubOwner + "/" +
                     config.githubRepo + "/releases/latest";

  reportStatus(callback, context, kStatusTitle, "Checking GitHub", config.githubRepo, 22);

  net::FetchOptions options;
  options.userAgent = userAgentForVersion(version);
  options.accept = "application/vnd.github+json";
  options.followRedirects = true;
  options.maxBodyBytes = kMaxReleaseJsonBytes;
  const net::FetchResult fetched = net::httpGet(url, options);

  if (fetched.status == net::FetchStatus::BeginFailed) {
    errorDetail = "HTTP begin failed";
    return false;
  }
  if (fetched.status == net::FetchStatus::HttpError) {
    if (fetched.httpCode == HTTP_CODE_NOT_FOUND) {
      errorDetail = "No published release";
    } else {
      errorDetail = "GitHub HTTP " + String(fetched.httpCode);
    }
    return false;
  }

  releaseparser::ReleaseInfo parsed;
  if (!releaseparser::parse(fetched.body, config.assetName, parsed)) {
    errorDetail = "Release tag missing";
    return false;
  }
  release.tagName = parsed.tagName;
  release.assetUrl = parsed.assetUrl;

  if (release.assetUrl.isEmpty()) {
    errorDetail = config.assetName + " missing";
    return false;
  }

  return true;
}

bool OtaUpdater::resolveDownloadUrl(const String &assetUrl, const String &version,
                                    String &resolvedUrl, String &errorDetail,
                                    StatusCallback callback, void *context) const {
  reportStatus(callback, context, kStatusTitle, "Resolving asset", version, 29);

  net::FetchOptions options;
  options.userAgent = userAgentForVersion(version);
  options.accept = "application/octet-stream";
  // maxBodyBytes 0: only the status/Location matter, the body is never read.
  const net::FetchResult fetched = net::httpGet(assetUrl, options);

  if (fetched.status == net::FetchStatus::Ok) {
    resolvedUrl = assetUrl;
    return true;
  }
  if (fetched.status == net::FetchStatus::Redirect) {
    resolvedUrl = fetched.location;
    if (!resolvedUrl.isEmpty()) {
      return true;
    }
    errorDetail = "Asset redirect missing";
    return false;
  }
  if (fetched.status == net::FetchStatus::BeginFailed) {
    errorDetail = "Asset URL failed";
    return false;
  }

  errorDetail = "Asset HTTP " + String(fetched.httpCode);
  return false;
}

void OtaUpdater::reportStatus(StatusCallback callback, void *context, const char *title,
                              const String &line1, const String &line2,
                              int progressPercent) const {
  if (callback == nullptr) {
    return;
  }

  callback(context, title, line1.c_str(), line2.c_str(), progressPercent);
}

OtaUpdater::Result OtaUpdater::checkOnly(const Config &config, StatusCallback callback,
                                         void *context) const {
  Result result;
  result.currentVersion = currentVersion();

  if (!isConfigured(config)) {
    result.code = ResultCode::NotConfigured;
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(config, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::ConnectFailed;
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  LatestRelease release;
  String metadataError;
  if (!fetchLatestRelease(config, release, metadataError, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::MetadataFailed;
    result.summary = "GitHub failed";
    result.detail = metadataError;
    return result;
  }

  disconnectWiFi();
  result.latestVersion = release.tagName;
  if (release.tagName == result.currentVersion) {
    result.code = ResultCode::NoUpdate;
    result.summary = "Already current";
    result.detail = release.tagName;
    return result;
  }

  if (release.assetUrl.isEmpty()) {
    result.code = ResultCode::AssetMissing;
    result.summary = "Asset missing";
    result.detail = config.assetName;
    return result;
  }

  result.code = ResultCode::UpdateAvailable;
  result.summary = "Update available";
  result.detail = release.tagName;
  return result;
}

OtaUpdater::Result OtaUpdater::checkAndInstall(const Config &config, StatusCallback callback,
                                               void *context) const {
  Result result;
  result.currentVersion = currentVersion();

  if (!isConfigured(config)) {
    result.code = ResultCode::NotConfigured;
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(config, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::ConnectFailed;
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  LatestRelease release;
  String metadataError;
  if (!fetchLatestRelease(config, release, metadataError, callback, context)) {
    disconnectWiFi();
    result.code = ResultCode::MetadataFailed;
    result.summary = "GitHub failed";
    result.detail = metadataError;
    return result;
  }

  result.latestVersion = release.tagName;
  if (release.tagName == result.currentVersion) {
    disconnectWiFi();
    result.code = ResultCode::NoUpdate;
    result.summary = "Already current";
    result.detail = release.tagName;
    return result;
  }

  if (release.assetUrl.isEmpty()) {
    disconnectWiFi();
    result.code = ResultCode::AssetMissing;
    result.summary = "Asset missing";
    result.detail = config.assetName;
    return result;
  }

  reportStatus(callback, context, kStatusTitle, "Preparing update",
               versionDetail(result.currentVersion, result.latestVersion), 28);

  // Wi-Fi stays connected across attempts; re-resolve each time since the
  // signed asset URL from GitHub is short-lived.
  t_httpUpdate_return updateResult = HTTP_UPDATE_FAILED;
  String lastError;
  for (int attempt = 1; attempt <= kMaxDownloadAttempts; ++attempt) {
    if (attempt > 1) {
      reportStatus(callback, context, kStatusTitle, "Retrying download",
                   String(attempt) + "/" + String(kMaxDownloadAttempts), 28);
      delay(1000);
    }

    String resolvedAssetUrl;
    String resolveError;
    if (!resolveDownloadUrl(release.assetUrl, result.latestVersion, resolvedAssetUrl, resolveError,
                            callback, context)) {
      lastError = resolveError;
      continue;
    }

    WiFiClientSecure client;
    // Match the metadata request behavior until the update path gains certificate pinning or
    // signature verification above the transport layer.
    client.setInsecure();
    client.setHandshakeTimeout(15);
    // Read timeout for the body: without it, a half-open TLS connection that
    // stops sending bytes leaves HTTPUpdate spinning on a 0-byte stream forever.
    client.setTimeout(kDownloadStallTimeoutSecs);

    HTTPUpdate updater;
    updater.rebootOnUpdate(false);
    updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int lastReportedProgress = -1;
    updater.onProgress([this, callback, context, &result, &lastReportedProgress](int current,
                                                                                  int total) {
      if (total <= 0) {
        // No Content-Length from the download host: show bytes received so a live
        // (if slow) download is visible and a true stall reads as a frozen count.
        const int kb = current / 1024;
        if (kb == lastReportedProgress) {
          return;
        }
        lastReportedProgress = kb;
        const String line = result.latestVersion + "  " + String(kb) + " KB";
        reportStatus(callback, context, kStatusTitle, "Downloading update", line, -1);
        return;
      }

      const int progress = 30 + static_cast<int>((static_cast<int64_t>(current) * 65) / total);
      if (progress == lastReportedProgress) {
        return;
      }

      lastReportedProgress = progress;
      reportStatus(callback, context, kStatusTitle, "Downloading update", result.latestVersion,
                   progress);
    });

    const String version = result.currentVersion;
    updateResult = updater.update(client, resolvedAssetUrl, version, [version](HTTPClient *http) {
      http->setUserAgent(userAgentForVersion(version));
      http->addHeader("Accept", "application/octet-stream");
    });

    if (updateResult == HTTP_UPDATE_OK || updateResult == HTTP_UPDATE_NO_UPDATES) {
      break;
    }
    lastError = updater.getLastErrorString();
  }

  disconnectWiFi();

  switch (updateResult) {
    case HTTP_UPDATE_OK:
      result.code = ResultCode::Success;
      result.summary = "Update ready";
      result.detail = result.latestVersion;
      result.rebootRequired = true;
      return result;
    case HTTP_UPDATE_NO_UPDATES:
      result.code = ResultCode::NoUpdate;
      result.summary = "Already current";
      result.detail = result.latestVersion;
      return result;
    case HTTP_UPDATE_FAILED:
    default:
      result.code = ResultCode::InstallFailed;
      result.summary = "Update failed";
      result.detail = lastError.isEmpty() ? "download failed" : lastError;
      return result;
  }
}
