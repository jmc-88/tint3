#include "catch.hpp"

#include "launcher/launcher.hh"
#include "panel.hh"
#include "util/environment.hh"

TEST_CASE("FindDesktopEntry") {
  constexpr char kTestEntryPath[] =
      "src/launcher/testdata/applications/launcher_test.desktop";

  SECTION("existing file") {
    std::string resolved_path;
    REQUIRE(FindDesktopEntry(kTestEntryPath, &resolved_path));
    REQUIRE(resolved_path == kTestEntryPath);
  }

  SECTION("XDG_DATA_HOME") {
    auto data_home = environment::MakeScopedOverride("XDG_DATA_HOME",
                                                     "src/launcher/testdata");
    std::string resolved_path;
    REQUIRE(FindDesktopEntry("launcher_test.desktop", &resolved_path));
    REQUIRE(resolved_path == kTestEntryPath);
  }

  SECTION("XDG_DATA_DIRS") {
    auto data_home = environment::MakeScopedOverride("XDG_DATA_DIRS",
                                                     "/tmp/bogus_path:"
                                                     "src/launcher/testdata");
    std::string resolved_path;
    REQUIRE(FindDesktopEntry("launcher_test.desktop", &resolved_path));
    REQUIRE(resolved_path == kTestEntryPath);
  }

  SECTION("fail") {
    REQUIRE(!FindDesktopEntry("obviously_wrong_desktop_file.desktop", nullptr));
  }
}

TEST_CASE("Launcher::LoadThemes") {
  auto data_home =
      environment::MakeScopedOverride("HOME", "src/launcher/testdata");
  icon_theme_name = "UnitTestTheme";

  // Should successfully load:
  //  src/launcher/testdata/.icons/UnitTestTheme/index.theme
  Launcher l;
  REQUIRE(l.LoadThemes());
  l.CleanupTheme();
}

TEST_CASE("Launcher::GetIconSize") {
  SECTION("borders") {
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

  SECTION("horizontal") {
    // Horizontal panel, no launcher_icon_size given: the computed icon size
    // should default to the panel height.
    panel_horizontal = true;
    launcher_max_icon_size = 0;

    Panel p;
    p.width_ = 200;
    p.height_ = 50;
    Launcher::InitPanel(&p);

    Launcher const& l = p.launcher_;
    REQUIRE(l.width_ == p.height_);
    REQUIRE(l.height_ == p.height_);
    REQUIRE(l.GetIconSize() == p.height_);
  }

  SECTION("vertical") {
    // Vertical panel, no launcher_icon_size given: the computed icon size
    // should default to the panel width.
    panel_horizontal = false;
    launcher_max_icon_size = 0;

    Panel p;
    p.width_ = 50;
    p.height_ = 200;
    Launcher::InitPanel(&p);

    Launcher const& l = p.launcher_;
    REQUIRE(l.width_ == p.width_);
    REQUIRE(l.height_ == p.width_);
    REQUIRE(l.GetIconSize() == p.width_);
  }
}
