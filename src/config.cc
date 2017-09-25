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

}  // namespace

const parser::Lexer kLexer{
    std::make_pair(parser::matcher::NewLine, kNewLine),
    std::make_pair(parser::matcher::Whitespace, kWhitespace),
    std::make_pair('#', kPoundSign),
    std::make_pair('=', kEqualsSign),
    std::make_pair("@import", kImport),
    std::make_pair(IdentifierMatcher, kIdentifier),
    std::make_pair(parser::matcher::Any, kAny),
};

Parser::Parser(Reader* reader, std::string const& path)
    : reader_(reader), current_config_path_(path) {}

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

  // import
  if (tokens->Current().symbol == kImport) {
    return Import(tokens);
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

bool Parser::Import(parser::TokenList* tokens) {
  tokens->Accept(kImport);
  tokens->SkipOver(kWhitespace);

  std::vector<parser::Token> skipped;
  if (!tokens->SkipUntil(kNewLine, &skipped) &&
      tokens->Current().symbol != parser::kEOF) {
    return false;
  }

  tokens->SkipOver(kNewLine);

  std::string path{parser::TokenList::JoinSkipped(skipped)};
  util::string::Trim(&path);

  if (path.empty()) {
    return false;
  }

  // try to expand words
  std::string expanded = ExpandWords(path);
  if (!expanded.empty()) {
    path = expanded;
  }

  if (util::fs::FileExists(path)) {
    util::log::Debug() << "config: importing \"" << path << "\"\n";
    if (!reader_->LoadFromFile(path)) {
      util::log::Error() << "config: failed importing \"" << path
                         << "\", ignoring\n";
    }
  } else if (!util::string::StartsWith(path, "/")) {
    util::log::Debug()
        << "config: import file \"" << path
        << "\" doesn't exist, attempting an import from a relative path\n";
    path = (current_config_path_.DirectoryName() / path);

    util::log::Debug() << "config: importing \"" << path << "\"\n";
    if (!reader_->LoadFromFile(path)) {
      util::log::Error() << "config: failed importing \"" << path
                         << "\", ignoring\n";
    }
  } else {
    util::log::Debug() << "config: import file \"" << path
                       << "\" doesn't exist, ignoring\n";
  }

  return ConfigEntryParser(tokens);
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
  util::string::Trim(&value);

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

void ExtractValues(std::string const& value, std::string* v1, std::string* v2,
                   std::string* v3) {
  v1->clear();
  v2->clear();
  v3->clear();

  size_t first_space = value.find_first_of(' ');
  size_t second_space = std::string::npos;

  v1->assign(value, 0, first_space);
  util::string::Trim(v1);

  if (first_space != std::string::npos) {
    second_space = value.find_first_of(' ', first_space + 1);

    v2->assign(value, first_space + 1, second_space - first_space);
    util::string::Trim(v2);
  }

  if (second_space != std::string::npos) {
    v3->assign(value, second_space + 1, std::string::npos);
    util::string::Trim(v3);
  }
}

Reader::Reader(Server* server) : server_(server), new_config_file_(false) {
  new_panel_config = PanelConfig::Default();
  tooltip_config = TooltipConfig::Default();
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
    config::Parser config_entry_parser{this, path};
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
    new_panel_config.items_order.insert(0, "T");
  }

  return true;
}

namespace {

template <typename T>
bool ParseNumber(std::string const& str, T* ptr) {
  if (!util::string::ToNumber(str, ptr)) {
    util::log::Error() << "invalid number: \"" << str << "\"\n";
    return false;
  }
  return true;
}

bool ParseBoolean(std::string str, bool* value) {
  util::string::Trim(&str);
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);

  if (str == "true" || str == "yes" || str == "on") {
    (*value) = true;
    return true;
  }

  if (str == "false" || str == "no" || str == "off") {
    (*value) = false;
    return true;
  }

  int i;
  if (util::string::ToNumber(str, &i)) {
    (*value) = (i != 0);
    return true;
  }

  return false;
}

Color ParseColor(std::string const& value, double default_alpha = 0.5) {
  std::string value1, value2, value3;
  config::ExtractValues(value, &value1, &value2, &value3);

  Color c;
  c.SetColorFromHexString(value1);
  c.set_alpha(default_alpha);

  if (!value2.empty()) {
    int alpha;
    if (ParseNumber(value2, &alpha)) {
      c.set_alpha(alpha / 100.0);
    }
  }

  return c;
}

unsigned int ParseBorderSides(std::string const& value) {
  unsigned int border_mask = 0;
  for (char c : value) {
    switch (c) {
      case 'T':
        border_mask |= BORDER_TOP;
        break;
      case 'R':
        border_mask |= BORDER_RIGHT;
        break;
      case 'B':
        border_mask |= BORDER_BOTTOM;
        break;
      case 'L':
        border_mask |= BORDER_LEFT;
        break;
    }
  }
  return border_mask;
}

}  // namespace

