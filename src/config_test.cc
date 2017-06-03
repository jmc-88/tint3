#include "catch.hpp"

#include <initializer_list>
#include <string>

#include "clock/clock.hh"  // TODO: decouple from config loading
#include "config.hh"
#include "panel.hh"  // TODO: decouple from config loading
#include "server.hh"
#include "tooltip/tooltip.hh"  // TODO: decouple from config loading
#include "util/color.hh"
#include "util/fs.hh"
#include "util/gradient.hh"
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
    num_monitors = monitor_list.size();

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
  ConfigReader() : config::Reader(&server_) {}

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
  DefaultPanel();  // TODO: decouple from config loading
  DefaultClock();  // TODO: decouple from config loading

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
  CleanupClock(timer);  // TODO: decouple from config loading
  CleanupPanel();       // TODO: decouple from config loading
}

static constexpr char kHoverPressed[] =
    u8R"EOF(
# Background 1
rounded = 0
background_color = #808080 0
border_color = #808080 0

# Background 2
rounded = 1
border_width = 2
background_color = #000000 0
background_color_hover = #666666 0
background_color_pressed = #ffffff 0
border_color = #000000 0
border_color_hover = #666666 0
border_color_pressed = #ffffff 0
)EOF";

TEST_CASE("ConfigParserHoverPressed", "Doesn't choke on hover/pressed states") {
  DefaultPanel();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kHoverPressed));

  REQUIRE(backgrounds.size() == 3);  // default background + 2 custom

  // Background 1: Hover not given, verify it corresponds to normal state
  REQUIRE(backgrounds[1].fill_color_hover() == backgrounds[1].fill_color());
  REQUIRE(backgrounds[1].border_color_hover() == backgrounds[1].border());
  // Background 1: Pressed not given, verify it corresponds to normal state
  REQUIRE(backgrounds[1].fill_color_pressed() == backgrounds[1].fill_color());
  REQUIRE(backgrounds[1].border_color_pressed() == backgrounds[1].border());

  // Background 2: Hover was given, verify it was parsed
  Color expected_hover_color{Color::Array{{0.4, 0.4, 0.4}}, 0.0};
  REQUIRE(backgrounds[2].fill_color_hover() == expected_hover_color);
  REQUIRE(backgrounds[2].border_color_hover() == expected_hover_color);
  // Background 2: Pressed was given, verify it was parsed
  Color expected_pressed_color{Color::Array{{1.0, 1.0, 1.0}}, 0.0};
  REQUIRE(backgrounds[2].fill_color_pressed() == expected_pressed_color);
  REQUIRE(backgrounds[2].border_color_pressed() == expected_pressed_color);
  // Background 2: rounded and border_width parsed
  REQUIRE(backgrounds[2].border().rounded() == 1);
  REQUIRE(backgrounds[2].border().width() == 2);

  CleanupPanel();  // TODO: decouple from config loading
}

static constexpr char kMouseEffects[] =
    u8R"EOF(
mouse_effects = 1
mouse_hover_icon_asb = 100 0 25
mouse_pressed_icon_asb = 100 0 -25
)EOF";

TEST_CASE("ConfigParserMouseEffects", "Mouse effects are correctly read") {
  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  // Before: defaults
  REQUIRE(!panel_config.mouse_effects);
  REQUIRE(panel_config.mouse_hover_alpha == 100);
  REQUIRE(panel_config.mouse_hover_saturation == 0);
  REQUIRE(panel_config.mouse_hover_brightness == 10);
  REQUIRE(panel_config.mouse_pressed_alpha == 100);
  REQUIRE(panel_config.mouse_pressed_saturation == 0);
  REQUIRE(panel_config.mouse_pressed_brightness == -10);

  REQUIRE(p.Parse(kMouseEffects));

  // After: config values
  REQUIRE(panel_config.mouse_effects);
  REQUIRE(panel_config.mouse_hover_alpha == 100);
  REQUIRE(panel_config.mouse_hover_saturation == 0);
  REQUIRE(panel_config.mouse_hover_brightness == 25);
  REQUIRE(panel_config.mouse_pressed_alpha == 100);
  REQUIRE(panel_config.mouse_pressed_saturation == 0);
  REQUIRE(panel_config.mouse_pressed_brightness == -25);
}

