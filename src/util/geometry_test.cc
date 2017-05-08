#include "catch.hpp"

#include <utility>

#include "util/geometry.hh"

TEST_CASE("Rect", "Come on, I can get at least this one right") {
  util::Rect r{100, 100, 50, 100};

  SECTION("Constructor") {
    REQUIRE(r.top_left() == std::make_pair(100, 100));
    REQUIRE(r.bottom_right() == std::make_pair(150, 200));
  }

  SECTION("ExpandBy") {
    r.ExpandBy(50);
    REQUIRE(r.top_left() == std::make_pair(50, 50));
    REQUIRE(r.bottom_right() == std::make_pair(200, 250));
  }

  SECTION("ShrinkBy") {
    // First iteration: shrinks by 10px
    REQUIRE(r.ShrinkBy(10));
    REQUIRE(r.top_left() == std::make_pair(110, 110));
    REQUIRE(r.bottom_right() == std::make_pair(140, 190));

    // Second iteration: shrinks by 10px
    REQUIRE(r.ShrinkBy(10));
    REQUIRE(r.top_left() == std::make_pair(120, 120));
    REQUIRE(r.bottom_right() == std::make_pair(130, 180));

    // Third iteration: can't shrink anymore, doesn't change dimensions
    REQUIRE(!r.ShrinkBy(10));
    REQUIRE(r.top_left() == std::make_pair(120, 120));
    REQUIRE(r.bottom_right() == std::make_pair(130, 180));
  }
}
