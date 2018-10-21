#ifndef TINT3_THEME_MANAGER_HH

#include "util/collection.hh"

#include "json.hpp"
using nlohmann::json;

struct ThemeInfo {
  ThemeInfo() = delete;
  ThemeInfo(std::string author, std::string name, unsigned int version);

  std::string ToString() const;
  bool operator==(ThemeInfo const& other) const;
  bool operator<(ThemeInfo const& other) const;

  const std::string author;
  const std::string name;
  const unsigned int version;
};

class Repository {
 public:
  Repository() = delete;

  static Repository FromJSON(std::string const& content);

  std::string Dump() const;

  void AddTheme(ThemeInfo const& theme_info);

  template <typename Matcher, typename Confirmation>
  bool RemoveMatchingThemes(Matcher&& match, Confirmation&& confirm_deletion) {
    bool found_any = false;

    for (auto& entry : repository_) {
      auto matching_theme = [&](json const& theme) {
        ThemeInfo theme_info{entry["author"], theme["name"], theme["version"]};
        if (!match(theme_info)) return false;

        found_any = true;
        return confirm_deletion(theme_info);
      };

      auto& themes = entry["themes"];
      erase_if(themes, matching_theme);
      if (themes.empty()) entry.erase("themes");
    }

    static auto has_no_themes = [](json const& entry) {
      return entry.find("themes") == entry.end();
    };
    erase_if(repository_, has_no_themes);

    return found_any;
  }

  template <typename Callback>
  void ForEach(Callback&& callback) const {
    for (auto& entry : repository_) {
      for (auto& theme : entry["themes"]) {
        ThemeInfo theme_info{entry["author"], theme["name"], theme["version"]};
        callback(theme_info);
      }
    }
  }

 private:
  explicit Repository(std::string const& content);

  json repository_;
};

int ThemeManager(int argc, char* argv[]);

#endif  // TINT3_THEME_MANAGER_HH
