#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <cstdlib>
#include <cstring>
#include "util/environment.h"

TEST_CASE("Get", "Reading from the environment is sane") {
  setenv("__BOGUS_NAME__", "__BOGUS_VALUE__", 1);
  REQUIRE(environment::Get("__BOGUS_NAME__") == "__BOGUS_VALUE__");

  unsetenv("__BOGUS_NAME__");
  REQUIRE(environment::Get("__BOGUS_NAME__").empty());
}

TEST_CASE("Set", "Writing to the environment works") {
  REQUIRE(getenv("__BOGUS_NAME__") == nullptr);
  REQUIRE(environment::Set("__BOGUS_NAME__", "__BOGUS_VALUE__"));
  REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__BOGUS_VALUE__") == 0);
}

TEST_CASE("Unset", "Deleting from the environment works") {
  setenv("__BOGUS_NAME__", "__BOGUS_VALUE__", 1);
  REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__BOGUS_VALUE__") == 0);
  REQUIRE(environment::Unset("__BOGUS_NAME__"));
  REQUIRE(getenv("__BOGUS_NAME__") == nullptr);
}

TEST_CASE("ScopedEnvironmentOverride",
          "Temporary environment overrides work as expected") {
  SECTION("An pre-existing variable gets overwritten, then restored") {
    setenv("__BOGUS_NAME__", "__BOGUS_VALUE__", 1);
    REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__BOGUS_VALUE__") == 0);

    {
      environment::ScopedOverride new_value{"__BOGUS_NAME__", "__NEW_VALUE__"};
      REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__NEW_VALUE__") == 0);
    }

    REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__BOGUS_VALUE__") == 0);
  }

  SECTION("A non-existing variable gets set, then unset") {
    unsetenv("__BOGUS_NAME__");
    REQUIRE(getenv("__BOGUS_NAME__") == nullptr);

    {
      environment::ScopedOverride new_value{"__BOGUS_NAME__", "__NEW_VALUE__"};
      REQUIRE(std::strcmp(getenv("__BOGUS_NAME__"), "__NEW_VALUE__") == 0);
    }

    REQUIRE(getenv("__BOGUS_NAME__") == nullptr);
  }
}