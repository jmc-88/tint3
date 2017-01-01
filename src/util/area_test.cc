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

// Area is an abstract class because it has a pure virtual destructor.
// Subclass it here so that we can instantiate it for testing.
class ConcreteArea : public Area {
 public:
  ~ConcreteArea() {}
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
