#include "rss/RssFeedManager.h"

#include <HTTPClient.h>
#include <SD_MMC.h>
#include <algorithm>
#include <vector>

#include "net/HttpFetch.h"
#include "net/WifiConnection.h"
#include "rss/FeedParser.h"

namespace {

constexpr const char *kConfigPaths[] = {
    "/config/rss.conf",
    "/rss.conf",
};
constexpr const char *kBooksPath = "/books";
constexpr const char *kArticleFilesPath = "/books/articles";
constexpr const char *kStatusTitle = "RSS";
constexpr uint32_t kFeedTotalTimeoutMs = 30000;
constexpr uint32_t kFeedIdleTimeoutMs = 5000;
constexpr uint32_t kFeedProgressIntervalMs = 1000;
constexpr size_t kMaxFeedBytes = 393216;
constexpr size_t kMaxArticleChars = 24000;
constexpr uint8_t kMaxFeedsPerCheck = 8;
constexpr uint8_t kMaxItemsPerFeed = 5;
constexpr uint8_t kMaxArticlesPerCheck = 12;
constexpr uint8_t kMaxFeedRedirects = 3;

String trimCopy(String value) {
  value.trim();
  return value;
}

bool startsWithHttp(const String &url) {
  String lowered = trimCopy(url);
  lowered.toLowerCase();
  return lowered.startsWith("http://") || lowered.startsWith("https://");
}

bool isSafeFilenameChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == ' ' || c == '.';
}

String userAgent() { return String("RSVP-Nano-RSS/1.0"); }

String feedProgressLabel(uint8_t feedIndex, uint8_t feedCount) {
  return "Feed " + String(feedIndex) + "/" + String(feedCount);
}

String friendlyHttpError(int statusCode) {
  switch (statusCode) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
      return "Could not reach feed";
    case HTTPC_ERROR_SEND_HEADER_FAILED:
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
      return "Connection failed";
    case HTTPC_ERROR_NOT_CONNECTED:
      return "Wi-Fi dropped out";
    case HTTPC_ERROR_CONNECTION_LOST:
      return "Connection was lost";
    case HTTPC_ERROR_NO_STREAM:
      return "No data from site";
    case HTTPC_ERROR_NO_HTTP_SERVER:
      return "Not a web feed";
    case HTTPC_ERROR_TOO_LESS_RAM:
      return "Feed is too large";
    case HTTPC_ERROR_ENCODING:
      return "Feed format not supported";
    case HTTPC_ERROR_STREAM_WRITE:
      return "Could not read feed";
    case HTTPC_ERROR_READ_TIMEOUT:
      return "Site took too long";
  }

  switch (statusCode) {
    case HTTP_CODE_BAD_REQUEST:
      return "Feed link looks wrong";
    case HTTP_CODE_UNAUTHORIZED:
      return "Feed needs login";
    case HTTP_CODE_FORBIDDEN:
      return "Site blocked reader";
    case HTTP_CODE_NOT_FOUND:
      return "Feed not found";
    case HTTP_CODE_REQUEST_TIMEOUT:
      return "Site took too long";
    case HTTP_CODE_TOO_MANY_REQUESTS:
      return "Site says try later";
  }

  if (statusCode >= 500 && statusCode < 600) {
    return "Site is having trouble";
  }
  if (statusCode >= 300 && statusCode < 400) {
    return "Feed moved unexpectedly";
  }
  return "Could not download feed";
}

String urlScheme(const String &url) {
  const int marker = url.indexOf("://");
  if (marker < 0) {
    return "http";
  }
  return url.substring(0, marker);
}

String urlOrigin(const String &url) {
  const int marker = url.indexOf("://");
  const int hostStart = marker < 0 ? 0 : marker + 3;
  int hostEnd = url.indexOf('/', hostStart);
  if (hostEnd < 0) {
    hostEnd = url.length();
  }
  return url.substring(0, hostEnd);
}

String resolveRedirectUrl(const String &baseUrl, String location) {
  location.trim();
  if (location.startsWith("http://") || location.startsWith("https://")) {
    return location;
  }
  if (location.startsWith("//")) {
    return urlScheme(baseUrl) + ":" + location;
  }
  if (location.startsWith("/")) {
    return urlOrigin(baseUrl) + location;
  }

  int slash = baseUrl.lastIndexOf('/');
  const int marker = baseUrl.indexOf("://");
  if (slash <= marker + 2) {
    return urlOrigin(baseUrl) + "/" + location;
  }
  return baseUrl.substring(0, slash + 1) + location;
}

}  // namespace

