#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/types/span.h"

#include "util/common.hh"
#include "util/fs.hh"
#include "util/log.hh"
#include "util/xdg.hh"

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
  search, s        - searches for matching remotely available themes
  install, in      - installs a theme
  uninstall, un    - uninstalls a theme
  list-local, ls   - lists locally installed themes

The "list-local" operation expects no arguments. If given, they are ignored.
The "search" operation expects any number of arguments which will be used to
match against remotely available themes. No arguments results in listing all
available ones.
The remaining operations require a sequence of theme names as their arguments.
)EOF";
  return 1;
}

}  // namespace

#ifdef HAVE_CURL
namespace curl {

size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  std::ostringstream& ss = *reinterpret_cast<std::ostringstream*>(userdata);
  ss << std::string{ptr, size * nmemb};
  return size * nmemb;
}

bool FetchURL(CURL* c, std::string const& url, std::string* result) {
  std::ostringstream ss;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl::WriteCallback);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &ss);
  CURLcode res = curl_easy_perform(c);
  if (res != CURLE_OK) {
    return false;
  }
  result->assign(ss.str());
  return true;
}

}  // namespace curl

class Repository {
 public:
  Repository() = delete;

  static Repository FromJSON(std::string const& content) {
    return Repository(content);
  }

  void for_each(
      std::function<void(std::string, std::string, unsigned int)> callback) {
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

int Search(CURL* c, absl::Span<char* const> needles) {
  std::string content;
  if (!curl::FetchURL(c, kThemeRepositoryURL, &content)) {
    util::log::Error() << "Failed fetching the remote JSON file!\n";
    return 1;
  }

  auto match = [&](absl::string_view author, absl::string_view theme,
                   unsigned int version) {
    return absl::c_all_of(needles, [&](absl::string_view str) {
      return absl::StrContains(author, str) || absl::StrContains(theme, str) ||
             absl::StrContains(util::string::Representation(version), str);
    });
  };

  auto repo = Repository::FromJSON(content);
  repo.for_each(
      [&](std::string author, std::string theme, unsigned int version) {
        if (match(author, theme, version)) {
          std::cout << author << '/' << theme << " (v" << version << ")\n";
        }
      });
  return 0;
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
  auto local_repo_dir = util::xdg::basedir::DataHome() / "tint3" / "themes";
  auto local_repo_path = local_repo_dir / "repository.json";
  if (!util::fs::FileExists(local_repo_path)) {
    std::cout << "No themes installed.\n";
    return 0;
  }

  std::string local_repo_contents;
  if (!util::fs::ReadFile(local_repo_path, &local_repo_contents)) {
    util::log::Error() << "Failed reading \"" << local_repo_path << "\".\n";
    return 1;
  }

  static constexpr auto normalize_path_component = [](absl::string_view str) {
    return absl::StrReplaceAll(
        str, {{" ", "_"}, {":", "_"}, {"/", "_"}, {"_", "__"}});
  };

  static constexpr auto format_file_name = [](absl::string_view author,
                                              absl::string_view theme) {
    return absl::StrFormat("%s_%s.tint3rc", normalize_path_component(author),
                           normalize_path_component(theme));
  };

  auto repo = Repository::FromJSON(local_repo_contents);
  repo.for_each(
      [&](std::string author, std::string theme, unsigned int version) {
        auto local_file_name = format_file_name(author, theme);
        if (util::fs::FileExists(local_repo_dir / local_file_name)) {
          std::cout << author << '/' << theme << " (v" << version << ")\n"
                    << "  * " << local_file_name << "\n\n";
        }
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
  ABSL_ATTRIBUTE_UNUSED auto cleanup_curl =
      util::MakeScopedCallback([=] { curl_easy_cleanup(c); });

  std::vector<std::string> arguments{argv + 1, argv + argc};
  if (arguments.front() == "search" || arguments.front() == "s") {
    return Search(c, absl::MakeConstSpan(argv + 2, argv + argc));
  } else if (arguments.front() == "install" || arguments.front() == "in") {
    return Install();
  } else if (arguments.front() == "uninstall" || arguments.front() == "un") {
    return Uninstall();
  } else if (arguments.front() == "list-local" || arguments.front() == "ls") {
    return ListLocal();
  }

  util::log::Error() << "Error: unknown operation \"" << arguments.front()
                     << "\".\n\n";
  return PrintUsage(argv[0]);
}
#else   // HAVE_CURL
int ThemeManager(int argc, char* argv[]) {
  util::log::Error() << "Disabled: tint3's theme manager requires to be "
                        "compiled with libcurl in order to work.\n";
  return 1;
}
#endif  // HAVE_CURL