void Reader::AddEntry(std::string const& key, std::string const& value) {
  if (AddEntry_BackgroundBorder(key, value) || AddEntry_Gradient(key, value) ||
      AddEntry_Panel(key, value) || AddEntry_Battery(key, value) ||
      AddEntry_Clock(key, value) || AddEntry_Taskbar(key, value) ||
      AddEntry_Task(key, value) || AddEntry_Systray(key, value) ||
      AddEntry_Launcher(key, value) || AddEntry_Tooltip(key, value) ||
      AddEntry_Executor(key, value) || AddEntry_Mouse(key, value) ||
      AddEntry_Autohide(key, value) || AddEntry_Legacy(key, value)) {
    return;
  }

  std::cerr << "tint3: invalid option \"" << key
            << "\", please upgrade tint3 or correct your configuration file.\n";
}

bool Reader::AddEntry_BackgroundBorder(std::string const& key,
                                       std::string const& value) {
  if (key == "rounded") {
    int roundness;
    if (!ParseNumber(value, &roundness)) {
      return true;
    }
    // 'rounded' is the first parameter => alloc a new background
    Background bg;
    bg.border().set_rounded(roundness);
    backgrounds.push_back(bg);
    return true;
  }
  if (key == "border_width") {
    int width;
    if (!ParseNumber(value, &width)) {
      return true;
    }
    backgrounds.back().border().set_width(width);
    return true;
  }
  if (key == "background_color") {
    backgrounds.back().set_fill_color(ParseColor(value));
    return true;
  }
  if (key == "background_color_hover") {
    backgrounds.back().set_fill_color_hover(ParseColor(value));
    return true;
  }
  if (key == "background_color_pressed") {
    backgrounds.back().set_fill_color_pressed(ParseColor(value));
    return true;
  }
  if (key == "border_sides") {
    backgrounds.back().border().set_mask(ParseBorderSides(value));
    return true;
  }
  if (key == "border_color") {
    backgrounds.back().border().set_color(ParseColor(value));
    return true;
  }
  if (key == "border_color_hover") {
    backgrounds.back().set_border_color_hover(ParseColor(value));
    return true;
  }
  if (key == "border_color_pressed") {
    backgrounds.back().set_border_color_pressed(ParseColor(value));
    return true;
  }
  if (key == "gradient_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    backgrounds.back().set_gradient_id(id);
    return true;
  }
  if (key == "gradient_id_hover") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    backgrounds.back().set_gradient_id_hover(id);
    return true;
  }
  if (key == "gradient_id_pressed") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    backgrounds.back().set_gradient_id_pressed(id);
    return true;
  }
  return false;
}

bool Reader::AddEntry_Gradient(std::string const& key,
                               std::string const& value) {
  if (key == "gradient") {
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
    return true;
  }
  if (key == "start_color") {
    gradients.back().set_start_color(ParseColor(value));
    return true;
  }
  if (key == "end_color") {
    gradients.back().set_end_color(ParseColor(value));
    return true;
  }
  if (key == "color_stop") {
    std::string::size_type first_space = value.find_first_of(' ');
    if (first_space == std::string::npos) {
      util::log::Error() << "malformed color stop \"" << value
                         << "\", ignoring\n";
      return true;
    }
    std::string percentage_string{value, 0, first_space};
    int percentage;
    if (!ParseNumber(percentage_string, &percentage)) {
      return true;
    }
    std::string color_spec{value, first_space + 1};
    if (!gradients.back().AddColorStop(percentage, ParseColor(color_spec))) {
      util::log::Error() << "malformed color stop \"" << value
                         << "\", ignoring\n";
    }
    return true;
  }

  return false;
}

