#include "catch.hpp"

#include "panel.hh"
#include "server.hh"

Monitor TestMonitor() {
  Monitor m;
  m.x = 0;
  m.y = 0;
  m.width = 1024;
  m.height = 768;
  m.names = {"test"};
  return m;
}

TEST_CASE("InitSizeAndPosition") {
  server.monitor.push_back(TestMonitor());

  SECTION("size != 0") {
    Panel p;
    p.monitor_ = 0;
    p.width_ = p.height_ = 0;

    p.InitSizeAndPosition();
    REQUIRE(p.width_ != 0);
    REQUIRE(p.height_ != 0);
  }
}
