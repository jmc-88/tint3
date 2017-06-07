#include "catch.hpp"

#include "panel.hh"
#include "server.hh"

TEST_CASE("InitSizeAndPosition") {
  server.monitor.push_back(Monitor{
    x : 0,
    y : 0,
    width : 1024,
    height : 768,
    names : {"Test"},
  });

  SECTION("size != 0") {
    Panel p;
    p.width_ = p.height_ = 0;

    p.InitSizeAndPosition();
    REQUIRE(p.width_ != 0);
    REQUIRE(p.height_ != 0);
  }
}
