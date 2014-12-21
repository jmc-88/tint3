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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <cairo-xlib.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <machine/apmvar.h>
#include <err.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "panel.h"
#include "battery.h"
#include "timer.h"
#include "common.h"
#include "../server.h"
#include "util/window.h"
#include "util/fs.h"
#include "util/log.h"

PangoFontDescription* bat1_font_desc;
PangoFontDescription* bat2_font_desc;
struct batstate battery_state;
int battery_enabled;
int percentage_hide;
static Timeout* battery_timeout;

static char buf_bat_percentage[10];
static char buf_bat_time[20];

int8_t battery_low_status;
unsigned char battery_low_cmd_send;
std::string battery_low_cmd;
std::string path_energy_now;
std::string path_energy_full;
std::string path_current_now;
std::string path_status;

#if defined(__OpenBSD__) || defined(__NetBSD__)
int apm_fd;
#endif

namespace {

void UpdateBatteries(void* arg) {
    int old_percentage = battery_state.percentage;
    int16_t old_hours = battery_state.time.hours;
    int8_t old_minutes = battery_state.time.minutes;

    UpdateBattery();

    if (old_percentage == battery_state.percentage
        && old_hours == battery_state.time.hours
        && old_minutes == battery_state.time.minutes) {
        return;
    }

    for (int i = 0 ; i < nb_panel ; i++) {
        if (battery_state.percentage >= percentage_hide) {
            if (panel1[i].battery_.on_screen_ == 1) {
                panel1[i].battery_.Hide();
                panel_refresh = 1;
            }
        } else {
            if (panel1[i].battery_.on_screen_ == 0) {
                panel1[i].battery_.Show();
                panel_refresh = 1;
            }
        }

        if (panel1[i].battery_.on_screen_ == 1) {
            panel1[i].battery_.need_resize_ = true;
            panel_refresh = 1;
        }
    }
}

} // namespace

void DefaultBattery() {
    battery_enabled = 0;
    percentage_hide = 101;
    battery_low_cmd_send = 0;
    battery_timeout = 0;
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
#if defined(__OpenBSD__) || defined(__NetBSD__)
    apm_fd = -1;
#endif
}

void CleanupBattery() {
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
        StopTimeout(battery_timeout);
    }

#if defined(__OpenBSD__) || defined(__NetBSD__)

    if ((apm_fd != -1) && (close(apm_fd) == -1)) {
        warn("cannot close /dev/apm");
    }

#endif
}


void InitBattery() {
    if (!battery_enabled) {
        return;
    }

#if defined(__OpenBSD__) || defined(__NetBSD__)
    apm_fd = open("/dev/apm", O_RDONLY);

    if (apm_fd < 0) {
        warn("init_battery: failed to open /dev/apm.");
        battery_enabled = 0;
        return;
    }

#elif !defined(__FreeBSD__)
    // check battery
    GDir* directory = 0;
    GError* error = nullptr;
    const char* entryname;
    std::string battery_dir;

    directory = g_dir_open("/sys/class/power_supply", 0, &error);

    if (error) {
        g_error_free(error);
    } else {
        while ((entryname = g_dir_read_name(directory))) {
            if (strncmp(entryname, "AC", 2) == 0) {
                continue;
            }

            auto sys_path = util::fs::BuildPath({ "/sys/class/power_supply", entryname });

            if (util::fs::FileExists({ sys_path, "present" })) {
                battery_dir = sys_path;
                break;
            }
        }
    }

    if (directory) {
        g_dir_close(directory);
    }

    if (battery_dir.empty()) {
        util::log::Error() << "ERROR: battery applet can't found power_supply\n";
        DefaultBattery();
        return;
    }

    if (util::fs::FileExists({ battery_dir, "energy_now" })) {
        path_energy_now = util::fs::BuildPath({ battery_dir, "energy_now" });
        path_energy_full = util::fs::BuildPath({ battery_dir, "energy_full" });
    }
    else if (util::fs::FileExists({ battery_dir, "charge_now" })) {
        path_energy_now = util::fs::BuildPath({ battery_dir, "charge_now" });
        path_energy_full = util::fs::BuildPath({ battery_dir, "charge_full" });
    }
    else {
        util::log::Error() << "ERROR: can't found energy_* or charge_*\n";
    }

    path_current_now = util::fs::BuildPath({ battery_dir, "power_now" });

    if (!util::fs::FileExists(path_current_now)) {
        path_current_now = util::fs::BuildPath({ battery_dir, "current_now" });
    }

    if (!path_energy_now.empty() && !path_energy_full.empty()) {
        path_status = util::fs::BuildPath({ battery_dir, "status" });

        // check file
        FILE* fp1 = fopen(path_energy_now.c_str(), "r");
        FILE* fp2 = fopen(path_energy_full.c_str(), "r");
        FILE* fp3 = fopen(path_current_now.c_str(), "r");
        FILE* fp4 = fopen(path_status.c_str(), "r");

        if (fp1 == nullptr || fp2 == nullptr || fp3 == nullptr || fp4 == nullptr) {
            CleanupBattery();
            DefaultBattery();
            util::log::Error() << "ERROR: battery applet can't open energy_now\n";
        }

        if (fp1) {
            fclose(fp1);
        }

        if (fp2) {
            fclose(fp2);
        }

        if (fp3) {
            fclose(fp3);
        }

        if (fp4) {
            fclose(fp4);
        }
    }

#endif

    if (battery_enabled && battery_timeout == 0) {
        battery_timeout = AddTimeout(10, 10000, UpdateBatteries, 0);
    }
}


