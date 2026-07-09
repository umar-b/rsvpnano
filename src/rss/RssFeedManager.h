#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <vector>

#include "rss/FeedParser.h"
#include "update/OtaUpdater.h"

class RssFeedManager {
 public:
  using StatusCallback = OtaUpdater::StatusCallback;

  struct Result {
    uint8_t feedsChecked = 0;
    uint8_t articlesSaved = 0;
    uint8_t articlesSkipped = 0;
    String summary;
    String detail;
  };

  Result checkFeeds(const OtaUpdater::Config &wifiConfig, Preferences &preferences,
                    StatusCallback callback = nullptr, void *context = nullptr);

 private:
  bool connectWiFi(const OtaUpdater::Config &wifiConfig, StatusCallback callback, void *context);
  void disconnectWiFi();
  bool fetchUrl(const String &url, String &body, String &error, uint8_t feedIndex,
                uint8_t feedCount, StatusCallback callback, void *context);
  bool processFeed(const String &feedUrl, const String &feedBody, Preferences &preferences,
                   Result &result, uint8_t feedIndex, uint8_t feedCount, StatusCallback callback,
                   void *context);
  bool saveItem(const feedparser::FeedItem &item, Preferences &preferences, Result &result);
  // Fetches the item's linked page and converts it to .rsvp body text via the
  // epubconvert writer. False when the fetch fails or extraction yields too
  // few words to beat the feed's own summary.
  bool fetchFullArticleRsvp(const feedparser::FeedItem &item, String &rsvpBody);
  // Writes the article file (header + body). rsvpBody non-null = already
  // converted .rsvp text; null = item.body as a truncated plain summary.
  bool writeArticleFile(const feedparser::FeedItem &item, const String *rsvpBody);
  // Send-to-device queue (/config/sendqueue.txt, one URL per line, written by
  // the companion's POST /api/send while the device had no internet). Page
  // URLs are fetched and converted like full-text articles; lines prefixed
  // "epub " download the file into the book library instead.
  struct QueuedSend {
    String url;
    bool epub = false;
  };
  std::vector<QueuedSend> readSendQueue();
  void processSendQueue(const std::vector<QueuedSend> &sends, Preferences &preferences,
                        Result &result, StatusCallback callback, void *context);
  bool saveSentUrl(const String &url, Preferences &preferences, Result &result);
  bool saveEpubUrl(const String &url, Result &result, StatusCallback callback, void *context);
  bool itemAlreadySeen(const feedparser::FeedItem &item, Preferences &preferences);
  void markItemSeen(const feedparser::FeedItem &item, Preferences &preferences);
  String seenKeyForItem(const feedparser::FeedItem &item) const;
  String itemIdentity(const feedparser::FeedItem &item) const;
  String filenameForItem(const feedparser::FeedItem &item) const;
  String metadataSafe(String value) const;
  uint32_t fnv1a(const String &value) const;
  void report(StatusCallback callback, void *context, const String &line1, const String &line2,
              int progressPercent);
};