RssFeedManager::Result RssFeedManager::checkFeeds(const OtaUpdater::Config &wifiConfig,
                                                  Preferences &preferences,
                                                  StatusCallback callback, void *context) {
  Result result;
  if (trimCopy(wifiConfig.wifiSsid).isEmpty()) {
    result.summary = "Wi-Fi not set";
    result.detail = "Settings -> Wi-Fi";
    return result;
  }

  if (!connectWiFi(wifiConfig, callback, context)) {
    disconnectWiFi();
    result.summary = "Wi-Fi failed";
    result.detail = "Check credentials";
    return result;
  }

  File config;
  for (const char *path : kConfigPaths) {
    config = SD_MMC.open(path);
    if (config && !config.isDirectory()) {
      break;
    }
    if (config) {
      config.close();
    }
  }

  if (!config || config.isDirectory()) {
    if (config) {
      config.close();
    }
    disconnectWiFi();
    result.summary = "No feeds";
    result.detail = "/config/rss.conf";
    return result;
  }

  std::vector<String> feeds;
  feeds.reserve(kMaxFeedsPerCheck);
  while (config.available() && feeds.size() < kMaxFeedsPerCheck) {
    String line = config.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line.startsWith("#")) {
      continue;
    }
    if (line.startsWith("feed=")) {
      line = line.substring(5);
      line.trim();
    }
    if (!startsWithHttp(line)) {
      continue;
    }

    feeds.push_back(line);
  }
  config.close();

  if (feeds.empty()) {
    disconnectWiFi();
    result.summary = "No feed URLs";
    result.detail = "/config/rss.conf";
    return result;
  }

  uint8_t feedFailures = 0;
  String firstFeedError;
  bool mixedFeedErrors = false;

  for (uint8_t feedIndex = 0;
       feedIndex < feeds.size() && result.articlesSaved < kMaxArticlesPerCheck; ++feedIndex) {
    const String &line = feeds[feedIndex];
    const uint8_t displayIndex = feedIndex + 1;
    const uint8_t feedCount = feeds.size();
    report(callback, context, feedProgressLabel(displayIndex, feedCount),
           "Downloading " + feedparser::hostLabelForUrl(line), 15 + displayIndex * 8);

    String feedBody;
    String error;
    if (!fetchUrl(line, feedBody, error, displayIndex, feedCount, callback, context)) {
      Serial.printf("[rss] feed failed url=%s error=%s\n", line.c_str(), error.c_str());
      ++feedFailures;
      if (firstFeedError.isEmpty()) {
        firstFeedError = error;
      } else if (error != firstFeedError) {
        mixedFeedErrors = true;
      }
      report(callback, context, feedProgressLabel(displayIndex, feedCount), "Skipped: " + error,
             15 + displayIndex * 8);
      delay(600);
      continue;
    }

    ++result.feedsChecked;
    processFeed(line, feedBody, preferences, result, displayIndex, feedCount, callback, context);
  }

  disconnectWiFi();

  if (result.feedsChecked == 0) {
    result.summary = "Feeds unavailable";
    result.detail =
        mixedFeedErrors || firstFeedError.isEmpty() ? "Check feed URLs" : firstFeedError;
  } else if (result.articlesSaved == 0) {
    result.summary = "No new articles";
    result.detail = String(result.feedsChecked) + " checked";
    if (feedFailures > 0) {
      result.detail += ", " + String(feedFailures) + " failed";
    }
  } else {
    result.summary = String(result.articlesSaved) + " article" +
                     (result.articlesSaved == 1 ? "" : "s") + " saved";
    result.detail = String(result.feedsChecked) + " checked";
    if (feedFailures > 0) {
      result.detail += ", " + String(feedFailures) + " failed";
    }
  }
  return result;
}

bool RssFeedManager::connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback,
                                 void *context) {
  return net::connectStation(wifiConfig.wifiSsid, wifiConfig.wifiPassword, [&](int percent) {
    report(callback, context, "Connecting Wi-Fi", wifiConfig.wifiSsid, percent);
  });
}

void RssFeedManager::disconnectWiFi() { net::disconnect(); }

