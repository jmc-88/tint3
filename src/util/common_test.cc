#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <string>
#include <vector>
#include "util/common.hh"

TEST_CASE("StringTrim", "Removing trailing spaces from strings should work") {
  std::string s;

  SECTION("Removes spaces from the left end") {
    s = "      text";
    REQUIRE(util::string::Trim(s) == "text");
  }

  SECTION("Removes spaces from the right end") {
    s = "text      ";
    REQUIRE(util::string::Trim(s) == "text");
  }

  SECTION("Removes spaces from both ends") {
    s = "   text   ";
    REQUIRE(util::string::Trim(s) == "text");
  }
}

TEST_CASE("SplitString",
          "Splitting a string using a character separator should work") {
  std::string s{"This is a string"};
  std::vector<std::string> v{"This", "is", "a", "string"};
  REQUIRE(util::string::Split(s, ' ') == v);
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

TEST_CASE("RegexMatch",
          "Matching a string by a regular expression should work") {
  SECTION("Character classes") {
    REQUIRE(util::string::RegexMatch("[0-9]", "0"));
    REQUIRE(!util::string::RegexMatch("[0-9]", "Z"));
  }

  SECTION("Any numer of occurrences (Kleene star)") {
    REQUIRE(util::string::RegexMatch("test[0-9]*", "test"));
    REQUIRE(util::string::RegexMatch("test[0-9]*", "test0"));
    REQUIRE(util::string::RegexMatch("test[0-9]*", "test01"));
  }

  SECTION("One or more occurrences (Kleene plus)") {
    REQUIRE(!util::string::RegexMatch("test[0-9]+", "test"));
    REQUIRE(util::string::RegexMatch("test[0-9]+", "test0"));
    REQUIRE(util::string::RegexMatch("test[0-9]+", "test01"));
  }

  SECTION("At most one occurrence") {
    REQUIRE(util::string::RegexMatch("test[0-9]?", "test"));
    REQUIRE(util::string::RegexMatch("test[0-9]?", "test0"));
    REQUIRE(!util::string::RegexMatch("test[0-9]?", "test01"));
  }
}