bool Reader::AddEntry_Panel(std::string const& key, std::string const& value) {
  if (key == "panel_monitor") {
    new_panel_config.monitor = GetMonitor(value);
    return true;
  }
  if (key == "panel_size") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    size_t b = value1.find_first_of('%');
    new_panel_config.percent_x = (b != std::string::npos);

    int width;
    if (!ParseNumber(value1.substr(0, b), &width)) {
      return true;
    }

    new_panel_config.width = width;
    if (new_panel_config.width == 0) {
      // full width mode
      new_panel_config.width = 100;
      new_panel_config.percent_x = true;
    }

    if (!value2.empty()) {
      b = value2.find_first_of('%');

      if (b != std::string::npos) {
        b = (b - 1);  // don't parse the '%' character
        new_panel_config.percent_y = true;
      }

      int height;
      if (!ParseNumber(value2.substr(0, b), &height)) {
        return true;
      }
      new_panel_config.height = height;
    }

    return true;
  }
  if (key == "panel_items") {
    new_config_file_ = true;
    new_panel_config.items_order.assign(value);

    for (char item : new_panel_config.items_order) {
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
    return true;
  }
  if (key == "panel_margin") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    int margin_x;
    if (!ParseNumber(value1, &margin_x)) {
      return true;
    }
    new_panel_config.margin_x = margin_x;

    if (!value2.empty()) {
      int margin_y;
      if (!ParseNumber(value2, &margin_y)) {
        return true;
      }
      new_panel_config.margin_y = margin_y;
    }
    return true;
  }
  if (key == "panel_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    int padding_x_lr;
    if (!ParseNumber(value1, &padding_x_lr)) {
      return true;
    }
    new_panel_config.padding_x_lr = new_panel_config.padding_x = padding_x_lr;

    if (!value2.empty()) {
      int padding_y;
      if (!ParseNumber(value2, &padding_y)) {
        return true;
      }
      new_panel_config.padding_y = padding_y;
    }
    if (!value3.empty()) {
      int padding_x;
      if (!ParseNumber(value3, &padding_x)) {
        return true;
      }
      new_panel_config.padding_x = padding_x;
    }
    return true;
  }
  if (key == "panel_position") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (value1 == "top") {
      new_panel_config.vertical_position = PanelVerticalPosition::kTop;
    } else if (value1 == "bottom") {
      new_panel_config.vertical_position = PanelVerticalPosition::kBottom;
    } else {
      new_panel_config.vertical_position = PanelVerticalPosition::kCenter;
    }

    if (value2 == "left") {
      new_panel_config.horizontal_position = PanelHorizontalPosition::kLeft;
    } else if (value2 == "right") {
      new_panel_config.horizontal_position = PanelHorizontalPosition::kRight;
    } else {
      new_panel_config.horizontal_position = PanelHorizontalPosition::kCenter;
    }

    new_panel_config.horizontal = (value3 != "vertical");
    return true;
  }
  if (key == "font_shadow") {
    int shadow;
    if (!ParseNumber(value, &shadow)) {
      return true;
    }
    panel_config.g_task.font_shadow = shadow;
    return true;
  }
  if (key == "panel_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    new_panel_config.background = GetBackgroundFromId(id);
    return true;
  }
  if (key == "wm_menu") {
    ParseBoolean(value, &new_panel_config.wm_menu);
    return true;
  }
  if (key == "panel_dock") {
    ParseBoolean(value, &new_panel_config.dock);
    return true;
  }
  if (key == "urgent_nb_of_blink") {
    ParseNumber(value, &max_tick_urgent);
    return true;
  }
  if (key == "panel_layer") {
    if (value == "bottom") {
      new_panel_config.layer = PanelLayer::kBottom;
    } else if (value == "top") {
      new_panel_config.layer = PanelLayer::kTop;
    } else {
      new_panel_config.layer = PanelLayer::kNormal;
    }
    return true;
  }

  return false;
}

