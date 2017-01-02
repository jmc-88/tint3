#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <sstream>
#include <string>
#include <utility>

#include "util/environment.hh"
#include "util/fs.hh"
#include "util/log.hh"

TEST_CASE("Path", "Represents a filesystem path string") {
  SECTION("move constructor") {
    auto new_path = []() -> util::fs::Path { return "/move"; };
    util::fs::Path moved_path{std::move(new_path())};
    REQUIRE(moved_path == "/move");
  }

  SECTION("assignment") {
    util::fs::Path test_path{"/"};
    // Assignment from another Path object.
    util::fs::Path other_path{"/path/to/somewhere"};
    test_path = other_path;
    REQUIRE(test_path == "/path/to/somewhere");
    // Assignment from a string.
    test_path = "/path/from/string";
    REQUIRE(test_path == "/path/from/string");
  }

  SECTION("copy and append") {
    util::fs::Path root{"/"};
    util::fs::Path usr{root / "usr"};
    REQUIRE(root == "/");    // root was not modified
    REQUIRE(usr == "/usr");  // usr holds "/usr"
  }

  SECTION("append in place") {
    util::fs::Path root{"/"};
    util::fs::Path usr{root /= "usr"};
    REQUIRE(root == "/usr");  // root was modified
    REQUIRE(usr == root);     // usr now holds the same value as "root"
  }

  SECTION("strip trailing and leading slashes") {
    REQUIRE(util::fs::Path("/usr/share/") == "/usr/share");
    REQUIRE((util::fs::Path("/usr") / "share/") == "/usr/share");
    REQUIRE((util::fs::Path("/usr") / "/share/") == "/usr/share");
  }

  SECTION("output to a stream") {
    util::fs::Path test_path{"/test/path"};
    std::ostringstream oss;
    oss << test_path;
    REQUIRE(test_path == oss.str());
  }
}

TEST_CASE("BuildPath", "Joins the given pieces into a single path string") {
  REQUIRE(util::fs::BuildPath({"/"}) == "/");
  REQUIRE(util::fs::BuildPath({"/", "home", "testusr"}) == "/home/testusr");
  REQUIRE(util::fs::BuildPath({"/", "/home", "/testusr"}) == "/home/testusr");
}

TEST_CASE("DirectoryExists", "Checks for the existence of a directory") {
  // Obviously existing path.
  REQUIRE(util::fs::DirectoryExists("/"));
  // Obviously non-existing path.
  REQUIRE(!util::fs::DirectoryExists("/road/to/nowhere"));
}

TEST_CASE("FileExists", "Checks for the existence of a file") {
  // Obviously existing path.
  REQUIRE(util::fs::FileExists("/proc/self/cmdline"));
  REQUIRE(util::fs::FileExists({"/proc", "self", "cmdline"}));
  // Obviously non-existing path.
  REQUIRE(!util::fs::FileExists("/dev/troll"));
}

TEST_CASE("HomeDirectory", "Returns the user's home path") {
  // Check that it honors the value of $HOME by default.
  std::string home_from_environment = environment::Get("HOME");
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);

  // When $HOME is empty or missing, getpwuid() should give us the same info.
  environment::ScopedOverride unset_home{"HOME", ""};
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);
}
