#include "catch.hpp"

#include <X11/Xlib.h>

#include "panel.hh"
#include "server.hh"
#include "util/area.hh"
#include "util/environment.hh"
#include "util/timer.hh"

// Area is an abstract class because it has a pure virtual destructor.
// Subclass it here so that we can instantiate it for testing.
class ConcreteArea : public Area {
 public:
  ~ConcreteArea() = default;
};

TEST_CASE("Area::InnermostAreaUnderPoint", "Lookup works correctly") {
  ConcreteArea parent;
  parent.panel_x_ = 0;
  parent.width_ = 200;
  parent.panel_y_ = 0;
  parent.height_ = 100;
  parent.on_screen_ = true;

  // If a point outside the parent area is requested, lookup fails with nullptr.
  REQUIRE(parent.InnermostAreaUnderPoint(300, 300) == nullptr);

  // Trivially, the method should return the area itself if a point inside it is
  // given and there are no children.
  REQUIRE(parent.InnermostAreaUnderPoint(50, 50) == &parent);

  // Add two children.
  //
  //  -------------------------------
  // | parent (200x100)              |
  // |   ----------     ----------   |
  // |  |  child1  |   |  child2  |  |
  // |  |  (80x80) |   |  (80x80) |  |
  // |   ----------     ----------   |
  // |                               |
  //  -------------------------------

  ConcreteArea child1;
  child1.panel_x_ = 10;
  child1.width_ = 80;
  child1.panel_y_ = 10;
  child1.height_ = 80;
  child1.on_screen_ = true;
  parent.AddChild(&child1);

  ConcreteArea child2;
  child2.panel_x_ = 110;
  child2.width_ = 80;
  child2.panel_y_ = 10;
  child2.height_ = 80;
  child2.on_screen_ = true;
  parent.AddChild(&child2);

  // Same lookup as above, but now we have a child that contains that point.
  // Verify that the child Area is returned instead.
  REQUIRE(parent.InnermostAreaUnderPoint(50, 50) == &child1);

  // Test that child2 is found as well, for good measure.
  REQUIRE(parent.InnermostAreaUnderPoint(150, 50) == &child2);

  // A point inside the parent but outside any children should return a pointer
  // to the parent.
  REQUIRE(parent.InnermostAreaUnderPoint(100, 50) == &parent);

  // Add a third child covering the entire parent area, but not currently shown.
  // This should be ignored for InnermostAreaUnderPoint purposes.
  ConcreteArea child3;
  child3.panel_x_ = 0;
  child3.width_ = parent.width_;
  child3.panel_y_ = 0;
  child3.height_ = parent.height_;
  child3.on_screen_ = false;
  parent.AddChild(&child3);

  // Same test as the last one, we expect this to still return a pointer to the
  // parent, since child3 isn't displayed.
  REQUIRE(parent.InnermostAreaUnderPoint(100, 50) == &parent);
}

TEST_CASE("Area::HandlesClick") {
  ConcreteArea area;
  area.panel_x_ = 0;
  area.width_ = 200;
  area.panel_y_ = 0;
  area.height_ = 100;

  SECTION("Click is inside an on-screen Area") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;

    area.on_screen_ = true;
    REQUIRE(area.HandlesClick(&e));
  }

  SECTION("Click is outside an on-screen Area") {
    XEvent e;
    e.xbutton.x = -50;
    e.xbutton.y = -50;

    area.on_screen_ = true;
    REQUIRE_FALSE(area.HandlesClick(&e));
  }

  SECTION("Click is inside an off-screen Area") {
    XEvent e;
    e.xbutton.x = 50;
    e.xbutton.y = 50;

    area.on_screen_ = false;
    REQUIRE_FALSE(area.HandlesClick(&e));
  }

  SECTION("Click is outside an off-screen Area") {
    XEvent e;
    e.xbutton.x = -50;
    e.xbutton.y = -50;

    area.on_screen_ = false;
    REQUIRE_FALSE(area.HandlesClick(&e));
  }
}

class AreaTestFixture {
 public:
  AreaTestFixture() {
    DefaultPanel();

    server.dsp = XOpenDisplay(nullptr);
    if (!server.dsp) {
      FAIL("Couldn't connect to the X server on DISPLAY="
           << environment::Get("DISPLAY"));
    }
    server.InitX11();
    GetMonitors();

    InitPanel(timer_);
  }

  ~AreaTestFixture() {
    CleanupPanel();
    server.Cleanup();
  }

 private:
  Timer timer_;
};

TEST_CASE_METHOD(AreaTestFixture, "Draw") {
  ConcreteArea area;
  area.panel_x_ = 0;
  area.panel_y_ = 0;
  area.panel_ = &panels.at(0);

  SECTION("zero width") {
    area.width_ = 0;
    area.height_ = 100;

    Pixmap old_pixmap = area.pix_;
    area.Draw();
    // Pixmap is not changed: draw is not attempted on a zero-size area
    REQUIRE(area.pix_ == old_pixmap);
  }

  SECTION("zero height") {
    area.width_ = 100;
    area.height_ = 0;

    Pixmap old_pixmap = area.pix_;
    area.Draw();
    // Pixmap is not changed: draw is not attempted on a zero-size area
    REQUIRE(area.pix_ == old_pixmap);
  }
}

// UndrawableArea is an Area that makes the test fail if a Draw() operation is
// attempted on it.
class UndrawableArea : public ConcreteArea {
 public:
  void Draw() override {
    FAIL("this Area should never be drawn");
  }
};

TEST_CASE_METHOD(AreaTestFixture, "Refresh") {
  UndrawableArea area;
  area.on_screen_ = true;
  area.need_redraw_ = true;
  area.panel_x_ = 0;
  area.panel_y_ = 0;
  area.panel_ = &panels.at(0);

  SECTION("zero width") {
    area.width_ = 0;
    area.height_ = 100;
    area.Refresh();  // doesn't trigger FAIL in UndrawableArea
  }

  SECTION("zero height") {
    area.width_ = 100;
    area.height_ = 0;
    area.Refresh();  // doesn't trigger FAIL in UndrawableArea
  }
}