bool Reader::AddEntry_Battery(std::string const& key,
                              std::string const& value) {
  if (key == "battery_low_status") {
#ifdef ENABLE_BATTERY
    int low_status;
    if (!ParseNumber(value, &low_status)) {
      return true;
    }
    battery_low_status = 0;
    if (low_status >= 0 && low_status <= 100) {
      battery_low_status = low_status;
    }
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "battery_low_cmd") {
#ifdef ENABLE_BATTERY
    if (!value.empty()) {
      battery_low_cmd = value;
    }
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "bat1_font") {
#ifdef ENABLE_BATTERY
    bat1_font_desc = pango_font_description_from_string(value.c_str());
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "bat2_font") {
#ifdef ENABLE_BATTERY
    bat2_font_desc = pango_font_description_from_string(value.c_str());
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "battery_font_color") {
#ifdef ENABLE_BATTERY
    new_panel_config.battery.font = ParseColor(value);
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "battery_padding") {
#ifdef ENABLE_BATTERY
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &new_panel_config.battery.padding_x_lr_)) {
      return true;
    }
    new_panel_config.battery.padding_x_ =
        new_panel_config.battery.padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &new_panel_config.battery.padding_y_)) {
        return true;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &new_panel_config.battery.padding_x_)) {
        return true;
      }
    }
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "battery_background_id") {
#ifdef ENABLE_BATTERY
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    new_panel_config.battery.bg_ = GetBackgroundFromId(id);
#endif  // ENABLE_BATTERY
    return true;
  }
  if (key == "battery_hide") {
#ifdef ENABLE_BATTERY
    if (!ParseNumber(value, &percentage_hide)) {
      return true;
    }
    if (percentage_hide == 0) {
      percentage_hide = 101;
    }
#endif  // ENABLE_BATTERY
    return true;
  }

  return false;
}

bool Reader::AddEntry_Clock(std::string const& key, std::string const& value) {
  if (key == "time1_format") {
    if (!new_config_file_) {
      clock_enabled = true;
      new_panel_config.items_order.push_back('C');
    }
    if (!value.empty()) {
      time1_format = value;
      clock_enabled = true;
    }
    return true;
  }
  if (key == "time2_format") {
    if (!value.empty()) {
      time2_format = value;
    }
    return true;
  }
  if (key == "time1_font") {
    time1_font_desc = pango_font_description_from_string(value.c_str());
    return true;
  }
  if (key == "time1_timezone") {
    if (!value.empty()) {
      time1_timezone = value;
    }
    return true;
  }
  if (key == "time2_timezone") {
    if (!value.empty()) {
      time2_timezone = value;
    }
    return true;
  }
  if (key == "time2_font") {
    time2_font_desc = pango_font_description_from_string(value.c_str());
    return true;
  }
  if (key == "clock_font_color") {
    panel_config.clock()->font_ = ParseColor(value);
    return true;
  }
  if (key == "clock_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &panel_config.clock()->padding_x_lr_)) {
      return true;
    }
    panel_config.clock()->padding_x_ = panel_config.clock()->padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &panel_config.clock()->padding_y_)) {
        return true;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &panel_config.clock()->padding_x_)) {
        return true;
      }
    }
    return true;
  }
  if (key == "clock_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.clock()->bg_ = GetBackgroundFromId(id);
    return true;
  }
  if (key == "clock_tooltip") {
    if (!value.empty()) {
      time_tooltip_format = value;
    }
    return true;
  }
  if (key == "clock_tooltip_timezone") {
    if (!value.empty()) {
      time_tooltip_timezone = value;
    }
    return true;
  }
  if (key == "clock_lclick_command") {
    if (!value.empty()) {
      clock_lclick_command = value;
    }
    return true;
  }
  if (key == "clock_rclick_command") {
    if (!value.empty()) {
      clock_rclick_command = value;
    }
    return true;
  }

  return false;
}

