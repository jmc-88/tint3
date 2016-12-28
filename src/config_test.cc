#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <initializer_list>
#include <string>

#include "clock/clock.hh"  // TODO: decouple from config loading
#include "config.hh"
#include "panel.hh"  // TODO: decouple from config loading
#include "server.hh"
#include "tooltip/tooltip.hh"  // TODO: decouple from config loading
#include "util/timer_test_utils.hh"

using namespace config;

TEST_CASE("ExtractValues",
          "Parsing a line from the configuration file should work") {
  std::string v1, v2, v3;

  SECTION("One value specified") {
    ExtractValues("first_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2.empty());
    REQUIRE(v3.empty());
  }

  SECTION("Two values specified") {
    ExtractValues("first_value second_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3.empty());
  }

  SECTION("Three values specified") {
    ExtractValues("first_value second_value third_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value");
  }

  SECTION("More than three values specified") {
    ExtractValues("first_value second_value third_value and all the rest", v1,
                  v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value and all the rest");
  }
}

namespace test {

class MockServer : public Server {
 public:
  void SetMonitors(std::initializer_list<std::string> const& monitor_list) {
    nb_monitor = monitor_list.size();

    monitor.clear();
    for (auto const& name : monitor_list) {
      Monitor m;
      m.names.push_back(name);
      monitor.push_back(m);
    }
  }
};

class ConfigReader : public config::Reader {
 public:
  ConfigReader() : config::Reader(&server_, false) {}

  MockServer& server() { return server_; }

  int GetMonitor(std::string const& monitor_name) const {
    return config::Reader::GetMonitor(monitor_name);
  }

 private:
  MockServer server_;
};

}  // namespace test

TEST_CASE("GetMonitor", "Accessing named monitors works as expected") {
  test::ConfigReader reader;
  reader.server().SetMonitors({"0", "1", "2"});
  REQUIRE(reader.GetMonitor("0") == 0);
  REQUIRE(reader.GetMonitor("1") == 1);
  REQUIRE(reader.GetMonitor("2") == 2);
  REQUIRE(reader.GetMonitor("all") == -1);
  REQUIRE(reader.GetMonitor("3") == -1);
}

// Taken from: /sample/tint3rc
static constexpr char kConfigFile[] =
    u8R"EOF(
# Tint3 config file
# For information on manually configuring tint3 see https://github.com/jmc-88/tint3

# Background definitions
# ID 1
rounded = 7
border_width = 2
background_color = #000000 60
border_color = #FFFFFF 16

# ID 2
rounded = 5
border_width = 0
background_color = #FFFFFF 40
border_color = #FFFFFF 48

# ID 3
rounded = 5
border_width = 0
background_color = #FFFFFF 16
border_color = #FFFFFF 68

# Panel
panel_monitor = all
panel_position = bottom center horizontal
panel_size = 94% 30
panel_margin = 0 0
panel_padding = 7 0 7
panel_dock = 0
wm_menu = 0
panel_layer = top
panel_background_id = 1

# Panel Autohide
autohide = 0
autohide_show_timeout = 0.3
autohide_hide_timeout = 2
autohide_height = 2
strut_policy = follow_size

# Taskbar
taskbar_mode = single_desktop
taskbar_padding = 2 3 2
taskbar_background_id = 0
taskbar_active_background_id = 0

# Tasks
urgent_nb_of_blink = 8
task_icon = 1
task_text = 1
task_centered = 1
task_maximum_size = 140 35
task_padding = 6 2
task_background_id = 3
task_active_background_id = 2
task_urgent_background_id = 2
task_iconified_background_id = 3
task_tooltip = 0

# Task Icons
task_icon_asb = 70 0 0
task_active_icon_asb = 100 0 0
task_urgent_icon_asb = 100 0 0
task_iconified_icon_asb = 70 0 0

# Fonts
task_font = sans 7
task_font_color = #FFFFFF 68
task_active_font_color = #FFFFFF 83
task_urgent_font_color = #FFFFFF 83
task_iconified_font_color = #FFFFFF 68
font_shadow = 0

# System Tray
systray = 1
systray_padding = 0 4 5
systray_sort = ascending
systray_background_id = 0
systray_icon_size = 16
systray_icon_asb = 70 0 0

# Clock
time1_format = %H:%M
time1_font = sans 8
time2_format = %A %d %B
time2_font = sans 6
clock_font_color = #FFFFFF 74
clock_padding = 1 0
clock_background_id = 0
clock_rclick_command = orage

# Tooltips
tooltip_padding = 2 2
tooltip_show_timeout = 0.7
tooltip_hide_timeout = 0.3
tooltip_background_id = 1
tooltip_font = sans 10
tooltip_font_color = #000000 80

# Mouse
mouse_middle = none
mouse_right = close
mouse_scroll_up = toggle
mouse_scroll_down = iconify

# Battery
battery = 0
battery_low_status = 10
battery_low_cmd = notify-send "battery low"
battery_hide = 98
bat1_font = sans 8
bat2_font = sans 6
battery_font_color = #FFFFFF 74
battery_padding = 1 0
battery_background_id = 0

# End of config)EOF";

TEST_CASE("ConfigParser", "Correctly loads a valid configuration file") {
  DefaultPanel();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kConfigFile));

  CleanupPanel();  // TODO: decouple from config loading
}

static constexpr char kEmptyAssignment[] =
    u8R"EOF(
clock_tooltip =
clock_padding = 10 6
)EOF";

TEST_CASE("ConfigParserEmptyAssignment", "Doesn't choke on empty assignments") {
  DefaultPanel();    // TODO: decouple from config loading
  DefaultClock();    // TODO: decouple from config loading
  DefaultTooltip();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kEmptyAssignment));

  // We expect the tooltip to stay empty -- the parser shouldn't skip the
  // whitespace after the equals sign as that would also skip the newline and
  // consume the following "clock_padding" line.
  REQUIRE(time_tooltip_format.empty());
  // Ensure the clock padding was parsed too.
  REQUIRE(panel_config.clock_.padding_x_lr_ == 10);
  REQUIRE(panel_config.clock_.padding_x_ == 10);
  REQUIRE(panel_config.clock_.padding_y_ == 6);

  FakeClock timer{0};
  CleanupTooltip(timer);  // TODO: decouple from config loading
  CleanupClock(timer);    // TODO: decouple from config loading
  CleanupPanel();         // TODO: decouple from config loading
}
