#include "catch.hpp"

#include <set>

#include "theme_manager.hh"

TEST_CASE("ThemeInfo") {
  ThemeInfo theme_info{"test_author", "test_theme", 1};
  REQUIRE(theme_info.ToString() == "test_author/test_theme (v1)");
}

// As formatted by: nlohmann::json::dump(2)
static const char kEmptyRepositoryJson[] = "[]";

// As formatted by: nlohmann::json::dump(2)
static const char kOneAuthorNoThemesJson[] =
    u8R"EOF([
  {
    "author": "test_author",
    "themes": []
  }
])EOF";

// As formatted by: nlohmann::json::dump(2)
static const char kCompleteRepositoryJson[] =
    u8R"EOF([
  {
    "author": "test_author",
    "themes": [
      {
        "name": "test_theme",
        "version": 1
      }
    ]
  }
])EOF";

TEST_CASE("Repository") {
  static auto confirmation_always_true = [](ThemeInfo const&) { return true; };
  static auto count_all_themes = [](Repository const& repo) {
    size_t count = 0;
    repo.ForEach([&](ThemeInfo const&) { ++count; });
    return count;
  };

  SECTION("FromJSON") {
    std::set<ThemeInfo> expected_set;
    expected_set.emplace("test_author", "test_theme", 1);

    std::set<ThemeInfo> actual_set;
    auto repo = Repository::FromJSON(kCompleteRepositoryJson);
    repo.ForEach(
        [&](ThemeInfo const& theme_info) { actual_set.emplace(theme_info); });

    REQUIRE(actual_set == expected_set);
  }

  SECTION("AddTheme") {
    // Starting from empty repo: creates new entry.
    auto empty_repo = Repository::FromJSON(kEmptyRepositoryJson);
    empty_repo.AddTheme(ThemeInfo{"test_author", "test_theme", 1});
    empty_repo.ForEach([&](ThemeInfo const& theme_info) {
      REQUIRE(theme_info.author == "test_author");
      REQUIRE(theme_info.name == "test_theme");
      REQUIRE(theme_info.version == 1);
    });

    // Starting from empty repo: creates new entry.
    auto only_author_entry = Repository::FromJSON(kOneAuthorNoThemesJson);
    only_author_entry.AddTheme(ThemeInfo{"test_author", "test_theme", 1});
    only_author_entry.ForEach([&](ThemeInfo const& theme_info) {
      REQUIRE(theme_info.author == "test_author");
      REQUIRE(theme_info.name == "test_theme");
      REQUIRE(theme_info.version == 1);
    });

    // Starting from complete repo: updates entry.
    auto complete_repo = Repository::FromJSON(kOneAuthorNoThemesJson);
    complete_repo.AddTheme(ThemeInfo{"test_author", "test_theme", 100});
    complete_repo.ForEach([&](ThemeInfo const& theme_info) {
      REQUIRE(theme_info.author == "test_author");
      REQUIRE(theme_info.name == "test_theme");
      REQUIRE(theme_info.version == 100);
    });
  }

  SECTION("RemoveMatchingThemes") {
    auto repo = Repository::FromJSON(kCompleteRepositoryJson);

    // Before removal: one author entry is present.
    REQUIRE(count_all_themes(repo) == 1);

    // No match with any theme: repository size unaffected.
    repo.RemoveMatchingThemes(
        [](ThemeInfo const& theme_info) {
          return theme_info.name == "you_wont_find_me_here";
        },
        confirmation_always_true);
    REQUIRE(count_all_themes(repo) == 1);

    // Matches with the only available them: repository gets emptied.
    repo.RemoveMatchingThemes(
        [](ThemeInfo const& theme_info) {
          return theme_info.name == "test_theme";
        },
        confirmation_always_true);
    REQUIRE(count_all_themes(repo) == 0);
  }

  SECTION("Dump") {
    auto repo = Repository::FromJSON(kCompleteRepositoryJson);
    REQUIRE(repo.Dump() == kCompleteRepositoryJson);
  }

  // Not tested here:
  //  * ForEach() is extensively employed by the above sections.
}