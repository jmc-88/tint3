#include "catch.hpp"

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
