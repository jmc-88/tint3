#include <iostream>
#include <string>

#include "util/common.hh"
#include "util/log.hh"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif  // HAVE_CURL

#include "json.hpp"
using nlohmann::json;

namespace {

const std::string kThemeRepositoryURL =
    "https://jmc-88.github.io/tint3-themes/repository.json";

}  // namespace

#ifdef HAVE_CURL
namespace curl {

size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  util::string::Builder& b =
      *reinterpret_cast<util::string::Builder*>(userdata);
  b << std::string{ptr, size * nmemb};
  return size * nmemb;
}

bool FetchURL(CURL* c, std::string const& url, std::string* result) {
  util::string::Builder b;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl::WriteCallback);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
  CURLcode res = curl_easy_perform(c);
  if (res != CURLE_OK) {
    return false;
  }
  result->assign(b);
  return true;
}

}  // namespace curl
#endif  // HAVE_CURL

int ThemeManager(int argc, char* argv[]) {
  // TODO: actually do something instead of dumping the repository to stdout.
  CURL* c = curl_easy_init();
  if (c == nullptr) {
    util::log::Error() << "Failed initializing CURL.\n";
    return 1;
  }
  auto cleanup_curl = util::MakeScopedDeleter([=] { curl_easy_cleanup(c); });

  std::string content;
  if (!curl::FetchURL(c, kThemeRepositoryURL, &content)) {
    util::log::Error() << "Failed fetching the remote JSON file!\n";
    return 1;
  }
  std::cout << json::parse(content);
  return 0;
}