bool RssFeedManager::fetchUrl(const String &url, String &body, String &error,
                              uint8_t feedIndex, uint8_t feedCount,
                              StatusCallback callback, void *context) {
  String currentUrl = url;
  for (uint8_t redirectCount = 0; redirectCount <= kMaxFeedRedirects; ++redirectCount) {
    report(callback, context, feedProgressLabel(feedIndex, feedCount),
           "Requesting " + feedparser::hostLabelForUrl(currentUrl), 18 + feedIndex * 7);

    net::FetchOptions options;
    options.userAgent = userAgent();
    options.maxBodyBytes = kMaxFeedBytes;
    options.reserveBytes = 8192;
    options.totalTimeoutMs = kFeedTotalTimeoutMs;
    options.idleTimeoutMs = kFeedIdleTimeoutMs;
    options.progress = [&](size_t bytesRead) {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded " + String(static_cast<unsigned int>(bytesRead / 1024)) + " KB",
             20 + feedIndex * 7);
    };
    const net::FetchResult fetched = net::httpGet(currentUrl, options);

    switch (fetched.status) {
      case net::FetchStatus::Redirect:
        if (fetched.location.isEmpty()) {
          error = "Feed moved but gave no link";
          Serial.printf("[rss] redirect missing location status=%d url=%s\n", fetched.httpCode,
                        currentUrl.c_str());
          return false;
        }
        currentUrl = resolveRedirectUrl(currentUrl, fetched.location);
        Serial.printf("[rss] redirect %u url=%s\n", static_cast<unsigned int>(fetched.httpCode),
                      currentUrl.c_str());
        report(callback, context, feedProgressLabel(feedIndex, feedCount),
               "Redirecting to " + feedparser::hostLabelForUrl(currentUrl), 18 + feedIndex * 7);
        delay(250);
        continue;
      case net::FetchStatus::BeginFailed:
        error = "Feed link did not open";
        Serial.printf("[rss] begin failed url=%s\n", currentUrl.c_str());
        return false;
      case net::FetchStatus::HttpError:
        error = friendlyHttpError(fetched.httpCode);
        Serial.printf("[rss] http failed status=%d message=%s url=%s\n", fetched.httpCode,
                      error.c_str(), currentUrl.c_str());
        return false;
      case net::FetchStatus::NoStream:
        error = "No data from site";
        Serial.printf("[rss] no stream url=%s\n", currentUrl.c_str());
        return false;
      case net::FetchStatus::TotalTimeout:
        error = "Site took too long";
        Serial.printf("[rss] total timeout url=%s\n", currentUrl.c_str());
        return false;
      case net::FetchStatus::IdleTimeout:
        error = "Site stopped sending data";
        Serial.printf("[rss] idle timeout url=%s\n", currentUrl.c_str());
        return false;
      case net::FetchStatus::Ok:
        break;
    }

    body = fetched.body;
    if (body.isEmpty()) {
      error = "Feed was empty";
      Serial.printf("[rss] empty response url=%s\n", currentUrl.c_str());
      return false;
    }
    if (fetched.capped) {
      Serial.printf("[rss] feed capped url=%s bytes=%u\n", currentUrl.c_str(),
                    static_cast<unsigned int>(body.length()));
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Reached " + String(static_cast<unsigned int>(kMaxFeedBytes / 1024)) + " KB cap",
             20 + feedIndex * 7);
      delay(500);
    } else {
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Downloaded " + String(static_cast<unsigned int>(body.length() / 1024)) + " KB",
             20 + feedIndex * 7);
    }
    return true;
  }

  error = "Feed redirected too often";
  Serial.printf("[rss] too many redirects url=%s\n", url.c_str());
  return false;
}

bool RssFeedManager::processFeed(const String &feedUrl, const String &feedBody,
                                 Preferences &preferences, Result &result,
                                 uint8_t feedIndex, uint8_t feedCount,
                                 StatusCallback callback, void *context) {
  size_t searchStart = 0;
  uint8_t itemCount = 0;
  uint8_t savedBefore = result.articlesSaved;
  uint8_t skippedBefore = result.articlesSkipped;
  report(callback, context, feedProgressLabel(feedIndex, feedCount), "Parsing items",
         24 + feedIndex * 7);
  while (itemCount < kMaxItemsPerFeed && result.articlesSaved < kMaxArticlesPerCheck) {
    feedparser::FeedItem item;
    if (!feedparser::parseNextItem(feedBody, searchStart, item)) {
      break;
    }
    ++itemCount;
    if (itemAlreadySeen(item, preferences)) {
      ++result.articlesSkipped;
      report(callback, context, feedProgressLabel(feedIndex, feedCount),
             "Already synced " + String(itemCount) + "/" + String(kMaxItemsPerFeed),
             24 + feedIndex * 7);
      continue;
    }
    report(callback, context, "Saving article " + String(itemCount), item.title,
           24 + feedIndex * 7);
    saveItem(item, preferences, result);
  }
  const uint8_t savedHere = result.articlesSaved - savedBefore;
  const uint8_t skippedHere = result.articlesSkipped - skippedBefore;
  if (itemCount == 0) {
    report(callback, context, feedProgressLabel(feedIndex, feedCount), "No usable items",
           24 + feedIndex * 7);
  } else {
    report(callback, context, feedProgressLabel(feedIndex, feedCount),
           String(savedHere) + " saved, " + String(skippedHere) + " skipped",
           24 + feedIndex * 7);
  }
  Serial.printf("[rss] feed url=%s items=%u saved=%u skipped=%u\n", feedUrl.c_str(),
                static_cast<unsigned int>(itemCount), static_cast<unsigned int>(savedHere),
                static_cast<unsigned int>(skippedHere));
  delay(600);
  return itemCount > 0;
}