void InitBatteryPanel(Panel* panel) {
    if (!battery_enabled) {
        return;
    }

    Battery& battery = panel->battery_;

    if (battery.bg_ == 0) {
        battery.bg_ = backgrounds.front();
    }

    battery.parent_ = panel;
    battery.panel_ = panel;
    battery.size_mode_ = SIZE_BY_CONTENT;
    battery.on_screen_ = true;
    battery.need_resize_ = true;
}


void UpdateBattery() {
#if !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__FreeBSD__)
    // unused on OpenBSD, silence compiler warnings
    FILE* fp;
    char tmp[25];
    int64_t current_now = 0;
#endif
#if defined(__FreeBSD__)
    int sysctl_out =  0;
    size_t len = 0;
#endif
    int64_t energy_now = 0, energy_full = 0;
    int seconds = 0;
    int8_t new_percentage = 0;

#if defined(__OpenBSD__) || defined(__NetBSD__)
    struct apm_power_info info;

    if (ioctl(apm_fd, APM_IOC_GETPOWER, &(info)) < 0) {
        warn("power update: APM_IOC_GETPOWER");
    }

    // best attempt at mapping to linux battery states
    battery_state.state = BATTERY_UNKNOWN;

    switch (info.battery_state) {
        case APM_BATT_CHARGING:
            battery_state.state = BATTERY_CHARGING;
            break;

        default:
            battery_state.state = BATTERY_DISCHARGING;
            break;
    }

    if (info.battery_life == 100) {
        battery_state.state = BATTERY_FULL;
    }

    // no mapping for openbsd really
    energy_full = 0;
    energy_now = 0;

    if (info.minutes_left != -1) {
        seconds = info.minutes_left * 60;
    } else {
        seconds = -1;
    }

    new_percentage = info.battery_life;

#elif defined(__FreeBSD__)
    len = sizeof(sysctl_out);

    if (sysctlbyname("hw.acpi.battery.state", &sysctl_out, &len, nullptr, 0) != 0) {
        util::log::Error() << "power update: no such sysctl";
    }

    // attemp to map the battery state to linux
    battery_state.state = BATTERY_UNKNOWN;

    switch (sysctl_out) {
        case 1:
            battery_state.state = BATTERY_DISCHARGING;
            break;

        case 2:
            battery_state.state = BATTERY_CHARGING;
            break;

        default:
            battery_state.state = BATTERY_FULL;
            break;
    }

    // no mapping for freebsd
    energy_full = 0;
    energy_now = 0;

    if (sysctlbyname("hw.acpi.battery.time", &sysctl_out, &len, nullptr, 0) != 0) {
        seconds = -1;
    } else {
        seconds = sysctl_out * 60;
    }

    // charging or error
    if (seconds < 0) {
        seconds = 0;
    }

    if (sysctlbyname("hw.acpi.battery.life", &sysctl_out, &len, nullptr, 0) != 0) {
        new_percentage = -1;
    } else {
        new_percentage = sysctl_out;
    }

#else
    fp = fopen(path_status.c_str(), "r");

    if (fp != nullptr) {
        if (fgets(tmp, sizeof tmp, fp)) {
            battery_state.state = BATTERY_UNKNOWN;

            if (strcasecmp(tmp, "Charging\n") == 0) {
                battery_state.state = BATTERY_CHARGING;
            }

            if (strcasecmp(tmp, "Discharging\n") == 0) {
                battery_state.state = BATTERY_DISCHARGING;
            }

            if (strcasecmp(tmp, "Full\n") == 0) {
                battery_state.state = BATTERY_FULL;
            }
        }

        fclose(fp);
    }

    fp = fopen(path_energy_now.c_str(), "r");

    if (fp != nullptr) {
        if (fgets(tmp, sizeof tmp, fp)) {
            energy_now = atoi(tmp);
        }

        fclose(fp);
    }

    fp = fopen(path_energy_full.c_str(), "r");

    if (fp != nullptr) {
        if (fgets(tmp, sizeof tmp, fp)) {
            energy_full = atoi(tmp);
        }

        fclose(fp);
    }

    fp = fopen(path_current_now.c_str(), "r");

    if (fp != nullptr) {
        if (fgets(tmp, sizeof tmp, fp)) {
            current_now = atoi(tmp);
        }

        fclose(fp);
    }

    if (current_now > 0) {
        switch (battery_state.state) {
            case BATTERY_CHARGING:
                seconds = 3600 * (energy_full - energy_now) / current_now;
                break;

            case BATTERY_DISCHARGING:
                seconds = 3600 * energy_now / current_now;
                break;

            default:
                seconds = 0;
                break;
        }
    } else {
        seconds = 0;
    }

#endif

    battery_state.time.hours = seconds / 3600;
    seconds -= 3600 * battery_state.time.hours;
    battery_state.time.minutes = seconds / 60;
    seconds -= 60 * battery_state.time.minutes;
    battery_state.time.seconds = seconds;

    if (energy_full > 0) {
        new_percentage = (energy_now * 100) / energy_full;
    }

    if (battery_low_status > new_percentage
        && battery_state.state == BATTERY_DISCHARGING && !battery_low_cmd_send) {
        TintExec(battery_low_cmd);
        battery_low_cmd_send = 1;
    }

    if (battery_low_status < new_percentage
        && battery_state.state == BATTERY_CHARGING && battery_low_cmd_send) {
        battery_low_cmd_send = 0;
    }

    battery_state.percentage = new_percentage;

    // clamp percentage to 100 in case battery is misreporting that its current charge is more than its max
    if (battery_state.percentage > 100) {
        battery_state.percentage = 100;
    }
}


