#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <string>
#include <vector>
#include "util/bimap.hh"

TEST_CASE("insert", "Insertion and lookup both work") {
  util::bimap<int, std::string> bimap;

  bimap.insert(1, "test");
  REQUIRE(bimap.size() == 1);

  bimap.insert(2, "toast");
  REQUIRE(bimap.size() == 2);

  REQUIRE(bimap.left.has(1));
  REQUIRE(bimap.right.has("test"));
  REQUIRE(bimap.left.has(2));
  REQUIRE(bimap.right.has("toast"));
}

TEST_CASE("erase", "Removal works as expected") {
  util::bimap<int, std::string> bimap;
  bimap.insert(1, "test");
  bimap.insert(2, "toast");

  REQUIRE(bimap.left.erase(1));
  REQUIRE(!bimap.left.has(1));
  REQUIRE(!bimap.right.has("test"));

  REQUIRE(bimap.right.erase("toast"));
  REQUIRE(!bimap.left.has(2));
  REQUIRE(!bimap.right.has("toast"));

  REQUIRE(bimap.empty());
}

TEST_CASE("const_iterator", "Iterating works from both sides") {
  util::bimap<int, std::string> bimap;
  std::vector<std::pair<int, std::string>> expected{
    {1, "test"},
    {2, "toast"},
  };

  for (auto& p : expected) {
    bimap.insert(p.first, p.second);
  }

  SECTION("left iteration") {
    auto it = bimap.left.begin();
    REQUIRE(it != bimap.left.end());
    for (auto& p : expected) {
      REQUIRE(it->first == p.first);
      REQUIRE(it->second == p.second);
      ++it;
    }
  }

  SECTION("left iteration with range-based for loop") {
    unsigned int i = 0;
    for (auto& p : bimap.left) {
      REQUIRE(p.first == expected[i].first);
      REQUIRE(p.second == expected[i].second);
      ++i;
    }
    REQUIRE(i == expected.size());
  }

  SECTION("right iteration") {
    auto it = bimap.right.begin();
    REQUIRE(it != bimap.right.end());
    for (auto& p : expected) {
      REQUIRE(it->second == p.first);
      REQUIRE(it->first == p.second);
      ++it;
    }
  }

  SECTION("right iteration with range-based for loop") {
    unsigned int i = 0;
    for (auto& p : bimap.right) {
      REQUIRE(p.first == expected[i].second);
      REQUIRE(p.second == expected[i].first);
      ++i;
    }
    REQUIRE(i == expected.size());
  }
}