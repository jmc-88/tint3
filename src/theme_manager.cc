#include <functional>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
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

static const std::string kRepositoryRootURL =
    "https://jmc-88.github.io/tint3-themes";

static const std::string kRepositoryFileURL =
    absl::StrCat(kRepositoryRootURL, "/repository.json");

static const std::string kRepositoryThemeTemplateURL =
    absl::StrCat(kRepositoryRootURL, "/t/$author/$theme/tint3rc");

int PrintUsage(std::string const& argv0) {
  util::log::Error() << "Usage: " << argv0
                     << u8R"EOF( theme <operation> [args...]

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

template <typename First>
bool StrAnyOf(absl::string_view str, First&& arg) {
  return str == arg;
}

template <typename First, typename... Other>
bool StrAnyOf(absl::string_view str, First&& arg, Other&&... args) {
  return str == arg || StrAnyOf(str, args...);
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

  std::string dump() { return repository_.dump(2); }

  void add_theme(std::string author_name, std::string theme_name,
                 unsigned int theme_version) {
    json theme_entry = {{"name", theme_name}, {"version", theme_version}};

    for (auto& entry : repository_) {
      if (entry["author"] != author_name) continue;
      for (auto& theme : entry["themes"]) {
        if (theme["name"] != theme_name) continue;
        // Author and theme entry exist. Just update version tag.
        theme["version"] = theme_version;
        return;
      }
      // Author exists, theme does not. Add new theme entry.
      entry["themes"] += theme_entry;
      return;
    }

    // Author does not exist. Add new author entry and inner theme entry.
    repository_ += {{"author", author_name}, {"themes", {theme_entry}}};
  }

  template <typename Matcher, typename Confirmation>
  bool remove_matching_themes(Matcher&& match,
                              Confirmation&& confirm_deletion) {
    bool found_any = false;

    for (auto& entry : repository_) {
      auto matching_theme = [&](json const& theme) {
        bool is_matching =
            match(entry["author"], theme["name"], theme["version"]);
        found_any = (found_any || is_matching);

        if (is_matching) {
          return confirm_deletion(entry["author"], theme["name"],
                                  theme["version"]);
        }
        return false;
      };

      auto& themes = entry["themes"];
      themes.erase(std::remove_if(themes.begin(), themes.end(), matching_theme),
                   themes.end());
      if (themes.empty()) entry.erase("themes");
    }

    static constexpr auto has_no_themes = [](json const& entry) {
      return entry.find("themes") == entry.end();
    };
    repository_.erase(
        std::remove_if(repository_.begin(), repository_.end(), has_no_themes),
        repository_.end());

    return found_any;
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
  explicit Repository(std::string const& content) {
    repository_ = json::parse(content);
  }

  json repository_;
};

template <typename Matcher, typename Callback>
bool ForEachMatchingRemoteTheme(CURL* c, Matcher&& match, Callback&& callback) {
  std::string content;
  if (!curl::FetchURL(c, kRepositoryFileURL, &content)) {
    util::log::Error() << "Failed fetching the remote JSON file!\n";
    return false;
  }

  auto repo = Repository::FromJSON(content);
  bool found_any = false;
  repo.for_each(
      [&](std::string author, std::string theme, unsigned int version) {
        if (match(author, theme, version)) {
          found_any = true;
          callback(author, theme, version);
        }
      });
  return found_any;
}

bool QueryMatches(absl::string_view author, absl::string_view theme,
                  absl::string_view query) {
  // simple substring query case
  if (absl::StrContains(author, query) || absl::StrContains(theme, query))
    return true;

  // literal "author_name/theme_name" case
  std::vector<std::string> pieces = absl::StrSplit(query, "/");
  if (pieces.size() != 2) return false;

  return author == pieces[0] && theme == pieces[1];
}

std::string NormalizePathComponent(absl::string_view str) {
  return absl::StrReplaceAll(str,
                             {{" ", "_"}, {":", "_"}, {"/", "_"}, {"_", "__"}});
}

std::string FormatLocalFileName(absl::string_view author,
                                absl::string_view theme) {
  return absl::StrFormat("%s_%s.tint3rc", NormalizePathComponent(author),
                         NormalizePathComponent(theme));
}

int Search(CURL* c, absl::Span<char* const> needles) {
  auto match = [&](absl::string_view author, absl::string_view theme,
                   unsigned int version) {
    auto single_query_matches =
        std::bind(QueryMatches, author, theme, std::placeholders::_1);
    return absl::c_all_of(needles, single_query_matches);
  };

  static auto print_theme_name = [](absl::string_view author,
                                    absl::string_view theme,
                                    unsigned int version) {
    std::cout << author << '/' << theme << " (v" << version << ")\n";
  };

  if (!ForEachMatchingRemoteTheme(c, match, print_theme_name)) {
    util::log::Error() << "No remote themes matched your query.\n";
    return 1;
  }

  return 0;
}

int Install(CURL* c, absl::Span<char* const> theme_queries) {
  if (theme_queries.empty()) {
    util::log::Error() << "You must provide at least one theme name/author "
                          "query in order to install the matching themes.\n";
    return 1;
  }

  auto local_repo_dir = util::xdg::basedir::DataHome() / "tint3" / "themes";
  if (!util::fs::DirectoryExists(local_repo_dir) &&
      !util::fs::CreateDirectory(local_repo_dir)) {
    util::log::Error() << "Couldn't create directory \"" << local_repo_dir
                       << "\".\n";
    return 1;
  }

  auto local_repo_path = local_repo_dir / "repository.json";
  std::string local_repo_contents = "[]";
  if (util::fs::FileExists(local_repo_path) &&
      !util::fs::ReadFile(local_repo_path, &local_repo_contents)) {
    util::log::Error() << "Failed reading \"" << local_repo_path << "\".\n";
    return 1;
  }

  auto local_repo = Repository::FromJSON(local_repo_contents);
  auto match = [&](absl::string_view author, absl::string_view theme,
                   unsigned int version) {
    auto single_query_matches =
        std::bind(QueryMatches, author, theme, std::placeholders::_1);
    return absl::c_any_of(theme_queries, single_query_matches);
  };

  auto install_theme = [&](absl::string_view author, absl::string_view theme,
                           unsigned int version) {
    auto remote_file_name = absl::StrReplaceAll(
        kRepositoryThemeTemplateURL, {{"$author", author}, {"$theme", theme}});
    std::string theme_content;
    if (!curl::FetchURL(c, remote_file_name, &theme_content)) {
      util::log::Error() << "Failed fetching the remote theme contents from \""
                         << remote_file_name << "\"!\n";
      return;
    }

    std::cout << "Installing " << author << '/' << theme << "...\n";

    auto local_file_name = FormatLocalFileName(author, theme);
    if (!util::fs::WriteFile(local_repo_dir / local_file_name, theme_content)) {
      util::log::Error() << "Failed writing the theme contents locally to \""
                         << local_file_name << "\"!\n";
      return;
    }

    local_repo.add_theme(std::string(author), std::string(theme), version);
  };

  if (!ForEachMatchingRemoteTheme(c, match, install_theme)) {
    util::log::Error() << "No remote themes matched your query.\n";
    return 1;
  }

  if (!util::fs::WriteFile(local_repo_path, local_repo.dump())) {
    util::log::Error()
        << "Failed dumping the updated local theme repository to \""
        << local_repo_path << "\".\n";
    return 1;
  }

  return 0;
}

int Uninstall(absl::Span<char* const> theme_queries) {
  if (theme_queries.empty()) {
    util::log::Error() << "You must provide at least one theme name/author "
                          "query in order to uninstall the matching themes.\n";
    return 1;
  }

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

  auto local_repo = Repository::FromJSON(local_repo_contents);
  auto match = [&](std::string author, std::string theme,
                   unsigned int version) {
    auto single_query_matches =
        std::bind(QueryMatches, author, theme, std::placeholders::_1);
    return absl::c_any_of(theme_queries, single_query_matches);
  };
  auto confirm_deletion = [&](std::string author, std::string theme,
                              unsigned int version) {
    while (true) {
      std::cout << "Do you really want to delete " << author << '/' << theme
                << " (v" << version << ")? [y/n] ";
      char c;
      if (!(std::cin >> c)) {
        util::log::Error() << "Failed reading from standard input, assuming "
                              "'n'.\n";
        return false;
      }
      if (absl::ascii_tolower(c) == 'y') return true;
      if (absl::ascii_tolower(c) == 'n') return false;
      util::log::Error() << "Unrecognized entry '" << c
                         << "', please use 'y' or 'n'.\n";
    }
  };

  bool found_any = local_repo.remove_matching_themes(match, confirm_deletion);
  if (!found_any) {
    util::log::Error() << "No themes matched the provided query.\n";
    return 1;
  }

  if (!util::fs::WriteFile(local_repo_path, local_repo.dump())) {
    util::log::Error()
        << "Failed dumping the updated local theme repository to \""
        << local_repo_path << "\".\n";
    return 1;
  }

  return 0;
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

  auto repo = Repository::FromJSON(local_repo_contents);
  repo.for_each(
      [&](std::string author, std::string theme, unsigned int version) {
        auto local_file_name = FormatLocalFileName(author, theme);
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
  if (argc == 2) {
    util::log::Error() << "Error: missing operation.\n";
    return PrintUsage(argv[0]);
  }

  // TODO: CURL could be only initialized when a network operation is
  // required; failure to initialize CURL shouldn't prevent "list-local" or
  // "uninstall" from working.
  CURL* c = curl_easy_init();
  if (c == nullptr) {
    util::log::Error() << "Failed initializing CURL.\n";
    return 1;
  }
  ABSL_ATTRIBUTE_UNUSED auto cleanup_curl =
      util::MakeScopedCallback([=] { curl_easy_cleanup(c); });

  auto arguments = absl::MakeConstSpan(argv + 2, argv + argc);
  if (StrAnyOf(arguments.front(), "help", "h")) {
    PrintUsage(argv[0]);
    return 0;
  } else if (StrAnyOf(arguments.front(), "search", "s")) {
    arguments.remove_prefix(1);
    return Search(c, arguments);
  } else if (StrAnyOf(arguments.front(), "install", "in")) {
    arguments.remove_prefix(1);
    return Install(c, arguments);
  } else if (StrAnyOf(arguments.front(), "uninstall", "un")) {
    arguments.remove_prefix(1);
    return Uninstall(arguments);
  } else if (StrAnyOf(arguments.front(), "list-local", "ls")) {
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
