#include "catch.hpp"

#include <limits>

#include "util/color.hh"

TEST_CASE("Color", "SetColorFromHexString") {
  Color c;

  SECTION("Malformed expressions get rejected") {
    REQUIRE(!c.SetColorFromHexString("oh hai"));
    REQUIRE(!c.SetColorFromHexString("ff007f"));
  }

  SECTION("Three-digit values are parsed correctly") {
    REQUIRE(c.SetColorFromHexString("#fff"));
    REQUIRE(c[0] == 1.0);
    REQUIRE(c[1] == 1.0);
    REQUIRE(c[2] == 1.0);
  }

  SECTION("Black is black") {
    REQUIRE(c.SetColorFromHexString("#000000"));
    REQUIRE(c[0] == 0.0);
    REQUIRE(c[1] == 0.0);
    REQUIRE(c[2] == 0.0);
  }

  SECTION("Gray is gray") {
    REQUIRE(c.SetColorFromHexString("#666666"));
    REQUIRE(c[0] == 0.4);
    REQUIRE(c[1] == 0.4);
    REQUIRE(c[2] == 0.4);
  }

  SECTION("White is white") {
    REQUIRE(c.SetColorFromHexString("#FFFFFF"));
    REQUIRE(c[0] == 1.0);
    REQUIRE(c[1] == 1.0);
    REQUIRE(c[2] == 1.0);
  }

  SECTION("Mixed case also works") {
    REQUIRE(c.SetColorFromHexString("#fFffFf"));
    REQUIRE(c[0] == 1.0);
    REQUIRE(c[1] == 1.0);
    REQUIRE(c[2] == 1.0);
  }
}

TEST_CASE("Border", "Copying") {
  Border b1;
  b1.set_color(Color{{{0.25, 0.50, 1.00}}, 0.75});

  Border b2{b1};
  REQUIRE(b2 == b1);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif  // __clang__
  Border b3{std::move(b1)};
  // b1 was moved at this point, check against b2 which was guaranteed to be
  // equal to b1 by the above check
  REQUIRE(b3 == b2);
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__
}

TEST_CASE("Border::set_mask") {
  // Defaults to BORDER_ALL
  Border b;
  REQUIRE(b.mask() == BORDER_ALL);

  // Filters out bits not in the BORDER_ALL mask
  b.set_mask(std::numeric_limits<unsigned int>::max());
  REQUIRE(b.mask() == BORDER_ALL);
}

TEST_CASE("Border::width_for_side") {
  // Defaults to BORDER_ALL
  Border b;
  b.set_width(2);
  REQUIRE(b.width() == 2);
  REQUIRE(b.width_for_side(BORDER_TOP) == 2);
  REQUIRE(b.width_for_side(BORDER_RIGHT) == 2);
  REQUIRE(b.width_for_side(BORDER_BOTTOM) == 2);
  REQUIRE(b.width_for_side(BORDER_LEFT) == 2);

  // Limit it to a single side
  b.set_mask(BORDER_LEFT);
  REQUIRE(b.width() == 2);
  REQUIRE(b.width_for_side(BORDER_TOP) == 0);
  REQUIRE(b.width_for_side(BORDER_RIGHT) == 0);
  REQUIRE(b.width_for_side(BORDER_BOTTOM) == 0);
  REQUIRE(b.width_for_side(BORDER_LEFT) == 2);
}

TEST_CASE("Border::GetInnerAreaRect") {
  // Default: no border
  Border b;
  REQUIRE(b.GetInnerAreaRect(100, 100) == util::Rect({0, 0, 100, 100}));

  // 2px border, on all sides
  b.set_width(2);
  REQUIRE(b.GetInnerAreaRect(100, 100) == util::Rect({2, 2, 96, 96}));

  // 2px border, only on the top and left sides
  b.set_mask(BORDER_TOP | BORDER_LEFT);
  REQUIRE(b.GetInnerAreaRect(100, 100) == util::Rect({2, 2, 98, 98}));
}
