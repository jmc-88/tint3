/**************************************************************************
*
* Tint3 : battery
*
* Copyright (C) 2009 Sebastian Reichel <elektranox@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* or any later version as published by the Free Software Foundation.
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

#include <cairo-xlib.h>
#include <cairo.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <err.h>
#include <dev/apm/apmbios.h>
#include <dev/apm/apmio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "battery/battery.hh"
#include "battery/battery_interface.hh"
#include "battery/linux_sysfs.hh"
#include "panel.hh"
#include "server.hh"
#include "util/common.hh"
#include "util/fs.hh"
#include "util/log.hh"
#include "util/timer.hh"
#include "util/window.hh"

PangoFontDescription* bat1_font_desc;
PangoFontDescription* bat2_font_desc;
std::unique_ptr<BatteryInterface> battery_ptr;
BatteryState battery_state;
bool battery_enabled;
int percentage_hide;
static Interval::Id battery_timeout;

int8_t battery_low_status;
bool battery_low_cmd_send;
std::string battery_low_cmd;
std::string path_energy_now;
std::string path_energy_full;
std::string path_current_now;
std::string path_status;

#if defined(__OpenBSD__) || defined(__NetBSD__)
int apm_fd;
#endif

namespace {

bool UpdateBatteries() {
  int old_percentage = battery_state.percentage;
  int16_t old_hours = battery_state.time.hours;
  int8_t old_minutes = battery_state.time.minutes;

  UpdateBattery();

  if (old_percentage == battery_state.percentage &&
      old_hours == battery_state.time.hours &&
      old_minutes == battery_state.time.minutes) {
    return true;
  }

  for (Panel& panel : panels) {
    if (battery_state.percentage >= percentage_hide) {
      if (panel.battery_.on_screen_) {
        panel.battery_.Hide();
        panel_refresh = true;
      }
    } else {
      if (!panel.battery_.on_screen_) {
        panel.battery_.Show();
        panel_refresh = true;
      }
    }

    if (panel.battery_.on_screen_) {
      panel.battery_.need_resize_ = true;
      panel_refresh = true;
    }
  }

  return true;
}

}  // namespace

void DefaultBattery() {
  battery_enabled = false;
  percentage_hide = 101;
  battery_low_cmd_send = false;
  battery_timeout.Clear();
  bat1_font_desc = 0;
  bat2_font_desc = 0;
  battery_low_cmd.clear();
  path_energy_now.clear();
  path_energy_full.clear();
  path_current_now.clear();
  path_status.clear();
  battery_state.percentage = 0;
  battery_state.time.hours = 0;
  battery_state.time.minutes = 0;
  battery_state.time.seconds = 0;

#if defined(__OpenBSD__) || defined(__NetBSD__)
  apm_fd = -1;
#endif
}

void CleanupBattery(Timer& timer) {
  if (bat1_font_desc) {
    pango_font_description_free(bat1_font_desc);
  }

  if (bat2_font_desc) {
    pango_font_description_free(bat2_font_desc);
  }

  battery_low_cmd.clear();
  path_energy_now.clear();
  path_energy_full.clear();
  path_current_now.clear();
  path_status.clear();

  if (battery_timeout) {
    timer.ClearInterval(battery_timeout);
  }

#if defined(__OpenBSD__) || defined(__NetBSD__)

  if ((apm_fd != -1) && (close(apm_fd) == -1)) {
    warn("cannot close /dev/apm");
  }

#endif

#if defined(__linux__)
  battery_ptr.reset();
#endif
}

void InitBattery(Timer& timer) {
  if (!battery_enabled) {
    return;
  }

  battery_ptr.reset();

#if defined(__OpenBSD__) || defined(__NetBSD__)
  apm_fd = open("/dev/apm", O_RDONLY);

  if (apm_fd < 0) {
    warn("init_battery: failed to open /dev/apm.");
    battery_enabled = 0;
    return;
  }

#elif !defined(__FreeBSD__)
  // check battery
  auto battery_dirs = linux_sysfs::GetBatteryDirectories();

  if (battery_dirs.empty()) {
    util::log::Error() << "ERROR: battery applet can't find power_supply\n";
    DefaultBattery();
    return;
  }

  // FIXME: this should handle all the batteries present
  battery_ptr.reset(new linux_sysfs::Battery(battery_dirs.front()));

  if (!battery_ptr->Found()) {
    util::log::Error() << "Can't initialize battery status.\n";
    return;
  }
#endif

  if (battery_enabled && !battery_timeout) {
    battery_timeout =
        timer.SetInterval(std::chrono::seconds(10), UpdateBatteries);
    UpdateBatteries();
  }
}

void Battery::InitPanel(Panel* panel) {
  if (!battery_enabled) {
    return;
  }

  Battery& battery = panel->battery_;
  battery.parent_ = panel;
  battery.panel_ = panel;
  battery.size_mode_ = SizeMode::kByContent;
  battery.on_screen_ = true;
  battery.need_resize_ = true;
}

void UpdateBattery() {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  int64_t energy_now = 0, energy_full = 0;
#endif

#if defined(__FreeBSD__)
  int sysctl_out = 0;
  size_t len = 0;
#endif

  unsigned int seconds = 0;

#if defined(__OpenBSD__) || defined(__NetBSD__)
  struct apm_power_info info;

  if (ioctl(apm_fd, APM_IOC_GETPOWER, &info) < 0) {
    warn("power update: APM_IOC_GETPOWER");
  }

  // best attempt at mapping to linux battery states
  battery_state.state = ChargeState::kUnknown;

  switch (info.battery_state) {
    case APM_BATT_CHARGING:
      battery_state.state = ChargeState::kCharging;
      break;

    default:
      battery_state.state = ChargeState::kDischarging;
      break;
  }

  if (info.battery_life == 100) {
    battery_state.state = ChargeState::kFull;
  }

  // no mapping for openbsd really
  energy_full = 0;
  energy_now = 0;
  seconds = info.minutes_left * 60;

  battery_state.percentage = info.battery_life;

#elif defined(__FreeBSD__)
  len = sizeof(sysctl_out);

  if (sysctlbyname("hw.acpi.battery.state", &sysctl_out, &len, nullptr, 0) !=
      0) {
    util::log::Error() << "power update: no such sysctl";
  }

  // attemp to map the battery state to linux
  battery_state.state = ChargeState::kUnknown;

  switch (sysctl_out) {
    case 1:
      battery_state.state = ChargeState::kDischarging;
      break;

    case 2:
      battery_state.state = ChargeState::kCharging;
      break;

    default:
      battery_state.state = ChargeState::kFull;
      break;
  }

  // no mapping for freebsd
  energy_full = 0;
  energy_now = 0;

  seconds = 0;

  if (sysctlbyname("hw.acpi.battery.time", &sysctl_out, &len, nullptr, 0) ==
      0) {
    seconds = sysctl_out * 60;
  }

  battery_state.percentage = -1;

  if (sysctlbyname("hw.acpi.battery.life", &sysctl_out, &len, nullptr, 0) ==
      0) {
    battery_state.percentage = sysctl_out;
  }
#else
  battery_ptr->Update();
  battery_state.state = battery_ptr->charge_state();
  battery_state.percentage = battery_ptr->charge_percentage();
  seconds = battery_ptr->seconds_to_charge();
#endif

  battery_state.time.seconds = (seconds % 60);
  battery_state.time.minutes = ((seconds / 60) % 60);
  battery_state.time.hours = (seconds / 3660);

  if (battery_low_status > battery_state.percentage &&
      battery_state.state == ChargeState::kDischarging &&
      !battery_low_cmd_send) {
    TintShellExec(battery_low_cmd);
    battery_low_cmd_send = true;
  }

  if (battery_low_status < battery_state.percentage &&
      battery_state.state == ChargeState::kCharging && battery_low_cmd_send) {
    battery_low_cmd_send = false;
  }
}

void Battery::DrawForeground(cairo_t* c) {
  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));

  // draw layout
  pango_layout_set_font_description(layout.get(), bat1_font_desc);
  pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);
  pango_layout_set_alignment(layout.get(), PANGO_ALIGN_CENTER);
  pango_layout_set_text(layout.get(), battery_percentage_.c_str(),
                        battery_percentage_.length());

  cairo_set_source_rgba(c, font[0], font[1], font[2], font.alpha());

  pango_cairo_update_layout(c, layout.get());
  cairo_move_to(c, 0, bat1_posy);
  pango_cairo_show_layout(c, layout.get());

  pango_layout_set_font_description(layout.get(), bat2_font_desc);
  pango_layout_set_indent(layout.get(), 0);
  pango_layout_set_text(layout.get(), battery_time_.c_str(),
                        battery_time_.length());
  pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);

  pango_cairo_update_layout(c, layout.get());
  cairo_move_to(c, 0, bat2_posy);
  pango_cairo_show_layout(c, layout.get());
}

bool Battery::Resize() {
  need_redraw_ = true;

  char buf_bat_percentage[10];
  snprintf(buf_bat_percentage, sizeof(buf_bat_percentage), "%d%%",
           battery_state.percentage);
  battery_percentage_ = buf_bat_percentage;

  if (battery_state.state == ChargeState::kFull) {
    battery_time_ = "Full";
  } else {
    char buf_bat_time[20];
    snprintf(buf_bat_time, sizeof(buf_bat_time), "%02d:%02d",
             battery_state.time.hours, battery_state.time.minutes);
    battery_time_ = buf_bat_time;
  }

  int bat_percentage_height, bat_percentage_width, bat_percentage_height_ink;
  GetTextSize2(bat1_font_desc, &bat_percentage_height_ink,
               &bat_percentage_height, &bat_percentage_width, panel_->height_,
               panel_->width_, battery_percentage_);

  int bat_time_height, bat_time_width, bat_time_height_ink;
  GetTextSize2(bat2_font_desc, &bat_time_height_ink, &bat_time_height,
               &bat_time_width, panel_->height_, panel_->width_, battery_time_);

  if (panel_horizontal) {
    int new_size = std::max(bat_percentage_width, bat_time_width) +
                   (2 * padding_x_lr_) + (2 * bg_.border().width());

    if (new_size > width_ || new_size < (width_ - 2)) {
      // we try to limit the number of resize
      width_ = new_size;
      bat1_posy = (height_ - bat_percentage_height - bat_time_height) / 2;
      bat2_posy = (bat1_posy + bat_percentage_height);
      return true;
    }
  } else {
    int new_size = bat_percentage_height + bat_time_height +
                   (2 * (padding_x_lr_ + bg_.border().width()));

    if (new_size > height_ || new_size < (height_ - 2)) {
      height_ = new_size;
      bat1_posy = (height_ - bat_percentage_height - bat_time_height - 2) / 2;
      bat2_posy = (bat1_posy + bat_percentage_height + 2);
      return true;
    }
  }

  return false;
}

#ifdef _TINT3_DEBUG

std::string Battery::GetFriendlyName() const { return "Battery"; }

#endif  // _TINT3_DEBUG
