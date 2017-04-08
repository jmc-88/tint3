#include "catch.hpp"

#include <vector>

#include "util/environment.hh"
#include "util/xdg.hh"

TEST_CASE("ConfigHome", "Overrideable through the environment") {
  environment::ScopedOverride env{"XDG_CONFIG_HOME", "something"};
  REQUIRE(util::xdg::basedir::ConfigHome() == "something");
}

TEST_CASE("DataDirs", "Overrideable through the environment") {
  environment::ScopedOverride env{"XDG_DATA_DIRS", "dir1:dir2"};
  std::vector<std::string> expected_dirs{"dir1", "dir2"};
  REQUIRE(util::xdg::basedir::DataDirs() == expected_dirs);
}

TEST_CASE("ConfigDirs", "Overrideable through the environment") {
  environment::ScopedOverride env{"XDG_CONFIG_DIRS", "dir1:dir2"};
  std::vector<std::string> expected_dirs{"dir1", "dir2"};
  REQUIRE(util::xdg::basedir::ConfigDirs() == expected_dirs);
}
