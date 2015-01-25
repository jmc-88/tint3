/**************************************************************************
*
* Tint3 : clock
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
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

#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "server.h"
#include "panel.h"
#include "clock/clock.h"
#include "util/common.h"
#include "util/timer.h"
#include "util/window.h"

std::string time1_format;
std::string time1_timezone;
std::string time2_format;
std::string time2_timezone;
std::string time_tooltip_format;
std::string time_tooltip_timezone;
std::string clock_lclick_command;
std::string clock_rclick_command;
struct timeval time_clock;
PangoFontDescription* time1_font_desc;
PangoFontDescription* time2_font_desc;
static char buf_time[256];
static char buf_date[256];
bool clock_enabled;
static Timeout* clock_timeout;

void DefaultClock() {
  clock_enabled = false;
  clock_timeout = nullptr;
  time1_format.clear();
  time1_timezone.clear();
  time2_format.clear();
  time2_timezone.clear();
  time_tooltip_format.clear();
  time_tooltip_timezone.clear();
  clock_lclick_command.clear();
  clock_rclick_command.clear();
  time1_font_desc = nullptr;
  time2_font_desc = nullptr;
}

void CleanupClock() {
  if (time1_font_desc) {
    pango_font_description_free(time1_font_desc);
  }

  if (time2_font_desc) {
    pango_font_description_free(time2_font_desc);
  }

  if (clock_timeout) {
    StopTimeout(clock_timeout);
  }

  time1_format.clear();
  time1_timezone.clear();
  time2_format.clear();
  time2_timezone.clear();
  time_tooltip_format.clear();
  time_tooltip_timezone.clear();
  clock_lclick_command.clear();
  clock_rclick_command.clear();
}

void UpdateClockSeconds(void* arg) {
  gettimeofday(&time_clock, 0);

  if (!time1_format.empty()) {
    for (int i = 0; i < nb_panel; i++) {
      panel1[i].clock_.need_resize_ = true;
    }
  }

  panel_refresh = true;
}

void UpdateClockMinutes(void* arg) {
  // remember old_sec because after suspend/hibernate the clock should be
  // updated directly, and not
  // on next minute change
  time_t old_sec = time_clock.tv_sec;
  gettimeofday(&time_clock, 0);

  if (time_clock.tv_sec % 60 == 0 || time_clock.tv_sec - old_sec > 60) {
    if (!time1_format.empty()) {
      for (int i = 0; i < nb_panel; i++) {
        panel1[i].clock_.need_resize_ = true;
      }
    }

    panel_refresh = true;
  }
}

struct tm* ClockGetTimeForTimezone(std::string const& timezone) {
  if (timezone.empty()) {
    return std::localtime(&time_clock.tv_sec);
  }

  const char* old_tz = getenv("TZ");
  setenv("TZ", timezone.c_str(), 1);
  struct tm* result = std::localtime(&time_clock.tv_sec);

  if (old_tz) {
    setenv("TZ", old_tz, 1);
  } else {
    unsetenv("TZ");
  }

  return result;
}

std::string Clock::GetTooltipText() {
  // FIXME: the buffer should be dynamically resizable in case the formatted
  // string doesn't fit
  static char tooltip[512];
  std::strftime(tooltip, sizeof(tooltip), time_tooltip_format.c_str(),
                ClockGetTimeForTimezone(time_tooltip_timezone));
  return tooltip;
}

void InitClock() {
  if (time1_format.empty() || clock_timeout != nullptr) {
    return;
  }

  bool has_seconds_format = time1_format.find('S') != std::string::npos ||
                            time1_format.find('T') != std::string::npos ||
                            time1_format.find('r') != std::string::npos;

  auto& update_func =
      (has_seconds_format ? UpdateClockSeconds : UpdateClockMinutes);
  clock_timeout = AddTimeout(10, 1000, update_func, 0);
}

void Clock::InitPanel(Panel* panel) {
  Clock& clock = panel->clock_;

  if (clock.bg_ == nullptr) {
    clock.bg_ = backgrounds.front();
  }

  clock.parent_ = panel;
  clock.panel_ = panel;
  clock.size_mode_ = SizeMode::kByContent;

  // check consistency
  if (time1_format.empty()) {
    return;
  }

  clock.need_resize_ = true;
  clock.on_screen_ = true;
}

void Clock::DrawForeground(cairo_t* c) {
  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));

  // draw layout
  pango_layout_set_font_description(layout.get(), time1_font_desc);
  pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);
  pango_layout_set_alignment(layout.get(), PANGO_ALIGN_CENTER);
  pango_layout_set_text(layout.get(), buf_time, strlen(buf_time));

  cairo_set_source_rgba(c, font_.color[0], font_.color[1], font_.color[2],
                        font_.alpha);

  pango_cairo_update_layout(c, layout.get());
  cairo_move_to(c, 0, time1_posy_);
  pango_cairo_show_layout(c, layout.get());

  if (!time2_format.empty()) {
    pango_layout_set_font_description(layout.get(), time2_font_desc);
    pango_layout_set_indent(layout.get(), 0);
    pango_layout_set_text(layout.get(), buf_date, strlen(buf_date));
    pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);

    pango_cairo_update_layout(c, layout.get());
    cairo_move_to(c, 0, time2_posy_);
    pango_cairo_show_layout(c, layout.get());
  }
}

bool Clock::Resize() {
  need_redraw_ = true;

  std::strftime(buf_time, sizeof(buf_time), time1_format.c_str(),
                ClockGetTimeForTimezone(time1_timezone));

  int time_height_ink = 0, time_height = 0, time_width = 0;
  int date_height_ink = 0, date_height = 0, date_width = 0;
  GetTextSize2(time1_font_desc, &time_height_ink, &time_height, &time_width,
               panel_->height_, panel_->width_, buf_time, strlen(buf_time));

  if (!time2_format.empty()) {
    std::strftime(buf_date, sizeof(buf_date), time2_format.c_str(),
                  ClockGetTimeForTimezone(time2_timezone));
    GetTextSize2(time2_font_desc, &date_height_ink, &date_height, &date_width,
                 panel_->height_, panel_->width_, buf_date, strlen(buf_date));
  }

  if (panel_horizontal) {
    int new_size = std::max(time_width, date_width) + (2 * padding_x_lr_) +
                   (2 * bg_->border.width);

    if (new_size > width_ || new_size < (width_ - 6)) {
      // we try to limit the number of resize
      width_ = new_size + 1;
      time1_posy_ = (height_ - time_height) / 2;

      if (!time2_format.empty()) {
        time1_posy_ -= (date_height) / 2;
        time2_posy_ = time1_posy_ + time_height;
      }

      return true;
    }
  } else {
    int new_size =
        time_height + date_height + (2 * (padding_x_lr_ + bg_->border.width));

    if (new_size != height_) {
      // we try to limit the number of resize
      height_ = new_size;
      time1_posy_ = (height_ - time_height) / 2;

      if (!time2_format.empty()) {
        time1_posy_ -= (date_height) / 2;
        time2_posy_ = time1_posy_ + time_height;
      }

      return true;
    }
  }

  return false;
}

#ifdef _TINT3_DEBUG

std::string Clock::GetFriendlyName() const { return "Clock"; }

#endif  // _TINT3_DEBUG

void ClockAction(int button) {
  if (button == 1) {
    TintExec(clock_lclick_command);
  } else if (button == 2) {
    TintExec(clock_rclick_command);
  }
}
