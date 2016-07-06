#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <string>
#include "config.h"

using namespace config;

TEST_CASE("ExtractValues",
          "Parsing a line from the configuration file should work") {
  std::string v1, v2, v3;

  SECTION("One value specified") {
    ExtractValues("first_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2.empty());
    REQUIRE(v3.empty());
  }

  SECTION("Two values specified") {
    ExtractValues("first_value second_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3.empty());
  }

  SECTION("Three values specified") {
    ExtractValues("first_value second_value third_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value");
  }

  SECTION("More than three values specified") {
    ExtractValues("first_value second_value third_value and all the rest", v1,
                  v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value and all the rest");
  }
}