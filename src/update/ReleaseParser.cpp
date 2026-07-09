#include "update/ReleaseParser.h"

#include <algorithm>

#include "text/JsonText.h"

namespace releaseparser {
namespace {

bool extractAssetDownloadUrl(const String &json, const String &assetName, String &assetUrl) {
  size_t searchStart = 0;
  String candidateName;
  int nameKeyIndex = -1;
  while (jsontext::readStringFrom(json, "name", searchStart, candidateName, &nameKeyIndex)) {
    if (candidateName == assetName &&
        jsontext::readStringFrom(json, "browser_download_url",
                                 static_cast<size_t>(std::max(0, nameKeyIndex)), assetUrl)) {
      return true;
    }

    searchStart = static_cast<size_t>(nameKeyIndex) + 1;
  }

  return false;
}

}  // namespace

bool parse(const String &json, const String &assetName, ReleaseInfo &out) {
  out = ReleaseInfo();
  const bool haveTag = jsontext::readString(json, "tag_name", out.tagName) &&
                       !out.tagName.isEmpty();
  extractAssetDownloadUrl(json, assetName, out.assetUrl);
  return haveTag;
}

}  // namespace releaseparser