bool Reader::AddEntry_Taskbar(std::string const& key,
                              std::string const& value) {
  if (key == "taskbar_mode") {
    if (value == "multi_desktop") {
      new_panel_config.taskbar_mode = TaskbarMode::kMultiDesktop;
    } else {
      new_panel_config.taskbar_mode = TaskbarMode::kSingleDesktop;
    }
    return true;
  }
  if (key == "taskbar_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &panel_config.g_taskbar.padding_x_lr_)) {
      return true;
    }
    panel_config.g_taskbar.padding_x_ = panel_config.g_taskbar.padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &panel_config.g_taskbar.padding_y_)) {
        return true;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &panel_config.g_taskbar.padding_x_)) {
        return true;
      }
    }
    return true;
  }
  if (key == "taskbar_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.g_taskbar.background[kTaskbarNormal] = GetBackgroundFromId(id);

    if (panel_config.g_taskbar.background[kTaskbarActive] == Background{}) {
      panel_config.g_taskbar.background[kTaskbarActive] =
          panel_config.g_taskbar.background[kTaskbarNormal];
    }
    return true;
  }
  if (key == "taskbar_active_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.g_taskbar.background[kTaskbarActive] = GetBackgroundFromId(id);
    return true;
  }
  if (key == "taskbar_name") {
    ParseBoolean(value, &taskbarname_enabled);
    return true;
  }
  if (key == "taskbar_name_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value, &panel_config.g_taskbar.bar_name.padding_x_lr_)) {
      return true;
    }
    panel_config.g_taskbar.bar_name.padding_x_ =
        panel_config.g_taskbar.bar_name.padding_x_lr_;
    return true;
  }
  if (key == "taskbar_name_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.g_taskbar.background_name[kTaskbarNormal] =
        GetBackgroundFromId(id);

    if (panel_config.g_taskbar.background_name[kTaskbarActive] ==
        Background{}) {
      panel_config.g_taskbar.background_name[kTaskbarActive] =
          panel_config.g_taskbar.background_name[kTaskbarNormal];
    }
    return true;
  }
  if (key == "taskbar_name_active_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.g_taskbar.background_name[kTaskbarActive] =
        GetBackgroundFromId(id);
    return true;
  }
  if (key == "taskbar_name_font") {
    taskbarname_font_desc = pango_font_description_from_string(value.c_str());
    return true;
  }
  if (key == "taskbar_name_font_color") {
    taskbarname_font = ParseColor(value);
    return true;
  }
  if (key == "taskbar_name_active_font_color") {
    taskbarname_active_font = ParseColor(value);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Task(std::string const& key, std::string const& value) {
  if (key == "task_text") {
    ParseBoolean(value, &panel_config.g_task.text);
    return true;
  }
  if (key == "task_icon") {
    ParseBoolean(value, &panel_config.g_task.icon);
    return true;
  }
  if (key == "task_centered") {
    ParseBoolean(value, &panel_config.g_task.centered);
    return true;
  }
  if (key == "task_width") {
    // old parameter: just for backward compatibility
    if (!ParseNumber(value, &panel_config.g_task.maximum_width)) {
      return true;
    }
    panel_config.g_task.maximum_height = 30;
    return true;
  }
  if (key == "task_maximum_size") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &panel_config.g_task.maximum_width)) {
      return true;
    }
    panel_config.g_task.maximum_height = 30;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &panel_config.g_task.maximum_height)) {
        return true;
      }
    }
    return true;
  }
  if (key == "task_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &panel_config.g_task.padding_x_lr_)) {
      return true;
    }
    panel_config.g_task.padding_x_ = panel_config.g_task.padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &panel_config.g_task.padding_y_)) {
        return true;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &panel_config.g_task.padding_x_)) {
        return true;
      }
    }
    return true;
  }
  if (key == "task_font") {
    panel_config.g_task.font_desc =
        pango_font_description_from_string(value.c_str());
    return true;
  }
  if (util::string::RegexMatch("task.*_font_color", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    panel_config.g_task.font[status] = ParseColor(value, 1.0);
    panel_config.g_task.config_font_mask |= (1 << status);
    return true;
  }
  if (util::string::RegexMatch("task.*_icon_asb", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);
    if (!ParseNumber(value1, &panel_config.g_task.alpha[status])) {
      return true;
    }
    if (!ParseNumber(value2, &panel_config.g_task.saturation[status])) {
      return true;
    }
    if (!ParseNumber(value3, &panel_config.g_task.brightness[status])) {
      return true;
    }
    panel_config.g_task.config_asb_mask |= (1 << status);
    return true;
  }
  if (util::string::RegexMatch("task.*_background_id", key)) {
    auto split = util::string::Split(key, '_');
    int status = GetTaskStatus(split[1]);
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.g_task.background[status] = GetBackgroundFromId(id);
    panel_config.g_task.config_background_mask |= (1 << status);

    if (status == kTaskNormal) {
      panel_config.g_task.bg_ = panel_config.g_task.background[kTaskNormal];
    }
    return true;
  }
  // "tooltip" is deprecated but here for backwards compatibility
  if (key == "task_tooltip" || key == "tooltip") {
    ParseBoolean(value, &panel_config.g_task.tooltip_enabled);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Systray(std::string const& key,
                              std::string const& value) {
  if (key == "systray_padding") {
    if (!new_config_file_ && !systray_enabled) {
      systray_enabled = true;
      new_panel_config.items_order.push_back('S');
    }

    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &systray.padding_x_lr_)) {
      return true;
    }
    systray.padding_x_ = systray.padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &systray.padding_y_)) {
        return true;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &systray.padding_x_)) {
        return true;
      }
    }
    return true;
  }
  if (key == "systray_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    systray.bg_ = GetBackgroundFromId(id);
    return true;
  }
  if (key == "systray_sort") {
    if (value == "descending") {
      systray.sort = -1;
    } else if (value == "ascending") {
      systray.sort = 1;
    } else if (value == "left2right") {
      systray.sort = 2;
    } else if (value == "right2left") {
      systray.sort = 3;
    }
    return true;
  }
  if (key == "systray_icon_size") {
    ParseNumber(value, &systray_max_icon_size);
    return true;
  }
  if (key == "systray_icon_asb") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);
    if (!ParseNumber(value1, &systray.alpha)) {
      return true;
    }
    if (!ParseNumber(value2, &systray.saturation)) {
      return true;
    }
    ParseNumber(value3, &systray.brightness);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Launcher(std::string const& key,
                               std::string const& value) {
  if (key == "launcher_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!ParseNumber(value1, &panel_config.launcher_.padding_x_lr_)) {
      return false;
    }
    panel_config.launcher_.padding_x_ = panel_config.launcher_.padding_x_lr_;

    if (!value2.empty()) {
      if (!ParseNumber(value2, &panel_config.launcher_.padding_y_)) {
        return false;
      }
    }
    if (!value3.empty()) {
      if (!ParseNumber(value3, &panel_config.launcher_.padding_x_)) {
        return false;
      }
    }
    return true;
  }
  if (key == "launcher_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    panel_config.launcher_.bg_ = GetBackgroundFromId(id);
    return true;
  }
  if (key == "launcher_icon_size") {
    ParseNumber(value, &launcher_max_icon_size);
    return true;
  }
  if (key == "launcher_item_app") {
    std::string expanded = ExpandWords(value);
    if (expanded.empty()) {
      util::log::Debug() << "expansion failed for \"" << value
                         << "\", adding verbatim\n";
      panel_config.launcher_.list_apps_.push_back(value);
    } else {
      panel_config.launcher_.list_apps_.push_back(expanded);
    }
    return true;
  }
  if (key == "launcher_icon_theme") {
    // if XSETTINGS manager running, tint3 use it.
    if (icon_theme_name.empty()) {
      icon_theme_name = value;
    }
    return true;
  }
  if (key == "launcher_icon_asb") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);
    if (!ParseNumber(value1, &launcher_alpha)) {
      return true;
    }
    if (!ParseNumber(value2, &launcher_saturation)) {
      return true;
    }
    ParseNumber(value3, &launcher_brightness);
    return true;
  }
  if (key == "launcher_tooltip") {
    ParseBoolean(value, &launcher_tooltip_enabled);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Tooltip(std::string const& key,
                              std::string const& value) {
  if (key == "tooltip_show_timeout") {
    float timeout;
    if (!ParseNumber(value, &timeout)) {
      return true;
    }
    tooltip_config.show_timeout_msec = 1000 * timeout;
    return true;
  }
  if (key == "tooltip_hide_timeout") {
    float timeout;
    if (!ParseNumber(value, &timeout)) {
      return true;
    }
    tooltip_config.hide_timeout_msec = 1000 * timeout;
    return true;
  }
  if (key == "tooltip_padding") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);

    if (!value1.empty()) {
      if (!ParseNumber(value1, &tooltip_config.paddingx)) {
        return true;
      }
    }
    if (!value2.empty()) {
      if (!ParseNumber(value2, &tooltip_config.paddingy)) {
        return true;
      }
    }
    return true;
  }
  if (key == "tooltip_background_id") {
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    tooltip_config.bg = GetBackgroundFromId(id);
    return true;
  }
  if (key == "tooltip_font_color") {
    tooltip_config.font_color = ParseColor(value, 0.1);
    return true;
  }
  if (key == "tooltip_font") {
    tooltip_config.font_desc =
        pango_font_description_from_string(value.c_str());
    return true;
  }

  return false;
}

