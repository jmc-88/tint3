#include "catch.hpp"

#include <X11/Xlib.h>

#include "panel.hh"
#include "server.hh"

Monitor TestMonitor() {
  Monitor m;
  m.number = 0;
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
    PanelConfig panel_config;
    panel_config.monitor = 0;
    panel_config.width = 0;
    panel_config.height = 0;

    Panel p;
    p.UseConfig(panel_config, 0);

    p.InitSizeAndPosition();
    REQUIRE(p.width_ != 0);
    REQUIRE(p.height_ != 0);
  }
}

TEST_CASE("SetItemsOrder_Executors") {
  // Add three (empty) executors for testing.
  executors.clear();
  executors.push_back(Executor{});
  executors.push_back(Executor{});
  executors.push_back(Executor{});

  SECTION("add only one") {
    PanelConfig panel_config;
    panel_config.items_order = "E";
    Panel p;
    p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
    p.UseConfig(panel_config, 1);
    p.SetItemsOrder();
    REQUIRE(p.children_.size() == 1);
    REQUIRE(p.children_.at(0) == &executors.at(0));
  }

  SECTION("add only two") {
    PanelConfig panel_config;
    panel_config.items_order = "EE";
    Panel p;
    p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
    p.UseConfig(panel_config, 1);
    p.SetItemsOrder();
    REQUIRE(p.children_.size() == 2);
    REQUIRE(p.children_.at(0) == &executors.at(0));
    REQUIRE(p.children_.at(1) == &executors.at(1));
  }

  SECTION("add all three") {
    PanelConfig panel_config;
    panel_config.items_order = "EEE";
    Panel p;
    p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
    p.UseConfig(panel_config, 1);
    p.SetItemsOrder();
    REQUIRE(p.children_.size() == 3);
    REQUIRE(p.children_.at(0) == &executors.at(0));
    REQUIRE(p.children_.at(1) == &executors.at(1));
    REQUIRE(p.children_.at(2) == &executors.at(2));
  }

  SECTION("add too many") {
    PanelConfig panel_config;
    panel_config.items_order = "EEEE";
    Panel p;
    p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
    p.UseConfig(panel_config, 1);
    p.SetItemsOrder();
    REQUIRE(p.children_.size() == 3);
    REQUIRE(p.children_.at(0) == &executors.at(0));
    REQUIRE(p.children_.at(1) == &executors.at(1));
    REQUIRE(p.children_.at(2) == &executors.at(2));
  }
}

TEST_CASE("HandlesClick_ClickTask") {
  // Make sure we have no executors because that's (still) a global...
  executors.clear();

  PanelConfig panel_config;
  Panel p;
  p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
  p.UseConfig(panel_config, 1);
  p.SetItemsOrder();
  p.num_desktops_ = 1;
  p.on_screen_ = true;

  p.taskbars.resize(1);
  p.taskbars[0].panel_ = &p;
  p.taskbars[0].on_screen_ = true;
  p.taskbars[0].width_ = panel_config.width;
  p.taskbars[0].height_ = panel_config.height;

  Timer test_timer;
  Task test_task{test_timer};
  test_task.on_screen_ = true;
  test_task.width_ = panel_config.width;
  test_task.height_ = panel_config.height;
  p.taskbars[0].AddChild(&test_task);
  p.taskbars[0].Resize();

  p.AddChild(&p.taskbars[0]);
  p.Resize();

  XEvent test_event;
  test_event.xbutton.x = (test_task.width_ / 2);
  test_event.xbutton.y = (test_task.height_ / 2);

  SECTION("left click always handled") {
    test_event.xbutton.button = 1;
    REQUIRE(p.HandlesClick(&test_event));
  }

  SECTION("middle click") {
    test_event.xbutton.button = 2;
    REQUIRE_FALSE(p.HandlesClick(&test_event));

    panel_config.mouse_actions.middle = MouseAction::kToggleIconify;
    p.UseConfig(panel_config, 1);
    REQUIRE(p.HandlesClick(&test_event));
  }

  SECTION("right click") {
    test_event.xbutton.button = 3;
    REQUIRE_FALSE(p.HandlesClick(&test_event));

    panel_config.mouse_actions.right = MouseAction::kToggleIconify;
    p.UseConfig(panel_config, 1);
    REQUIRE(p.HandlesClick(&test_event));
  }

  SECTION("wheel up") {
    test_event.xbutton.button = 4;
    REQUIRE_FALSE(p.HandlesClick(&test_event));

    panel_config.mouse_actions.scroll_up = MouseAction::kToggleIconify;
    p.UseConfig(panel_config, 1);
    REQUIRE(p.HandlesClick(&test_event));
  }

  SECTION("wheel down") {
    test_event.xbutton.button = 5;
    REQUIRE_FALSE(p.HandlesClick(&test_event));

    panel_config.mouse_actions.scroll_down = MouseAction::kToggleIconify;
    p.UseConfig(panel_config, 1);
    REQUIRE(p.HandlesClick(&test_event));
  }
}
