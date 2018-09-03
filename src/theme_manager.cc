#include <termios.h>
#include <unistd.h>

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

#include "config.hh"
#include "util/common.hh"
#include "util/fs.hh"
#include "util/log.hh"
#include "util/xdg.hh"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif  // HAVE_CURL

#include "theme_manager.hh"

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
  set              - sets the current theme
  search, s        - searches for matching remotely available themes
  install, in      - installs a theme
  uninstall, rm    - uninstalls a theme
  list-local, ls   - lists locally installed themes

The "list-local" operation expects no arguments. If given, they are ignored.

The "search" operation expects any number of arguments which will be used to
match against remotely available themes. No arguments results in listing all
available ones.

The remaining operations require a sequence of queries as their arguments,
which will match against the theme author name or theme name as substrings,
or will match literally if given in the form "author_name/theme_name".
)EOF";
  return 1;
}

template <typename First>
constexpr bool StrAnyOf(absl::string_view str, First&& arg) {
  return str == arg;
}

template <typename First, typename... Other>
constexpr bool StrAnyOf(absl::string_view str, First&& arg, Other&&... args) {
  return str == arg || StrAnyOf(str, args...);
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

bool TermiosYesNoConfirmation(absl::string_view prompt, bool* result) {
  struct termios original;
  if (tcgetattr(STDIN_FILENO, &original) == -1) {
    util::log::Error() << "tcgetattr(): " << std::strerror(errno) << '\n';
    return false;
  }

  struct termios tweaked = original;
  tweaked.c_lflag &= ~ICANON;
  tweaked.c_lflag &= ~ECHO;
  tweaked.c_cc[VMIN] = 1;
  tweaked.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &tweaked) == -1) {
    util::log::Error() << "tcsetattr(): " << std::strerror(errno) << '\n';
    return false;
  }

  char c = '\0';
  while (true) {
    std::cout << prompt << " [y/n] " << std::flush;

    if (read(STDIN_FILENO, &c, 1) == -1) {
      util::log::Error() << "read(): " << std::strerror(errno) << '\n';
      return false;
    }

    // ~ICANON and ~ECHO also have the effect of swallowing the enter key,
    // so let's go to the newline to ensure we don't mess up the output
    // formatting.
    std::cout << '\n';

    if (absl::ascii_tolower(c) == 'y') break;
    if (absl::ascii_tolower(c) == 'n') break;
    util::log::Error() << "Unrecognized entry '" << c
                       << "', please use 'y' or 'n'.\n";
  }

  if (tcsetattr(STDIN_FILENO, TCSANOW, &original) == -1) {
    util::log::Error() << "tcgetattr(): " << std::strerror(errno) << '\n';
    return false;
  }

  *result = (c == 'y');
  return true;
}

bool FallbackYesNoConfirmation(absl::string_view prompt, bool* result) {
  while (true) {
    std::cout << prompt << " [y/n] " << std::flush;

    char c;
    if (!(std::cin >> c)) {
      return false;
    }
    if (absl::ascii_tolower(c) == 'y') return true;
    if (absl::ascii_tolower(c) == 'n') return false;
    util::log::Error() << "Unrecognized entry '" << c
                       << "', please use 'y' or 'n'.\n";
  }
}