static constexpr char kLauncherItemExpansion[] =
    u8R"EOF(
launcher_item_app = ~/tilde_expansion
launcher_item_app = $HOME/var_expansion
launcher_item_app = ${HOME}/braced_var_expansion
launcher_item_app = $IDKFA/failed_expansion
)EOF";

TEST_CASE("ConfigParserLauncherItemExpansion", "Expands shell-like items") {
  DefaultPanel();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kLauncherItemExpansion));

  REQUIRE(panel_config.launcher_.list_apps_.size() == 4);
  REQUIRE(util::fs::HomeDirectory() / "tilde_expansion" ==
          panel_config.launcher_.list_apps_[0]);
  REQUIRE(util::fs::HomeDirectory() / "var_expansion" ==
          panel_config.launcher_.list_apps_[1]);
  REQUIRE(util::fs::HomeDirectory() / "braced_var_expansion" ==
          panel_config.launcher_.list_apps_[2]);
  REQUIRE("$IDKFA/failed_expansion" == panel_config.launcher_.list_apps_[3]);

  CleanupPanel();  // TODO: decouple from config loading
}

static constexpr char kGradients[] =
    u8R"EOF(
rounded = 0
background_color = #000 100
gradient_id = 0
gradient_id_hover = 1
gradient_id_pressed = 2

gradient = vertical
start_color = #fff 0
end_color = #fff 100

gradient = horizontal
start_color = #fff 0
color_stop = 50 #000 50
# Invalid stop percentage: ignored
color_stop = 100 #000 0
end_color = #fff 100
)EOF";

TEST_CASE("ConfigParserGradients", "Accepts gradients") {
  DefaultPanel();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kGradients));

  // Check that the background was parsed and is linked to the gradients
  REQUIRE(backgrounds.size() == 2);
  REQUIRE(backgrounds[1].gradient_id() == 0);
  REQUIRE(backgrounds[1].gradient_id_hover() == 1);
  REQUIRE(backgrounds[1].gradient_id_pressed() == 2);

  // Check that the gradients were parsed as expected
  REQUIRE(gradients.size() == 3);
  REQUIRE(gradients[0] == util::Gradient{});

  util::Gradient first{util::GradientKind::kVertical};
  first.set_start_color({{{1.0, 1.0, 1.0}}, 0.0});
  first.set_end_color({{{1.0, 1.0, 1.0}}, 1.0});
  REQUIRE(gradients[1] == first);

  util::Gradient second{util::GradientKind::kHorizontal};
  second.set_start_color({{{1.0, 1.0, 1.0}}, 0.0});
  second.set_end_color({{{1.0, 1.0, 1.0}}, 1.0});
  // only one color stop added: the second is expected to be rejected
  second.AddColorStop(50, {{{0.0, 0.0, 0.0}}, 0.5});
  REQUIRE(gradients[2] == second);

  CleanupPanel();  // TODO: decouple from config loading
}

static constexpr char kNoNewlineAtEOF[] =
    u8R"EOF(mouse_scroll_down = iconify)EOF";

TEST_CASE("ConfigParserNoNewlineAtEOF") {
  DefaultPanel();  // TODO: decouple from config loading

  test::ConfigReader reader;
  config::Parser config_entry_parser{&reader};
  parser::Parser p{config::kLexer, &config_entry_parser};

  REQUIRE(p.Parse(kNoNewlineAtEOF));

  // Check that the mouse action was parsed correctly
  REQUIRE(mouse_scroll_down == MouseAction::kIconify);

  CleanupPanel();  // TODO: decouple from config loading
}
