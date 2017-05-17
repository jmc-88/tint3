#include "catch.hpp"

#include "launcher/launcher.hh"
#include "panel.hh"

TEST_CASE("Launcher::GetIconSize", "Icon size is computed sensibly") {
  panel_horizontal = true;

  Launcher l;
  l.height_ = 50;
  l.width_ = 50;

  // No padding, no border: 50px
  l.padding_y_ = 0;
  l.bg_.border().set_width(0);
  REQUIRE(l.GetIconSize() == 50);

  // 5px padding, no border: 40px
  l.padding_y_ = 5;
  l.bg_.border().set_width(0);
  REQUIRE(l.GetIconSize() == 40);

  // No padding, 5px border: 40px
  l.padding_y_ = 0;
  l.bg_.border().set_width(5);
  REQUIRE(l.GetIconSize() == 40);

  // 2px padding, 2px border: 42px
  l.padding_y_ = 2;
  l.bg_.border().set_width(2);
  REQUIRE(l.GetIconSize() == 42);
}
