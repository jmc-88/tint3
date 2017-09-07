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

int PrintUsage(std::string const& argv0) {
  util::log::Error() << "Usage: " << argv0 << u8R"EOF( <operation> [args...]

Operation can be one of:
  search, s        - searches for a theme in the remote repository
  install, in      - installs a theme
  uninstall, un    - uninstalls a theme
  list-remote, rls - lists remotely available themes
  list-local, ls   - lists locally installed themes

The "list-*" operations accept no arguments. If provided, they are ignored.
The remaining operations require a sequence of theme names as their arguments.
)EOF";
  return 1;
}

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

class Repository {
 public:
  Repository() = delete;

  static Repository FromJSON(std::string const& content) {
    return Repository(content);
  }

  void for_each(void callback(std::string, std::string, unsigned int)) {
    for (auto& entry : repository_) {
      for (auto& theme : entry["themes"]) {
        callback(entry["author"], theme["name"], theme["version"]);
      }
    }
  }

 private:
  Repository(std::string const& content) { repository_ = json::parse(content); }

  json repository_;
};

int Search() {
  util::log::Error() << "Not implemented.\n";
  return 1;
}

int Install() {
  util::log::Error() << "Not implemented.\n";
  return 1;
}

int Uninstall() {
  util::log::Error() << "Not implemented.\n";
  return 1;
}

int ListLocal() {
  util::log::Error() << "Not implemented.\n";
  return 1;
}

int ListRemote(CURL* c) {
  std::string content;
  if (!curl::FetchURL(c, kThemeRepositoryURL, &content)) {
    util::log::Error() << "Failed fetching the remote JSON file!\n";
    return 1;
  }

  auto repo = Repository::FromJSON(content);
  repo.for_each(
      [](std::string author, std::string theme, unsigned int version) {
        std::cout << author << '/' << theme << " (v" << version << ")\n";
      });
  return 0;
}
#endif  // HAVE_CURL

#ifdef HAVE_CURL
int ThemeManager(int argc, char* argv[]) {
  if (argc == 1) {
    util::log::Error() << "Error: missing operation.\n";
    return PrintUsage(argv[0]);
  }

  // TODO: CURL could be only initialized when a network operation is required;
  // failure to initialize CURL shouldn't prevent "list-local" or "uninstall"
  // from working.
  CURL* c = curl_easy_init();
  if (c == nullptr) {
    util::log::Error() << "Failed initializing CURL.\n";
    return 1;
  }
  auto cleanup_curl = util::MakeScopedDeleter([=] { curl_easy_cleanup(c); });

  std::vector<std::string> arguments{argv + 1, argv + argc};
  if (arguments.front() == "search" || arguments.front() == "s") {
    return Search();
  }
  if (arguments.front() == "install" || arguments.front() == "in") {
    return Install();
  }
  if (arguments.front() == "uninstall" || arguments.front() == "un") {
    return Uninstall();
  }
  if (arguments.front() == "list-local" || arguments.front() == "ls") {
    return ListLocal();
  }
  if (arguments.front() == "list-remote" || arguments.front() == "rls") {
    return ListRemote(c);
  }

  // actually unreachable, but will make compilers happy
  return 0;
}
#else   // HAVE_CURL
int ThemeManager(int argc, char* argv[]) {
  util::log::Error() << "Disabled: tint3's theme manager requires to be "
                        "compiled with libcurl in order to work.\n";
  return 1;
}
#endif  // HAVE_CURL
