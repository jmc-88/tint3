#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "util/area.hh"

TEST_CASE("Color", "SetColorFromHexString") {
  Color c;

  SECTION("Malformed expressions get rejected") {
    REQUIRE(!c.SetColorFromHexString("oh hai"));
    REQUIRE(!c.SetColorFromHexString("ff007f"));
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