bool UserConfirmation(absl::string_view prompt) {
  bool result = false;
  if (TermiosYesNoConfirmation(prompt, &result)) return result;
  if (FallbackYesNoConfirmation(prompt, &result)) return result;

  util::log::Error() << "Failed reading input, assuming 'n'.\n";
  return false;
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
#endif  // HAVE_CURL

ThemeInfo::ThemeInfo(std::string author, std::string name, unsigned int version)
    : author{author}, name{name}, version{version} {}

std::string ThemeInfo::ToString() const {
  return absl::StrFormat("%s/%s (v%d)", author, name, version);
}

bool ThemeInfo::operator==(ThemeInfo const& other) const {
  return author == other.author && name == other.name &&
         version == other.version;
}

bool ThemeInfo::operator<(ThemeInfo const& other) const {
  return author < other.author || name < other.name || version < other.version;
}

std::ostream& operator<<(std::ostream& os, ThemeInfo const& theme_info) {
  return os << theme_info.ToString();
}

Repository Repository::FromJSON(std::string const& content) {
  return Repository(content);
}

std::string Repository::Dump() const { return repository_.dump(2); }

void Repository::AddTheme(ThemeInfo const& theme_info) {
  json theme_entry = {{"name", theme_info.name},
                      {"version", theme_info.version}};

  for (auto& entry : repository_) {
    if (entry["author"] != theme_info.author) continue;
    for (auto& theme : entry["themes"]) {
      if (theme["name"] != theme_info.name) continue;
      // Author and theme entry exist. Just update version tag.
      theme["version"] = theme_info.version;
      return;
    }
    // Author exists, theme does not. Add new theme entry.
    entry["themes"] += theme_entry;
    return;
  }

  // Author does not exist. Add new author entry and inner theme entry.
  repository_ += {{"author", theme_info.author}, {"themes", {theme_entry}}};
}

Repository::Repository(std::string const& content) {
  repository_ = json::parse(content);
}

#ifdef HAVE_CURL
template <typename Matcher, typename Callback>
bool ForEachMatchingRemoteTheme(CURL* c, Matcher&& match, Callback&& callback) {
  std::string content;
  if (!curl::FetchURL(c, kRepositoryFileURL, &content)) {
    util::log::Error() << "Failed fetching the remote JSON file!\n";
    return false;
  }

  auto repo = Repository::FromJSON(content);
  bool found_any = false;
  repo.ForEach([&](ThemeInfo const& theme_info) {
    if (match(theme_info)) {
      found_any = true;
      callback(theme_info);
    }
  });
  return found_any;
}

int Search(absl::Span<char* const> needles) {
  CURL* c = curl_easy_init();
  if (c == nullptr) {
    util::log::Error() << "Failed initializing CURL.\n";
    return 1;
  }
  ABSL_ATTRIBUTE_UNUSED auto cleanup_curl =
      util::MakeScopedCallback([=] { curl_easy_cleanup(c); });

  auto match = [&](ThemeInfo const& theme_info) {
    auto single_query_matches =
        std::bind(QueryMatches, theme_info.author, theme_info.name,
                  std::placeholders::_1);
    return absl::c_all_of(needles, single_query_matches);
  };

  static auto print_theme_info = [](ThemeInfo const& theme_info) {
    std::cout << theme_info << '\n';
  };

  if (!ForEachMatchingRemoteTheme(c, match, print_theme_info)) {
    util::log::Error() << "No remote themes matched your query.\n";
    return 1;
  }

  return 0;
}

int Install(absl::Span<char* const> theme_queries) {
  if (theme_queries.empty()) {
    util::log::Error() << "You must provide at least one theme name/author "
                          "query in order to install the matching themes.\n";
    return 1;
  }

  CURL* c = curl_easy_init();
  if (c == nullptr) {
    util::log::Error() << "Failed initializing CURL.\n";
    return 1;
  }
  ABSL_ATTRIBUTE_UNUSED auto cleanup_curl =
      util::MakeScopedCallback([=] { curl_easy_cleanup(c); });

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
  auto match = [&](ThemeInfo const& theme_info) {
    auto single_query_matches =
        std::bind(QueryMatches, theme_info.author, theme_info.name,
                  std::placeholders::_1);
    return absl::c_any_of(theme_queries, single_query_matches);
  };

  auto install_theme = [&](ThemeInfo const& theme_info) {
    auto remote_file_name = absl::StrReplaceAll(
        kRepositoryThemeTemplateURL,
        {{"$author", theme_info.author}, {"$theme", theme_info.name}});
    std::string theme_content;
    if (!curl::FetchURL(c, remote_file_name, &theme_content)) {
      util::log::Error() << "Failed fetching the remote theme contents from \""
                         << remote_file_name << "\"!\n";
      return;
    }

    std::cout << "Installing " << theme_info << "...\n";

    auto local_file_name =
        FormatLocalFileName(theme_info.author, theme_info.name);
    if (!util::fs::WriteFile(local_repo_dir / local_file_name, theme_content)) {
      util::log::Error() << "Failed writing the theme contents locally to \""
                         << local_file_name << "\"!\n";
      return;
    }

    local_repo.AddTheme(theme_info);
  };

  if (!ForEachMatchingRemoteTheme(c, match, install_theme)) {
    util::log::Error() << "No remote themes matched your query.\n";
    return 1;
  }

  if (!util::fs::WriteFile(local_repo_path, local_repo.Dump())) {
    util::log::Error()
        << "Failed dumping the updated local theme repository to \""
        << local_repo_path << "\".\n";
    return 1;
  }

  return 0;
}
#else   // HAVE_CURL
int Search(absl::Span<char* const> /*theme_queries*/) {
  util::log::Error() << "tint3 was compiled without CURL: functionalities "
                        "depending on remote assets cannot work.\n";
  return 1;
}

int Install(absl::Span<char* const> /*theme_queries*/) {
  util::log::Error() << "tint3 was compiled without CURL: functionalities "
                        "depending on remote assets cannot work.\n";
  return 1;
}
#endif  // HAVE_CURL

int SetTheme(absl::Span<char* const> theme_queries) {
  if (theme_queries.empty()) {
    util::log::Error() << "You must provide at least one theme name/author "
                          "query in order to set the matching theme.\n";
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
  auto match = [&](ThemeInfo const& theme_info) {
    auto single_query_matches =
        std::bind(QueryMatches, theme_info.author, theme_info.name,
                  std::placeholders::_1);
    return absl::c_any_of(theme_queries, single_query_matches);
  };

  std::set<ThemeInfo> matching_themes;
  local_repo.ForEach([&](ThemeInfo const& theme_info) {
    if (match(theme_info)) matching_themes.emplace(theme_info);
  });

  if (matching_themes.empty()) {
    util::log::Error() << "No themes matched your query.\n";
    return 1;
  }

  if (matching_themes.size() != 1) {
    util::log::Error() << "More than one theme matched your query. Please use "
                          "the literal `author_name/theme_name` format or a "
                          "more restrictive partial query.\n";
    return 1;
  }

  auto& theme_info = *matching_themes.begin();
  std::cout << "Setting current theme to " << theme_info << "...\n";

  util::fs::Path user_config_dir, config_path;
  config::Reader::GetDefaultPaths(&user_config_dir, &config_path);

  if (!util::fs::DirectoryExists(user_config_dir) &&
      !util::fs::CreateDirectory(user_config_dir)) {
    util::log::Error() << "Couldn't create directory \"" << user_config_dir
                       << "\".\n";
    return 1;
  }

  if (util::fs::FileExists(config_path)) {
    if (!util::fs::IsSymbolicLink(config_path)) {
      auto prompt = absl::StrFormat(
          "Target \"%s\" exists, and is not a symbolic link. "
          "Setting the theme now will result in its deletion. Proceed?",
          config_path);
      if (!UserConfirmation(prompt)) {
        return 1;
      }
    }

    if (!util::fs::Unlink(config_path)) {
      util::log::Error() << "Couldn't unlink \"" << config_path << "\".\n";
      return 1;
    }
  }

  auto local_file_name =
      FormatLocalFileName(theme_info.author, theme_info.name);
  auto theme_file_name = local_repo_dir / local_file_name;

  if (!util::fs::SymbolicLink(theme_file_name, config_path)) {
    util::log::Error() << "Couldn't link \"" << config_path << "\" to \""
                       << theme_file_name << "\".\n";
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
  auto match = [&](ThemeInfo const& theme_info) {
    auto single_query_matches =
        std::bind(QueryMatches, theme_info.author, theme_info.name,
                  std::placeholders::_1);
    return absl::c_any_of(theme_queries, single_query_matches);
  };
  auto confirm_deletion = [&](ThemeInfo const& theme_info) {
    auto prompt = absl::StrFormat("Do you really want to delete %s?",
                                  theme_info.ToString());
    return UserConfirmation(prompt);
  };

  bool found_any = local_repo.RemoveMatchingThemes(match, confirm_deletion);
  if (!found_any) {
    util::log::Error() << "No themes matched the provided query.\n";
    return 1;
  }

  if (!util::fs::WriteFile(local_repo_path, local_repo.Dump())) {
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
  repo.ForEach([&](ThemeInfo const& theme_info) {
    auto local_file_name =
        FormatLocalFileName(theme_info.author, theme_info.name);
    if (util::fs::FileExists(local_repo_dir / local_file_name)) {
      std::cout << theme_info << "\n  * " << local_file_name << "\n\n";
    }
  });
  return 0;
}

int ThemeManager(int argc, char* argv[]) {
  if (argc == 2) {
    util::log::Error() << "Error: missing operation.\n";
    return PrintUsage(argv[0]);
  }

  auto arguments = absl::MakeConstSpan(argv + 2, argv + argc);
  if (StrAnyOf(arguments.front(), "help", "h")) {
    PrintUsage(argv[0]);
    return 0;
  } else if (StrAnyOf(arguments.front(), "set")) {
    arguments.remove_prefix(1);
    return SetTheme(arguments);
  } else if (StrAnyOf(arguments.front(), "search", "s")) {
    arguments.remove_prefix(1);
    return Search(arguments);
  } else if (StrAnyOf(arguments.front(), "install", "in")) {
    arguments.remove_prefix(1);
    return Install(arguments);
  } else if (StrAnyOf(arguments.front(), "uninstall", "rm")) {
    arguments.remove_prefix(1);
    return Uninstall(arguments);
  } else if (StrAnyOf(arguments.front(), "list-local", "ls")) {
    return ListLocal();
  }

  util::log::Error() << "Error: unknown operation \"" << arguments.front()
                     << "\".\n\n";
  return PrintUsage(argv[0]);
}
