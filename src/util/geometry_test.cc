#include "catch.hpp"

#include <utility>

#include "util/geometry.hh"

TEST_CASE("Rect", "Come on, I can get at least this one right") {
  util::Rect r{100, 100, 50, 100};

  SECTION("Constructor") {
    REQUIRE(r.top_left() == std::make_pair(100, 100));
    REQUIRE(r.bottom_right() == std::make_pair(150, 200));
  }

  SECTION("operator==()") {
    util::Rect a{0, 0, 20, 20};
    REQUIRE(a == a);

    util::Rect b{0, 0, 20, 20};
    REQUIRE(a == b);

    util::Rect c{10, 10, 30, 30};
    REQUIRE_FALSE(a == c);
  }

  SECTION("Contains") {
    util::Rect inside{100, 100, 20, 20};
    REQUIRE(r.Contains(inside));

    util::Rect outside_left{99, 100, 20, 20};
    REQUIRE_FALSE(r.Contains(outside_left));

    util::Rect outside_top{100, 99, 20, 20};
    REQUIRE_FALSE(r.Contains(outside_top));

    util::Rect outside_right{131, 100, 20, 20};
    REQUIRE_FALSE(r.Contains(outside_right));

    util::Rect outside_bottom{100, 181, 20, 20};
    REQUIRE_FALSE(r.Contains(outside_bottom));
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
    REQUIRE_FALSE(r.ShrinkBy(10));
    REQUIRE(r.top_left() == std::make_pair(120, 120));
    REQUIRE(r.bottom_right() == std::make_pair(130, 180));
  }
}
