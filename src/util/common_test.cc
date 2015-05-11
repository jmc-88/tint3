#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "util/common.h"
#include <string>
#include <cstdlib>

TEST_CASE("GetEnvironment", "Reading from the environment is sane") {
  setenv("__BOGUS_NAME__", "__BOGUS_VALUE__", 1);
  REQUIRE(GetEnvironment("__BOGUS_NAME__") == "__BOGUS_VALUE__");

  unsetenv("__BOGUS_NAME__");
  REQUIRE(GetEnvironment("__BOGUS_NAME__").empty());
}

TEST_CASE("StringTrim", "Removing trailing spaces from strings should work") {
  std::string s;

  SECTION("Removes spaces from the left end") {
    s = "      text";
    REQUIRE(StringTrim(s) == "text");
  }

  SECTION("Removes spaces from the right end") {
    s = "text      ";
    REQUIRE(StringTrim(s) == "text");
  }

  SECTION("Removes spaces from both ends") {
    s = "   text   ";
    REQUIRE(StringTrim(s) == "text");
  }
}

TEST_CASE("StringToLongInt",
          "Parsing a string containing an integer value should work") {
  char* endptr = nullptr;

  SECTION("A valid string produces no errors") {
    std::string value{"123"};
    REQUIRE(StringToLongInt(value, &endptr) == 123L);
    REQUIRE(*endptr == '\0');
  }

  SECTION("A partially valid string returns a value and an error") {
    std::string value{"123this is not a number!"};
    REQUIRE(StringToLongInt(value, &endptr) == 123L);
    REQUIRE(*endptr == 't');
  }

  SECTION("An entirely invalid string returns a null value and an error") {
    std::string value{"this is not a number!"};
    REQUIRE(StringToLongInt(value, &endptr) == 0L);
    REQUIRE(*endptr == 't');
  }
}

TEST_CASE("StringToFloat",
          "Parsing a string containing a floating point value should work") {
  char* endptr = nullptr;

  SECTION("A valid string produces no errors") {
    std::string value{"1.23"};
    REQUIRE(StringToFloat(value, &endptr) == 1.23f);
    REQUIRE(*endptr == '\0');
  }

  SECTION("A partially valid string returns a value and an error") {
    std::string value{"1.23this is not a number!"};
    REQUIRE(StringToFloat(value, &endptr) == 1.23f);
    REQUIRE(*endptr == 't');
  }

  SECTION("An entirely invalid string returns a null value and an error") {
    std::string value{"this is not a number!"};
    REQUIRE(StringToFloat(value, &endptr) == 0.0f);
    REQUIRE(*endptr == 't');
  }
}

TEST_CASE("SplitString",
          "Splitting a string using a character separator should work") {
  std::string s{"This is a string"};
  std::vector<std::string> v{"This", "is", "a", "string"};
  REQUIRE(SplitString(s, ' ') == v);
}

TEST_CASE("GetColor", "Parsing a CSS hex triplet should work") {
  double rgb[3];

  SECTION("Malformed expressions get rejected") {
    REQUIRE(!GetColor("oh hai", rgb));
    REQUIRE(!GetColor("ff007f", rgb));
  }

  SECTION("Black is black") {
    REQUIRE(GetColor("#000000", rgb));
    REQUIRE(rgb[0] == 0.0);
    REQUIRE(rgb[1] == 0.0);
    REQUIRE(rgb[2] == 0.0);
  }

  SECTION("Gray is gray") {
    REQUIRE(GetColor("#666666", rgb));
    REQUIRE(rgb[0] == 0.4);
    REQUIRE(rgb[1] == 0.4);
    REQUIRE(rgb[2] == 0.4);
  }

  SECTION("White is white") {
    REQUIRE(GetColor("#FFFFFF", rgb));
    REQUIRE(rgb[0] == 1.0);
    REQUIRE(rgb[1] == 1.0);
    REQUIRE(rgb[2] == 1.0);
  }

  SECTION("Mixed case also works") {
    REQUIRE(GetColor("#fFffFf", rgb));
    REQUIRE(rgb[0] == 1.0);
    REQUIRE(rgb[1] == 1.0);
    REQUIRE(rgb[2] == 1.0);
  }
}

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