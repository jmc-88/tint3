/**************************************************************************
*
* Tint3 : read/write config file
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <Imlib2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <pango/pangoxft.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <wordexp.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include "clock.hh"
#include "config.hh"
#include "launcher/launcher.hh"
#include "panel.hh"
#include "systraybar.hh"
#include "taskbar/task.hh"
#include "taskbar/taskbar.hh"
#include "taskbar/taskbarname.hh"
#include "tooltip/tooltip.hh"
#include "util/common.hh"
#include "util/fs.hh"
#include "util/log.hh"
#include "util/timer.hh"
#include "util/window.hh"
#include "util/xdg.hh"

#ifdef ENABLE_BATTERY
#include "battery/battery.hh"
#endif

namespace {

void GetAction(std::string const& event, MouseAction* action) {
  static std::map<std::string, MouseAction> eventmap{
      {"none", MouseAction::kNone},
      {"close", MouseAction::kClose},
      {"toggle", MouseAction::kToggle},
      {"iconify", MouseAction::kIconify},
      {"shade", MouseAction::kShade},
      {"toggle_iconify", MouseAction::kToggleIconify},
      {"maximize_restore", MouseAction::kMaximizeRestore},
      {"desktop_left", MouseAction::kDesktopLeft},
      {"desktop_right", MouseAction::kDesktopRight},
      {"next_task", MouseAction::kNextTask},
      {"prev_task", MouseAction::kPrevTask},
  };

  auto const& it = eventmap.find(event);

  if (it != eventmap.end()) {
    (*action) = it->second;
  }
}

int GetTaskStatus(std::string const& status) {
  if (status == "active") {
    return kTaskActive;
  }

  if (status == "iconified") {
    return kTaskIconified;
  }

  if (status == "urgent") {
    return kTaskUrgent;
  }

  return kTaskNormal;
}

Background& GetBackgroundFromId(size_t id) {
  try {
    return backgrounds.at(id);
  } catch (std::out_of_range) {
    return backgrounds.front();
  }
}

}  // namespace

namespace config {
namespace {

bool IdentifierMatcher(std::string const& buffer, unsigned int* position,
                       std::string* output) {
  unsigned int begin = (*position);
  if (!isalpha(buffer[*position])) {
    return false;
  }
  unsigned int end = (begin + 1);
  while (end != buffer.length() &&
         (isalnum(buffer[end]) || buffer[end] == '_')) {
    ++end;
  }
  (*position) = end;
  output->assign(buffer, begin, end - begin);
  return true;
}

}  // namespace

const parser::Lexer kLexer{
    std::make_pair(parser::matcher::NewLine, kNewLine),
    std::make_pair(parser::matcher::Whitespace, kWhitespace),
    std::make_pair('#', kPoundSign),
    std::make_pair('=', kEqualsSign),
    std::make_pair(IdentifierMatcher, kIdentifier),
    std::make_pair(parser::matcher::Any, kAny),
};

Parser::Parser(Reader* reader) : reader_(reader) {}

bool Parser::operator()(parser::TokenList* tokens) {
  return ConfigEntryParser(tokens);
}

bool Parser::ConfigEntryParser(parser::TokenList* tokens) {
  tokens->SkipOver(kWhitespace);

  // end of file, stop parsing
  if (tokens->Accept(parser::kEOF)) {
    return true;
  }

  // empty line
  if (tokens->Accept(kNewLine)) {
    return ConfigEntryParser(tokens);
  }

  // comment line
  if (tokens->Current().symbol == kPoundSign) {
    Comment(tokens);
    return ConfigEntryParser(tokens);
  }

  // assignment, or fail
  return Assignment(tokens);
}

bool Parser::Comment(parser::TokenList* tokens) {
  // comment, skip entire line
  if (!tokens->Accept(kPoundSign)) {
    return false;
  }
  tokens->SkipUntil(kNewLine);
  return true;
}

bool Parser::Assignment(parser::TokenList* tokens) {
  tokens->SkipOver(kWhitespace);

  std::string key{tokens->Current().match};
  util::string::ToLowerCase(&key);
  if (!tokens->Accept(kIdentifier)) {
    return false;
  }

  tokens->SkipOver(kWhitespace);

  if (!tokens->Accept(kEqualsSign)) {
    return false;
  }

  tokens->SkipOver(kWhitespace);

  std::vector<parser::Token> skipped;
  if (!tokens->SkipUntil(kNewLine, &skipped) &&
      tokens->Current().symbol != parser::kEOF) {
    return false;
  }

  std::string value{parser::TokenList::JoinSkipped(skipped)};
  util::string::Trim(value);

  if (!AddKeyValue(key, value)) {
    return false;
  }

  // skip over the actual newline
  tokens->Next();

  return ConfigEntryParser(tokens);
}

bool Parser::AddKeyValue(std::string key, std::string value) {
  reader_->AddEntry(key, value);
  return true;
}

void ExtractValues(std::string const& value, std::string& v1, std::string& v2,
                   std::string& v3) {
  v1.clear();
  v2.clear();
  v3.clear();

  size_t first_space = value.find_first_of(' ');
  size_t second_space = std::string::npos;

  v1.assign(value, 0, first_space);
  util::string::Trim(v1);

  if (first_space != std::string::npos) {
    second_space = value.find_first_of(' ', first_space + 1);

    v2.assign(value, first_space + 1, second_space - first_space);
    util::string::Trim(v2);
  }

  if (second_space != std::string::npos) {
    v3.assign(value, second_space + 1, std::string::npos);
    util::string::Trim(v3);
  }
}

std::string ExpandWords(std::string const& line) {
  util::string::Builder sb;
  wordexp_t we;
  if (wordexp(line.c_str(), &we, WRDE_NOCMD | WRDE_UNDEF) == 0) {
    for (char** ptr = we.we_wordv; *ptr != nullptr; ++ptr) {
      if (ptr != we.we_wordv) {
        sb << ' ';
      }
      sb << (*ptr);
    }
    wordfree(&we);
  }
  return sb;
}

Reader::Reader(Server* server) : server_(server), new_config_file_(false) {
  panel_config.mouse_effects = false;
  panel_config.mouse_hover_alpha = 100;
  panel_config.mouse_hover_saturation = 0;
  panel_config.mouse_hover_brightness = 10;
  panel_config.mouse_pressed_alpha = 100;
  panel_config.mouse_pressed_saturation = 0;
  panel_config.mouse_pressed_brightness = -10;
}

bool Reader::LoadFromDefaults() {
  // follow XDG specification
  // check tint3rc in user directory
  auto user_config_dir = util::xdg::basedir::ConfigHome() / "tint3";
  auto config_path = user_config_dir / "tint3rc";

  if (util::fs::FileExists(config_path)) {
    return LoadFromFile(config_path);
  }

  // copy tint3rc from system directory to user directory
  std::string system_config_file;

  for (auto const& system_dir : util::xdg::basedir::ConfigDirs()) {
    system_config_file = util::fs::Path(system_dir) / "tint3" / "tint3rc";

    if (util::fs::FileExists(system_config_file)) {
      break;
    }

    system_config_file.clear();
  }

  if (!system_config_file.empty()) {
    // copy file in user directory
    if (!util::fs::CreateDirectory(user_config_dir)) {
      util::log::Error() << "Couldn't create the \"" << user_config_dir
                         << "\" directory.\n";
      return false;
    }
    if (!util::fs::CopyFile(system_config_file, config_path)) {
      util::log::Error() << "Couldn't copy \"" << system_config_file
                         << "\" to \"" << config_path << "\".\n";
      return false;
    }
    return LoadFromFile(config_path);
  }

  util::log::Error() << "Couldn't find the configuration file.\n";
  return false;
}

bool Reader::LoadFromFile(std::string const& path) {
  bool read = util::fs::ReadFile(path, [=](std::string const& contents) {
    config::Parser config_entry_parser{this};
    parser::Parser p{config::kLexer, &config_entry_parser};
    return p.Parse(contents);
  });

  if (!read) {
    util::log::Error() << "Couldn't read the configuration file.\n";
    return false;
  }

  // append Taskbar item
  if (!new_config_file_) {
    taskbar_enabled = true;
    panel_items_order.insert(0, "T");
  }

  return true;
}

namespace {

Color ParseColor(std::string const& value, double default_alpha = 0.5) {
  std::string value1, value2, value3;
  config::ExtractValues(value, value1, value2, value3);

  Color c;
  c.SetColorFromHexString(value1);

  if (!value2.empty()) {
    c.set_alpha(std::stol(value2) / 100.0);
  } else {
    c.set_alpha(default_alpha);
  }

  return c;
}

}  // namespace

void Reader::AddEntry(std::string const& key, std::string const& value) {
  std::string value1, value2, value3;

  /* Background and border */
  if (key == "rounded") {
    // 'rounded' is the first parameter => alloc a new background
    Background bg;
    bg.border().set_rounded(std::stol(value));
    backgrounds.push_back(bg);
  } else if (key == "border_width") {
    backgrounds.back().border().set_width(std::stol(value));
  } else if (key == "background_color") {
    backgrounds.back().set_fill_color(ParseColor(value));
  } else if (key == "background_color_hover") {
    backgrounds.back().set_fill_color_hover(ParseColor(value));
  } else if (key == "background_color_pressed") {
    backgrounds.back().set_fill_color_pressed(ParseColor(value));
  } else if (key == "border_color") {
    backgrounds.back().border().set_color(ParseColor(value));
  } else if (key == "border_color_hover") {
    backgrounds.back().set_border_color_hover(ParseColor(value));
  } else if (key == "border_color_pressed") {
    backgrounds.back().set_border_color_pressed(ParseColor(value));
  } else if (key == "gradient_id") {
    backgrounds.back().set_gradient_id(std::stol(value));
  } else if (key == "gradient_id_hover") {
    backgrounds.back().set_gradient_id_hover(std::stol(value));
  } else if (key == "gradient_id_pressed") {
    backgrounds.back().set_gradient_id_pressed(std::stol(value));
  }

  /* Gradients */
  else if (key == "gradient") {
    if (value == "vertical") {
      gradients.push_back(util::Gradient{util::GradientKind::kVertical});
    } else if (value == "horizontal") {
      gradients.push_back(util::Gradient{util::GradientKind::kHorizontal});
    } else if (value == "radial") {
      gradients.push_back(util::Gradient{util::GradientKind::kRadial});
    } else {
      util::log::Error() << "unknown gradient kind \"" << value
                         << "\", ignoring\n";
    }
  } else if (key == "start_color") {
    gradients.back().set_start_color(ParseColor(value));
  } else if (key == "end_color") {
    gradients.back().set_end_color(ParseColor(value));
  } else if (key == "color_stop") {
    std::string::size_type first_space = value.find_first_of(' ');
    if (first_space == std::string::npos) {
      util::log::Error() << "malformed color stop \"" << value
                         << "\", ignoring\n";
      return;
    }
    std::string percentage{value, 0, first_space};
    std::string color_spec{value, first_space + 1};
    if (!gradients.back().AddColorStop(std::stol(percentage),
                                       ParseColor(color_spec))) {
      util::log::Error() << "malformed color stop \"" << value
                         << "\", ignoring\n";
    }
  }

  /* Panel */
  else if (key == "panel_monitor") {
    panel_config.monitor_ = GetMonitor(value);
  } else if (key == "panel_size") {
    config::ExtractValues(value, value1, value2, value3);

    size_t b = value1.find_first_of('%');

    panel_config.percent_x = (b != std::string::npos);
    panel_config.width_ = std::stol(value1.substr(0, b));

    if (panel_config.width_ == 0) {
      // full width mode
      panel_config.width_ = 100;
      panel_config.percent_x = 1;
    }

    if (!value2.empty()) {
      b = value2.find_first_of('%');

      if (b != std::string::npos) {
        b = (b - 1);  // don't parse the '%' character
        panel_config.percent_y = 1;
      }

      panel_config.height_ = std::stol(value2.substr(0, b));
    }
  } else if (key == "panel_items") {
    new_config_file_ = true;
    panel_items_order.assign(value);

    for (char item : panel_items_order) {
      if (item == 'L') {
        launcher_enabled = true;
      }

      if (item == 'T') {
        taskbar_enabled = true;
      }

      if (item == 'B') {
#ifdef ENABLE_BATTERY
        battery_enabled = true;
#else   // ENABLE_BATTERY
        util::log::Error() << "tint3 is built without battery support\n";
#endif  // ENABLE_BATTERY
      }

      if (item == 'S') {
        systray_enabled = true;
      }

      if (item == 'C') {
        clock_enabled = true;
      }
    }
  } else if (key == "panel_margin") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.margin_x_ = std::stol(value1);

    if (!value2.empty()) {
      panel_config.margin_y_ = std::stol(value2);
    }
  } else if (key == "panel_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.padding_x_lr_ = panel_config.padding_x_ = std::stol(value1);

    if (!value2.empty()) {
      panel_config.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.padding_x_ = std::stol(value3);
    }
  } else if (key == "panel_position") {
    config::ExtractValues(value, value1, value2, value3);

    if (value1 == "top") {
      panel_vertical_position = PanelVerticalPosition::kTop;
    } else if (value1 == "bottom") {
      panel_vertical_position = PanelVerticalPosition::kBottom;
    } else {
      panel_vertical_position = PanelVerticalPosition::kCenter;
    }

    if (value2 == "left") {
      panel_horizontal_position = PanelHorizontalPosition::kLeft;
    } else if (value2 == "right") {
      panel_horizontal_position = PanelHorizontalPosition::kRight;
    } else {
      panel_horizontal_position = PanelHorizontalPosition::kCenter;
    }

    panel_horizontal = (value3 != "vertical");
  } else if (key == "font_shadow") {
    panel_config.g_task.font_shadow = std::stol(value);
  } else if (key == "panel_background_id") {
    panel_config.bg_ = GetBackgroundFromId(std::stol(value));
  } else if (key == "wm_menu") {
    wm_menu = std::stol(value);
  } else if (key == "panel_dock") {
    panel_dock = std::stol(value);
  } else if (key == "urgent_nb_of_blink") {
    max_tick_urgent = std::stol(value);
  } else if (key == "panel_layer") {
    if (value == "bottom") {
      panel_layer = PanelLayer::kBottom;
    } else if (value == "top") {
      panel_layer = PanelLayer::kTop;
    } else {
      panel_layer = PanelLayer::kNormal;
    }
  }

  /* Battery */
  else if (key == "battery_low_status") {
#ifdef ENABLE_BATTERY
    battery_low_status = std::stol(value);

    if (battery_low_status < 0 || battery_low_status > 100) {
      battery_low_status = 0;
    }
#endif  // ENABLE_BATTERY
  } else if (key == "battery_low_cmd") {
#ifdef ENABLE_BATTERY
    if (!value.empty()) {
      battery_low_cmd = value;
    }
#endif  // ENABLE_BATTERY
  } else if (key == "bat1_font") {
#ifdef ENABLE_BATTERY
    bat1_font_desc = pango_font_description_from_string(value.c_str());
#endif  // ENABLE_BATTERY
  } else if (key == "bat2_font") {
#ifdef ENABLE_BATTERY
    bat2_font_desc = pango_font_description_from_string(value.c_str());
#endif  // ENABLE_BATTERY
  } else if (key == "battery_font_color") {
#ifdef ENABLE_BATTERY
    panel_config.battery_.font = ParseColor(value);
#endif  // ENABLE_BATTERY
  } else if (key == "battery_padding") {
#ifdef ENABLE_BATTERY
    config::ExtractValues(value, value1, value2, value3);
    panel_config.battery_.padding_x_lr_ = panel_config.battery_.padding_x_ =
        std::stol(value1);

    if (!value2.empty()) {
      panel_config.battery_.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.battery_.padding_x_ = std::stol(value3);
    }
#endif  // ENABLE_BATTERY
  } else if (key == "battery_background_id") {
#ifdef ENABLE_BATTERY
    panel_config.battery_.bg_ = GetBackgroundFromId(std::stol(value));
#endif  // ENABLE_BATTERY
  } else if (key == "battery_hide") {
#ifdef ENABLE_BATTERY
    percentage_hide = std::stol(value);

    if (percentage_hide == 0) {
      percentage_hide = 101;
    }
#endif  // ENABLE_BATTERY
  }

  /* Clock */
  else if (key == "time1_format") {
    if (!new_config_file_) {
      clock_enabled = true;
      panel_items_order.push_back('C');
    }

    if (!value.empty()) {
      time1_format = value;
      clock_enabled = true;
    }
  } else if (key == "time2_format") {
    if (!value.empty()) {
      time2_format = value;
    }
  } else if (key == "time1_font") {
    time1_font_desc = pango_font_description_from_string(value.c_str());
  } else if (key == "time1_timezone") {
    if (!value.empty()) {
      time1_timezone = value;
    }
  } else if (key == "time2_timezone") {
    if (!value.empty()) {
      time2_timezone = value;
    }
  } else if (key == "time2_font") {
    time2_font_desc = pango_font_description_from_string(value.c_str());
  } else if (key == "clock_font_color") {
    panel_config.clock_.font_ = ParseColor(value);
  } else if (key == "clock_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.clock_.padding_x_lr_ = panel_config.clock_.padding_x_ =
        std::stol(value1);

    if (!value2.empty()) {
      panel_config.clock_.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.clock_.padding_x_ = std::stol(value3);
    }
  } else if (key == "clock_background_id") {
    panel_config.clock_.bg_ = GetBackgroundFromId(std::stol(value));
  } else if (key == "clock_tooltip") {
    if (!value.empty()) {
      time_tooltip_format = value;
    }
  } else if (key == "clock_tooltip_timezone") {
    if (!value.empty()) {
      time_tooltip_timezone = value;
    }
  } else if (key == "clock_lclick_command") {
    if (!value.empty()) {
      clock_lclick_command = value;
    }
  } else if (key == "clock_rclick_command") {
    if (!value.empty()) {
      clock_rclick_command = value;
    }
  }

  /* Taskbar */
  else if (key == "taskbar_mode") {
    if (value == "multi_desktop") {
      panel_mode = PanelMode::kMultiDesktop;
    } else {
      panel_mode = PanelMode::kSingleDesktop;
    }
  } else if (key == "taskbar_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.g_taskbar.padding_x_lr_ = panel_config.g_taskbar.padding_x_ =
        std::stol(value1);

    if (!value2.empty()) {
      panel_config.g_taskbar.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.g_taskbar.padding_x_ = std::stol(value3);
    }
  } else if (key == "taskbar_background_id") {
    panel_config.g_taskbar.background[kTaskbarNormal] =
        GetBackgroundFromId(std::stol(value));

    if (panel_config.g_taskbar.background[kTaskbarActive] == Background{}) {
      panel_config.g_taskbar.background[kTaskbarActive] =
          panel_config.g_taskbar.background[kTaskbarNormal];
    }
  } else if (key == "taskbar_active_background_id") {
    panel_config.g_taskbar.background[kTaskbarActive] =
        GetBackgroundFromId(std::stol(value));
  } else if (key == "taskbar_name") {
    taskbarname_enabled = (0 != std::stol(value));
  } else if (key == "taskbar_name_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.g_taskbar.bar_name_.padding_x_lr_ =
        panel_config.g_taskbar.bar_name_.padding_x_ = std::stol(value1);
  } else if (key == "taskbar_name_background_id") {
    panel_config.g_taskbar.background_name[kTaskbarNormal] =
        GetBackgroundFromId(std::stol(value));

    if (panel_config.g_taskbar.background_name[kTaskbarActive] ==
        Background{}) {
      panel_config.g_taskbar.background_name[kTaskbarActive] =
          panel_config.g_taskbar.background_name[kTaskbarNormal];
    }
  } else if (key == "taskbar_name_active_background_id") {
    panel_config.g_taskbar.background_name[kTaskbarActive] =
        GetBackgroundFromId(std::stol(value));
  } else if (key == "taskbar_name_font") {
    taskbarname_font_desc = pango_font_description_from_string(value.c_str());
  } else if (key == "taskbar_name_font_color") {
    taskbarname_font = ParseColor(value);
  } else if (key == "taskbar_name_active_font_color") {
    taskbarname_active_font = ParseColor(value);
  }

  /* Task */
  else if (key == "task_text") {
    panel_config.g_task.text = std::stol(value);
  } else if (key == "task_icon") {
    panel_config.g_task.icon = std::stol(value);
  } else if (key == "task_centered") {
    panel_config.g_task.centered = std::stol(value);
  } else if (key == "task_width") {
    // old parameter : just for backward compatibility
    panel_config.g_task.maximum_width = std::stol(value);
    panel_config.g_task.maximum_height = 30;
  } else if (key == "task_maximum_size") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.g_task.maximum_width = std::stol(value1);
    panel_config.g_task.maximum_height = 30;

    if (!value2.empty()) {
      panel_config.g_task.maximum_height = std::stol(value2);
    }
  } else if (key == "task_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.g_task.padding_x_lr_ = panel_config.g_task.padding_x_ =
        std::stol(value1);

    if (!value2.empty()) {
      panel_config.g_task.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.g_task.padding_x_ = std::stol(value3);
    }
  } else if (key == "task_font") {
    panel_config.g_task.font_desc =
        pango_font_description_from_string(value.c_str());
  } else if (util::string::RegexMatch("task.*_font_color", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    panel_config.g_task.font[status] = ParseColor(value, 1.0);
    panel_config.g_task.config_font_mask |= (1 << status);
  } else if (util::string::RegexMatch("task.*_icon_asb", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    config::ExtractValues(value, value1, value2, value3);
    panel_config.g_task.alpha[status] = std::stol(value1);
    panel_config.g_task.saturation[status] = std::stol(value2);
    panel_config.g_task.brightness[status] = std::stol(value3);
    panel_config.g_task.config_asb_mask |= (1 << status);
  } else if (util::string::RegexMatch("task.*_background_id", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    panel_config.g_task.background[status] =
        GetBackgroundFromId(std::stol(value));
    panel_config.g_task.config_background_mask |= (1 << status);

    if (status == kTaskNormal) {
      panel_config.g_task.bg_ = panel_config.g_task.background[kTaskNormal];
    }
  }
  // "tooltip" is deprecated but here for backwards compatibility
  else if (key == "task_tooltip" || key == "tooltip") {
    panel_config.g_task.tooltip_enabled = std::stol(value);
  }

  /* Systray */
  else if (key == "systray_padding") {
    if (!new_config_file_ && !systray_enabled) {
      systray_enabled = true;
      panel_items_order.push_back('S');
    }

    config::ExtractValues(value, value1, value2, value3);
    systray.padding_x_lr_ = systray.padding_x_ = std::stol(value1);

    if (!value2.empty()) {
      systray.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      systray.padding_x_ = std::stol(value3);
    }
  } else if (key == "systray_background_id") {
    systray.bg_ = GetBackgroundFromId(std::stol(value));
  } else if (key == "systray_sort") {
    if (value == "descending") {
      systray.sort = -1;
    } else if (value == "ascending") {
      systray.sort = 1;
    } else if (value == "left2right") {
      systray.sort = 2;
    } else if (value == "right2left") {
      systray.sort = 3;
    }
  } else if (key == "systray_icon_size") {
    systray_max_icon_size = std::stol(value);
  } else if (key == "systray_icon_asb") {
    config::ExtractValues(value, value1, value2, value3);
    systray.alpha = std::stol(value1);
    systray.saturation = std::stol(value2);
    systray.brightness = std::stol(value3);
  }

  /* Launcher */
  else if (key == "launcher_padding") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.launcher_.padding_x_lr_ = panel_config.launcher_.padding_x_ =
        std::stol(value1);

    if (!value2.empty()) {
      panel_config.launcher_.padding_y_ = std::stol(value2);
    }

    if (!value3.empty()) {
      panel_config.launcher_.padding_x_ = std::stol(value3);
    }
  } else if (key == "launcher_background_id") {
    panel_config.launcher_.bg_ = GetBackgroundFromId(std::stol(value));
  } else if (key == "launcher_icon_size") {
    launcher_max_icon_size = std::stol(value);
  } else if (key == "launcher_item_app") {
    std::string expanded = ExpandWords(value);
    if (expanded.empty()) {
      util::log::Error() << "expansion failed for \"" << value
                         << "\", adding verbatim\n";
      panel_config.launcher_.list_apps_.push_back(value);
    } else {
      panel_config.launcher_.list_apps_.push_back(expanded);
    }
  } else if (key == "launcher_icon_theme") {
    // if XSETTINGS manager running, tint3 use it.
    if (icon_theme_name.empty()) {
      icon_theme_name = value;
    }
  } else if (key == "launcher_icon_asb") {
    config::ExtractValues(value, value1, value2, value3);
    launcher_alpha = std::stol(value1);
    launcher_saturation = std::stol(value2);
    launcher_brightness = std::stol(value3);
  } else if (key == "launcher_tooltip") {
    launcher_tooltip_enabled = std::stol(value);
  }

  /* Tooltip */
  else if (key == "tooltip_show_timeout") {
    int timeout_msec = 1000 * std::stof(value);
    g_tooltip.show_timeout_msec = timeout_msec;
  } else if (key == "tooltip_hide_timeout") {
    int timeout_msec = 1000 * std::stof(value);
    g_tooltip.hide_timeout_msec = timeout_msec;
  } else if (key == "tooltip_padding") {
    config::ExtractValues(value, value1, value2, value3);

    if (!value1.empty()) {
      g_tooltip.paddingx = std::stol(value1);
    }

    if (!value2.empty()) {
      g_tooltip.paddingy = std::stol(value2);
    }
  } else if (key == "tooltip_background_id") {
    g_tooltip.bg = GetBackgroundFromId(std::stol(value));
  } else if (key == "tooltip_font_color") {
    g_tooltip.font_color = ParseColor(value, 0.1);
  } else if (key == "tooltip_font") {
    g_tooltip.font_desc = pango_font_description_from_string(value.c_str());
  }

  /* Mouse actions */
  else if (key == "mouse_middle") {
    GetAction(value, &mouse_middle);
  } else if (key == "mouse_right") {
    GetAction(value, &mouse_right);
  } else if (key == "mouse_scroll_up") {
    GetAction(value, &mouse_scroll_up);
  } else if (key == "mouse_scroll_down") {
    GetAction(value, &mouse_scroll_down);
  } else if (key == "mouse_effects") {
    panel_config.mouse_effects = (0 != std::stol(value));
  } else if (key == "mouse_hover_icon_asb") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.mouse_hover_alpha = std::stol(value1);
    panel_config.mouse_hover_saturation = std::stol(value2);
    panel_config.mouse_hover_brightness = std::stol(value3);
  } else if (key == "mouse_pressed_icon_asb") {
    config::ExtractValues(value, value1, value2, value3);
    panel_config.mouse_pressed_alpha = std::stol(value1);
    panel_config.mouse_pressed_saturation = std::stol(value2);
    panel_config.mouse_pressed_brightness = std::stol(value3);
  }

  /* autohide options */
  else if (key == "autohide") {
    panel_autohide = (0 != std::stol(value));
  } else if (key == "autohide_show_timeout") {
    panel_autohide_show_timeout = 1000 * std::stof(value);
  } else if (key == "autohide_hide_timeout") {
    panel_autohide_hide_timeout = 1000 * std::stof(value);
  } else if (key == "strut_policy") {
    if (value == "follow_size") {
      panel_strut_policy = PanelStrutPolicy::kFollowSize;
    } else if (value == "none") {
      panel_strut_policy = PanelStrutPolicy::kNone;
    } else {
      panel_strut_policy = PanelStrutPolicy::kMinimum;
    }
  } else if (key == "autohide_height") {
    panel_autohide_height = std::stol(value);

    if (panel_autohide_height == 0) {
      // autohide need height > 0
      panel_autohide_height = 1;
    }
  }

  // old config option
  else if (key == "systray") {
    if (!new_config_file_) {
      systray_enabled = (0 != std::stol(value));

      if (systray_enabled) {
        panel_items_order.push_back('S');
      }
    }
  } else if (key == "battery") {
#ifdef ENABLE_BATTERY
    if (!new_config_file_) {
      battery_enabled = (0 != std::stol(value));

      if (battery_enabled) {
        panel_items_order.push_back('B');
      }
    }
#endif  // ENABLE_BATTERY
  } else {
    std::cerr
        << "tint3: invalid option \"" << key
        << "\", please upgrade tint3 or correct your configuration file.\n";
  }
}

unsigned int Reader::GetMonitor(std::string const& monitor_name) const {
  if (monitor_name != "all") {
    std::size_t end;
    unsigned int ret_int = std::stol(monitor_name, &end);

    if (monitor_name[end] != '\0') {
      return (ret_int - 1);
    }

    // monitor specified by name, not by index
    for (unsigned int i = 0; i < server_->num_monitors; ++i) {
      for (auto& name : server_->monitor[i].names) {
        if (name == monitor_name) {
          return i;
        }
      }
    }
  }

  // monitor == "all" or monitor not found or xrandr can't identify monitors
  return Panel::kAllMonitors;
}

}  // namespace config