bool RssFeedManager::saveItem(const feedparser::FeedItem &item, Preferences &preferences,
                              Result &result) {
  SD_MMC.mkdir(kBooksPath);
  SD_MMC.mkdir(kArticleFilesPath);
  const String finalPath = String(kArticleFilesPath) + "/" + filenameForItem(item);
  const String tmpPath = finalPath + ".tmp";
  SD_MMC.remove(tmpPath);

  File file = SD_MMC.open(tmpPath, FILE_WRITE);
  if (!file) {
    Serial.printf("[rss] could not create %s\n", tmpPath.c_str());
    return false;
  }

  file.println("@rsvp 1");
  file.print("@title ");
  file.println(metadataSafe(item.title));
  file.print("@author ");
  file.println(
      metadataSafe(item.author.isEmpty() ? feedparser::sourceLabelForItem(item) : item.author));
  if (!item.link.isEmpty()) {
    file.print("@source ");
    file.println(metadataSafe(item.link));
  }
  file.println();

  String body = item.body;
  if (body.length() > kMaxArticleChars) {
    body = body.substring(0, kMaxArticleChars);
    body += "\n\n[Article truncated on device.]";
  }
  file.println(body);
  file.close();

  SD_MMC.remove(finalPath);
  if (!SD_MMC.rename(tmpPath, finalPath)) {
    SD_MMC.remove(tmpPath);
    Serial.printf("[rss] rename failed %s\n", finalPath.c_str());
    return false;
  }

  markItemSeen(item, preferences);
  ++result.articlesSaved;
  Serial.printf("[rss] saved %s\n", finalPath.c_str());
  return true;
}

bool RssFeedManager::itemAlreadySeen(const feedparser::FeedItem &item, Preferences &preferences) {
  return preferences.getBool(seenKeyForItem(item).c_str(), false);
}

void RssFeedManager::markItemSeen(const feedparser::FeedItem &item, Preferences &preferences) {
  preferences.putBool(seenKeyForItem(item).c_str(), true);
}

String RssFeedManager::seenKeyForItem(const feedparser::FeedItem &item) const {
  char key[16];
  std::snprintf(key, sizeof(key), "rss%08lx", static_cast<unsigned long>(fnv1a(itemIdentity(item))));
  return String(key);
}

String RssFeedManager::itemIdentity(const feedparser::FeedItem &item) const {
  return item.link.isEmpty() ? item.title : item.link;
}

String RssFeedManager::filenameForItem(const feedparser::FeedItem &item) const {
  String name = item.title;  // already HTML-stripped and entity-decoded by FeedParser
  String cleaned;
  cleaned.reserve(80);
  for (size_t i = 0; i < name.length() && cleaned.length() < 72; ++i) {
    const char c = name[i];
    cleaned += isSafeFilenameChar(c) ? c : '-';
  }
  cleaned.trim();
  while (cleaned.indexOf("--") >= 0) {
    cleaned.replace("--", "-");
  }
  if (cleaned.isEmpty()) {
    cleaned = "rss-article";
  }
  char suffix[16];
  std::snprintf(suffix, sizeof(suffix), "-%08lx", static_cast<unsigned long>(fnv1a(itemIdentity(item))));
  return cleaned + suffix + ".rsvp";
}

String RssFeedManager::metadataSafe(String value) const {
  value.replace("\r", " ");
  value.replace("\n", " ");
  value.trim();
  return value;
}

uint32_t RssFeedManager::fnv1a(const String &value) const {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < value.length(); ++i) {
    hash ^= static_cast<uint8_t>(value[i]);
    hash *= 16777619UL;
  }
  return hash;
}

void RssFeedManager::report(StatusCallback callback, void *context, const String &line1,
                            const String &line2, int progressPercent) {
  if (callback == nullptr) {
    return;
  }
  callback(context, kStatusTitle, line1.c_str(), line2.c_str(), progressPercent);
}
