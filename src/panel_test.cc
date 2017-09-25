#include "catch.hpp"

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
    PanelConfig panel_config = PanelConfig::Default();
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
    PanelConfig panel_config = PanelConfig::Default();
    panel_config.items_order = "E";
    Panel p;
    p.panel_ = &p;  // TODO: this is silly, Area should not read from Panel
    p.UseConfig(panel_config, 1);
    p.SetItemsOrder();
    REQUIRE(p.children_.size() == 1);
    REQUIRE(p.children_.at(0) == &executors.at(0));
  }

  SECTION("add only two") {
    PanelConfig panel_config = PanelConfig::Default();
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
    PanelConfig panel_config = PanelConfig::Default();
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
    PanelConfig panel_config = PanelConfig::Default();
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
