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
  // Empty string: no matter the separator, results in a vector with an
  // empty string in it.
  REQUIRE(util::string::Split("", '*') == std::vector<std::string>{""});
  // Non-empty string, separator not found: results in a vector with the string
  // itself in it.
  REQUIRE(util::string::Split("test", '*') == std::vector<std::string>{"test"});
  // Non-empty string, separator found: separated as intuitively expected.
  REQUIRE(util::string::Split("This is a string", ' ') ==
          std::vector<std::string>({"This", "is", "a", "string"}));
  // String composed of the separator only: results in a vector with two
  // empty strings in it (the empty left and right halves).
  REQUIRE(util::string::Split("/", '/') == std::vector<std::string>({"", ""}));
  // String with the separator at the end: results in a vector with the left
  // substring, and an empty string.
  REQUIRE(util::string::Split("something//something", '/') ==
          std::vector<std::string>({"something", "", "something"}));
  // String with two continguous separators: results in a vector with the left
  // substring, an empty string, and the right substring.
  REQUIRE(util::string::Split("something//something", '/') ==
          std::vector<std::string>({"something", "", "something"}));
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

TEST_CASE("StartsWith",
          "Checking if a string has a prefix works even in corner cases") {
  // normal case: left string longer
  REQUIRE(util::string::StartsWith("tautology", ""));
  REQUIRE(util::string::StartsWith("left", "le"));

  // corner case: same length
  REQUIRE(util::string::StartsWith("", ""));
  REQUIRE(util::string::StartsWith("same", "same"));

  // corner case: right string longer
  REQUIRE(!util::string::StartsWith("", "tautology"));
  REQUIRE(!util::string::StartsWith("man", "mannequin"));
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
