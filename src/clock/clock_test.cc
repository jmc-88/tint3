#include "catch.hpp"

#include <X11/Xlib.h>

#include "clock/clock.hh"

TEST_CASE("HandlesClick") {
  Clock clock;
  clock.panel_x_ = 0;
  clock.width_ = 200;
  clock.panel_y_ = 0;
  clock.height_ = 100;
  clock.on_screen_ = true;

  SECTION("Click is outside an on-screen Clock") {
    XEvent e;
    e.xbutton.x = -50;
    e.xbutton.y = -50;
    REQUIRE_FALSE(clock.HandlesClick(&e));
  }

  SECTION("Click is inside an off-screen Clock") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;

    clock.on_screen_ = false;
    REQUIRE_FALSE(clock.HandlesClick(&e));
  }

  SECTION("Left click inside, left click action undefined") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;
    e.xbutton.button = 1;

    clock_lclick_command.clear();
    REQUIRE_FALSE(clock.HandlesClick(&e));
  }

  SECTION("Left click inside, left click action defined") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;
    e.xbutton.button = 1;

    clock_lclick_command = "bogus";
    REQUIRE(clock.HandlesClick(&e));
  }

  SECTION("Right click inside, right click action undefined") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;
    e.xbutton.button = 3;

    clock_rclick_command.clear();
    REQUIRE_FALSE(clock.HandlesClick(&e));
  }

  SECTION("Right click inside, right click action defined") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;
    e.xbutton.button = 3;

    clock_rclick_command = "bogus";
    REQUIRE(clock.HandlesClick(&e));
  }
}