void Battery::DrawForeground(cairo_t* c) {
    PangoLayout* layout = pango_cairo_create_layout(c);

    // draw layout
    pango_layout_set_font_description(layout, bat1_font_desc);
    pango_layout_set_width(layout, width_ * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_text(layout, buf_bat_percentage, strlen(buf_bat_percentage));

    cairo_set_source_rgba(c, font.color[0], font.color[1],
                          font.color[2], font.alpha);

    pango_cairo_update_layout(c, layout);
    cairo_move_to(c, 0, bat1_posy);
    pango_cairo_show_layout(c, layout);

    pango_layout_set_font_description(layout, bat2_font_desc);
    pango_layout_set_indent(layout, 0);
    pango_layout_set_text(layout, buf_bat_time, strlen(buf_bat_time));
    pango_layout_set_width(layout, width_ * PANGO_SCALE);

    pango_cairo_update_layout(c, layout);
    cairo_move_to(c, 0, bat2_posy);
    pango_cairo_show_layout(c, layout);

    g_object_unref(layout);
}


bool Battery::Resize() {
    int bat_percentage_height, bat_percentage_width, bat_percentage_height_ink;
    int bat_time_height, bat_time_width, bat_time_height_ink;
    int ret = 0;

    need_redraw_ = true;

    snprintf(buf_bat_percentage, sizeof(buf_bat_percentage), "%d%%",
             battery_state.percentage);

    if (battery_state.state == BATTERY_FULL) {
        strcpy(buf_bat_time, "Full");
    } else {
        snprintf(buf_bat_time, sizeof(buf_bat_time), "%02d:%02d",
                 battery_state.time.hours, battery_state.time.minutes);
    }

    GetTextSize2(bat1_font_desc, &bat_percentage_height_ink,
                 &bat_percentage_height, &bat_percentage_width, panel_->height_,
                 panel_->width_, buf_bat_percentage, strlen(buf_bat_percentage));
    GetTextSize2(bat2_font_desc, &bat_time_height_ink, &bat_time_height,
                 &bat_time_width, panel_->height_, panel_->width_, buf_bat_time,
                 strlen(buf_bat_time));

    if (panel_horizontal) {
        int new_size = (bat_percentage_width > bat_time_width) ? bat_percentage_width :
                       bat_time_width;
        new_size += (2 * padding_x_lr_) + (2 * bg_->border.width);

        if (new_size > width_ || new_size < (width_ - 2)) {
            // we try to limit the number of resize
            width_ = new_size;
            bat1_posy = (height_ - bat_percentage_height - bat_time_height) / 2;
            bat2_posy = bat1_posy + bat_percentage_height;
            ret = 1;
        }
    } else {
        int new_size = bat_percentage_height + bat_time_height + (2 *
                       (padding_x_lr_ + bg_->border.width));

        if (new_size > height_ || new_size < (height_ - 2)) {
            height_ = new_size;
            bat1_posy = (height_ - bat_percentage_height - bat_time_height - 2) / 2;
            bat2_posy = bat1_posy + bat_percentage_height + 2;
            ret = 1;
        }
    }

    return ret;
}

