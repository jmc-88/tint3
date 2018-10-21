#include "catch.hpp"

#include "util/collection.hh"

TEST_CASE("erase") {
  std::vector<int> container = {1, 2, 2, 3, 3, 3, 4, 5};
  auto original_size = container.size();

  SECTION("no matches") {
    erase(container, 42) == container.end();
    REQUIRE(container.size() == original_size);
  }

  SECTION("single match") {
    erase(container, 1) != container.end();
    REQUIRE(container.size() == original_size - 1);
  }

  SECTION("multiple matches") {
    erase(container, 2) != container.end();
    REQUIRE(container.size() == original_size - 2);
  }
}

TEST_CASE("erase_if") {
  std::vector<int> container = {1, 2, 2, 3, 3, 3, 4, 5};
  auto original_size = container.size();

  SECTION("no matches") {
    erase_if(container, [](int) { return false; }) == container.end();
    REQUIRE(container.size() == original_size);
  }

  SECTION("single match") {
    erase_if(container, [](int x) { return x >= 5; }) != container.end();
    REQUIRE(container.size() == original_size - 1);
  }

  SECTION("multiple matches") {
    erase_if(container, [](int x) { return x >= 4; }) != container.end();
    REQUIRE(container.size() == original_size - 2);
  }
}