bool Reader::AddEntry_Executor(std::string const& key,
                               std::string const& value) {
  if (key == "execp") {
    if (value != "new") {
      util::log::Error() << "unexpected value \"" << value
                         << "\", for execp, ignoring\n";
      return true;
    }
    executors.push_back(Executor{});
    return true;
  }
  if (key == "execp_background_id") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    int id;
    if (!ParseNumber(value, &id)) {
      return true;
    }
    executors.back().set_background(GetBackgroundFromId(id));
    return true;
  }
  if (key == "execp_cache_icon") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    bool enabled;
    ParseBoolean(value, &enabled);
    executors.back().set_cache_icon(enabled);
    return true;
  }
  if (key == "execp_centered") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    bool enabled;
    ParseBoolean(value, &enabled);
    executors.back().set_centered(enabled);
    return true;
  }
  if (key == "execp_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command(value);
    return true;
  }
  if (key == "execp_continuous") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    bool enabled;
    ParseBoolean(value, &enabled);
    executors.back().set_continuous(enabled);
    return true;
  }
  if (key == "execp_dwheel_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command_down_wheel(value);
    return true;
  }
  if (key == "execp_font") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_font(value);
    return true;
  }
  if (key == "execp_font_color") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_font_color(ParseColor(value));
    return true;
  }
  if (key == "execp_has_icon") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    bool enabled;
    ParseBoolean(value, &enabled);
    executors.back().set_has_icon(enabled);
    return true;
  }
  if (key == "execp_icon_h") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    int height;
    if (!ParseNumber(value, &height)) {
      return true;
    }
    if (height < 0) {
      util::log::Error() << "negative " << key << " given, ignoring\n";
    } else {
      executors.back().set_icon_height(height);
    }
    return true;
  }
  if (key == "execp_icon_w") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    int width;
    if (!ParseNumber(value, &width)) {
      return true;
    }
    if (width < 0) {
      util::log::Error() << "negative " << key << " given, ignoring\n";
    } else {
      executors.back().set_icon_width(width);
    }
    return true;
  }
  if (key == "execp_interval") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    int interval;
    if (!ParseNumber(value, &interval)) {
      return true;
    }
    if (interval < 0) {
      util::log::Error() << "negative " << key << " given, ignoring\n";
    } else {
      executors.back().set_interval(interval);
    }
    return true;
  }
  if (key == "execp_lclick_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command_left_click(value);
    return true;
  }
  if (key == "execp_markup") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    bool enabled;
    ParseBoolean(value, &enabled);
    executors.back().set_markup(enabled);
    return true;
  }
  if (key == "execp_mclick_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command_middle_click(value);
    return true;
  }
  if (key == "execp_rclick_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command_right_click(value);
    return true;
  }
  if (key == "execp_tooltip") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_tooltip(value);
    return true;
  }
  if (key == "execp_uwheel_command") {
    if (executors.empty()) {
      util::log::Error() << key << " config entry without any previous execp "
                                   "plugin initialized, ignoring\n";
      return true;
    }
    executors.back().set_command_up_wheel(value);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Mouse(std::string const& key, std::string const& value) {
  if (key == "mouse_middle") {
    GetAction(value, &mouse_middle);
    return true;
  }
  if (key == "mouse_right") {
    GetAction(value, &mouse_right);
    return true;
  }
  if (key == "mouse_scroll_up") {
    GetAction(value, &mouse_scroll_up);
    return true;
  }
  if (key == "mouse_scroll_down") {
    GetAction(value, &mouse_scroll_down);
    return true;
  }
  if (key == "mouse_effects") {
    ParseBoolean(value, &new_panel_config.mouse_effects);
    return true;
  }
  if (key == "mouse_hover_icon_asb") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);
    if (!ParseNumber(value1, &new_panel_config.mouse_hover_alpha)) {
      return true;
    }
    if (!ParseNumber(value2, &new_panel_config.mouse_hover_saturation)) {
      return true;
    }
    ParseNumber(value3, &new_panel_config.mouse_hover_brightness);
    return true;
  }
  if (key == "mouse_pressed_icon_asb") {
    std::string value1, value2, value3;
    config::ExtractValues(value, &value1, &value2, &value3);
    if (!ParseNumber(value1, &new_panel_config.mouse_pressed_alpha)) {
      return true;
    }
    if (!ParseNumber(value2, &new_panel_config.mouse_pressed_saturation)) {
      return true;
    }
    ParseNumber(value3, &new_panel_config.mouse_pressed_brightness);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Autohide(std::string const& key,
                               std::string const& value) {
  if (key == "autohide") {
    ParseBoolean(value, &new_panel_config.autohide);
    return true;
  }
  if (key == "autohide_show_timeout") {
    float timeout;
    if (!ParseNumber(value, &timeout)) {
      return true;
    }
    new_panel_config.autohide_show_timeout = 1000 * timeout;
    return true;
  }
  if (key == "autohide_hide_timeout") {
    float timeout;
    if (!ParseNumber(value, &timeout)) {
      return true;
    }
    new_panel_config.autohide_hide_timeout = 1000 * timeout;
    return true;
  }
  if (key == "strut_policy") {
    if (value == "follow_size") {
      new_panel_config.strut_policy = PanelStrutPolicy::kFollowSize;
    } else if (value == "none") {
      new_panel_config.strut_policy = PanelStrutPolicy::kNone;
    } else {
      new_panel_config.strut_policy = PanelStrutPolicy::kMinimum;
    }
    return true;
  }
  if (key == "autohide_height") {
    int height;
    if (!ParseNumber(value, &height)) {
      return true;
    }
    new_panel_config.autohide_size_px = std::max(1, height);
    return true;
  }

  return false;
}

bool Reader::AddEntry_Legacy(std::string const& key, std::string const& value) {
  if (key == "systray") {
    if (!new_config_file_) {
      ParseBoolean(value, &systray_enabled);
      if (systray_enabled) {
        new_panel_config.items_order.push_back('S');
      }
      return true;
    }
  }
  if (key == "battery") {
#ifdef ENABLE_BATTERY
    if (!new_config_file_) {
      ParseBoolean(value, &battery_enabled);
      if (battery_enabled) {
        new_panel_config.items_order.push_back('B');
      }
    }
    return true;
#endif  // ENABLE_BATTERY
  }
  if (key == "primary_monitor_first") {
    util::log::Error() << "Ignoring legacy option \"primary_monitor_first\".\n";
    return true;
  }

  return false;
}

unsigned int Reader::GetMonitor(std::string const& monitor_name) const {
  if (monitor_name == "all") {
    return Panel::kAllMonitors;
  }

  // monitor specified by number
  int num_monitor;
  if (ParseNumber(monitor_name, &num_monitor)) {
    return (num_monitor - 1);
  }

  // monitor specified by name
  for (unsigned int i = 0; i < server_->num_monitors; ++i) {
    for (auto& name : server_->monitor[i].names) {
      if (name == monitor_name) {
        return i;
      }
    }
  }

  // monitor not found or XRandR can't identify monitors
  return Panel::kAllMonitors;
}

}  // namespace config
