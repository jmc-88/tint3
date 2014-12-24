/**************************************************************************
*
* Tint3 : read/write config file
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib/gstdio.h>
#include <pango/pangocairo.h>
#include <pango/pangoxft.h>
#include <Imlib2.h>

#include <iostream>
#include <stdexcept>

#include "server.h"
#include "panel.h"
#include "systraybar.h"
#include "clock.h"
#include "config.h"
#include "launcher/launcher.h"
#include "taskbar/task.h"
#include "taskbar/taskbar.h"
#include "taskbar/taskbarname.h"
#include "tooltip/tooltip.h"
#include "util/fs.h"
#include "util/log.h"
#include "util/timer.h"
#include "util/window.h"
#include "util/xdg.h"

#ifdef ENABLE_BATTERY
#include "battery/battery.h"
#endif

// global path
std::string config_path;
std::string snapshot_path;

// --------------------------------------------------
// backward compatibility
// detect if it's an old config file (==1)
static int new_config_file;


namespace {

void GetAction(std::string const& event, int* action) {
    static std::map<std::string, MouseActionEnum> eventmap = {
        {"none", NONE},
        {"close", CLOSE},
        {"toggle", TOGGLE},
        {"iconify", ICONIFY},
        {"shade", SHADE},
        {"toggle_iconify", TOGGLE_ICONIFY},
        {"maximize_restore", MAXIMIZE_RESTORE},
        {"desktop_left", DESKTOP_LEFT},
        {"desktop_right", DESKTOP_RIGHT},
        {"next_task", NEXT_TASK},
        {"prev_task", PREV_TASK},
    };

    auto const& it = eventmap.find(event);

    if (it != eventmap.end()) {
        (*action) = it->second;
    }
}


int GetTaskStatus(char* status) {
    if (strcmp(status, "active") == 0) {
        return TASK_ACTIVE;
    }

    if (strcmp(status, "iconified") == 0) {
        return TASK_ICONIFIED;
    }

    if (strcmp(status, "urgent") == 0) {
        return TASK_URGENT;
    }

    return TASK_NORMAL;
}

int ConfigGetMonitor(std::string const& monitor_name) {
    if (monitor_name != "all") {
        char* endptr;
        int ret_int = StringToLongInt(monitor_name, &endptr);

        if (*endptr == '\0') {
            return (ret_int - 1);
        }

        // monitor specified by name, not by index
        for (int i = 0; i < server.nb_monitor; ++i) {
            for (auto& name : server.monitor[i].names) {
                if (name == monitor_name) {
                    return i;
                }
            }
        }
    }

    // monitor == "all" or monitor not found or xrandr can't identify monitors
    return -1;
}

bool regex_match(char const* pattern, char const* string) {
    return g_regex_match_simple(
               pattern, string,
               static_cast<GRegexCompileFlags>(0),
               static_cast<GRegexMatchFlags>(0));
}

gchar** regex_split(char const* pattern, char const* string) {
    return g_regex_split_simple(
               pattern, string,
               static_cast<GRegexCompileFlags>(0),
               static_cast<GRegexMatchFlags>(0));
}

Background* GetBackgroundFromId(size_t id) {
    try {
        return backgrounds.at(id);
    } catch (std::out_of_range) {
        return backgrounds.front();
    }
}

void AddEntry(std::string const& key, std::string const& value) {
    std::string value1, value2, value3;

    /* Background and border */
    if (key == "rounded") {
        // 'rounded' is the first parameter => alloc a new background
        auto bg = new Background();
        bg->border.rounded = StringToLongInt(value);
        backgrounds.push_back(bg);
    } else if (key == "border_width") {
        backgrounds.back()->border.width = StringToLongInt(value);
    } else if (key == "background_color") {
        auto bg = backgrounds.back();
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, bg->back.color);

        if (!value2.empty()) {
            bg->back.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            bg->back.alpha = 0.5;
        }
    } else if (key == "border_color") {
        auto bg = backgrounds.back();
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, bg->border.color);

        if (!value2.empty()) {
            bg->border.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            bg->border.alpha = 0.5;
        }
    }

    /* Panel */
    else if (key == "panel_monitor") {
        panel_config.monitor_ = ConfigGetMonitor(value);
    } else if (key == "panel_size") {
        ExtractValues(value, value1, value2, value3);

        size_t b = value1.find_first_of('%');

        panel_config.percent_x = (b != std::string::npos);
        panel_config.width_ = StringToLongInt(value1.substr(0, b));

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

            panel_config.height_ = StringToLongInt(value2.substr(0, b));
        }
    } else if (key == "panel_items") {
        new_config_file = 1;
        panel_items_order.assign(value);

        for (char item : panel_items_order) {
            if (item == 'L') {
                launcher_enabled = true;
            }

            if (item == 'T') {
                taskbar_enabled = 1;
            }

            if (item == 'B') {
#ifdef ENABLE_BATTERY
                battery_enabled = 1;
#else
                util::log::Error() << "tint3 is built without battery support\n";
#endif
            }

            if (item == 'S') {
                // systray disabled in snapshot mode
                if (snapshot_path.empty()) {
                    systray_enabled = 1;
                }
            }

            if (item == 'C') {
                clock_enabled = true;
            }
        }
    } else if (key == "panel_margin") {
        ExtractValues(value, value1, value2, value3);
        panel_config.margin_x_ = StringToLongInt(value1);

        if (!value2.empty()) {
            panel_config.margin_y_ = StringToLongInt(value2);
        }
    } else if (key == "panel_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.padding_x_lr_ = panel_config.padding_x_ = StringToLongInt(value1);

        if (!value2.empty()) {
            panel_config.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "panel_position") {
        ExtractValues(value, value1, value2, value3);

        if (value1 == "top") {
            panel_position = TOP;
        } else if (value1 == "bottom") {
            panel_position = BOTTOM;
        } else {
            panel_position = CENTER;
        }

        if (value2 == "left") {
            panel_position |= LEFT;
        } else if (value2 == "right") {
            panel_position |= RIGHT;
        } else {
            panel_position |= CENTER;
        }

        panel_horizontal = (value3 != "vertical");
    } else if (key == "font_shadow") {
        panel_config.g_task.font_shadow = StringToLongInt(value);
    } else if (key == "panel_background_id") {
        panel_config.bg_ = GetBackgroundFromId(StringToLongInt(value));
    } else if (key == "wm_menu") {
        wm_menu = StringToLongInt(value);
    } else if (key == "panel_dock") {
        panel_dock = StringToLongInt(value);
    } else if (key == "urgent_nb_of_blink") {
        max_tick_urgent = StringToLongInt(value);
    } else if (key == "panel_layer") {
        if (value == "bottom") {
            panel_layer = BOTTOM_LAYER;
        } else if (value == "top") {
            panel_layer = TOP_LAYER;
        } else {
            panel_layer = NORMAL_LAYER;
        }
    }

    /* Battery */
    else if (key == "battery_low_status") {
#ifdef ENABLE_BATTERY
        battery_low_status = StringToLongInt(value);

        if (battery_low_status < 0 || battery_low_status > 100) {
            battery_low_status = 0;
        }

#endif
    } else if (key == "battery_low_cmd") {
#ifdef ENABLE_BATTERY

        if (!value.empty()) {
            battery_low_cmd = value;
        }

#endif
    } else if (key == "bat1_font") {
#ifdef ENABLE_BATTERY
        bat1_font_desc = pango_font_description_from_string(value.c_str());
#endif
    } else if (key == "bat2_font") {
#ifdef ENABLE_BATTERY
        bat2_font_desc = pango_font_description_from_string(value.c_str());
#endif
    } else if (key == "battery_font_color") {
#ifdef ENABLE_BATTERY
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, panel_config.battery_.font.color);

        if (!value2.empty()) {
            panel_config.battery_.font.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            panel_config.battery_.font.alpha = 0.5;
        }

#endif
    } else if (key == "battery_padding") {
#ifdef ENABLE_BATTERY
        ExtractValues(value, value1, value2, value3);
        panel_config.battery_.padding_x_lr_ = panel_config.battery_.padding_x_ =
                StringToLongInt(value1);

        if (!value2.empty()) {
            panel_config.battery_.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.battery_.padding_x_ = StringToLongInt(value3);
        }

#endif
    } else if (key == "battery_background_id") {
#ifdef ENABLE_BATTERY
        panel_config.battery_.bg_ = GetBackgroundFromId(StringToLongInt(value));
#endif
    } else if (key == "battery_hide") {
#ifdef ENABLE_BATTERY
        percentage_hide = StringToLongInt(value);

        if (percentage_hide == 0) {
            percentage_hide = 101;
        }

#endif
    }

    /* Clock */
    else if (key == "time1_format") {
        if (new_config_file == 0) {
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
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, panel_config.clock_.font.color);

        if (!value2.empty()) {
            panel_config.clock_.font.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            panel_config.clock_.font.alpha = 0.5;
        }
    } else if (key == "clock_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.clock_.padding_x_lr_ = panel_config.clock_.padding_x_ =
                                                StringToLongInt(
                                                        value1);

        if (!value2.empty()) {
            panel_config.clock_.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.clock_.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "clock_background_id") {
        panel_config.clock_.bg_ = GetBackgroundFromId(StringToLongInt(value));
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
            panel_mode = MULTI_DESKTOP;
        } else {
            panel_mode = SINGLE_DESKTOP;
        }
    } else if (key == "taskbar_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.g_taskbar.padding_x_lr_ = panel_config.g_taskbar.padding_x_ =
                StringToLongInt(value1);

        if (!value2.empty()) {
            panel_config.g_taskbar.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.g_taskbar.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "taskbar_background_id") {
        panel_config.g_taskbar.background[TASKBAR_NORMAL] = GetBackgroundFromId(
                    StringToLongInt(value));

        if (panel_config.g_taskbar.background[TASKBAR_ACTIVE] == 0) {
            panel_config.g_taskbar.background[TASKBAR_ACTIVE] =
                panel_config.g_taskbar.background[TASKBAR_NORMAL];
        }
    } else if (key == "taskbar_active_background_id") {
        panel_config.g_taskbar.background[TASKBAR_ACTIVE] = GetBackgroundFromId(
                    StringToLongInt(value));
    } else if (key == "taskbar_name") {
        taskbarname_enabled = (0 != StringToLongInt(value));
    } else if (key == "taskbar_name_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.g_taskbar.bar_name_.padding_x_lr_ =
            panel_config.g_taskbar.bar_name_.padding_x_ = StringToLongInt(value1);
    } else if (key == "taskbar_name_background_id") {
        panel_config.g_taskbar.background_name[TASKBAR_NORMAL] = GetBackgroundFromId(
                    StringToLongInt(value));

        if (panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] == 0) {
            panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] =
                panel_config.g_taskbar.background_name[TASKBAR_NORMAL];
        }
    } else if (key == "taskbar_name_active_background_id") {
        panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] = GetBackgroundFromId(
                    StringToLongInt(value));
    } else if (key == "taskbar_name_font") {
        taskbarname_font_desc = pango_font_description_from_string(value.c_str());
    } else if (key == "taskbar_name_font_color") {
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, taskbarname_font.color);

        if (!value2.empty()) {
            taskbarname_font.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            taskbarname_font.alpha = 0.5;
        }
    } else if (key == "taskbar_name_active_font_color") {
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, taskbarname_active_font.color);

        if (!value2.empty()) {
            taskbarname_active_font.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            taskbarname_active_font.alpha = 0.5;
        }
    }

    /* Task */
    else if (key == "task_text") {
        panel_config.g_task.text = StringToLongInt(value);
    } else if (key == "task_icon") {
        panel_config.g_task.icon = StringToLongInt(value);
    } else if (key == "task_centered") {
        panel_config.g_task.centered = StringToLongInt(value);
    } else if (key == "task_width") {
        // old parameter : just for backward compatibility
        panel_config.g_task.maximum_width = StringToLongInt(value);
        panel_config.g_task.maximum_height = 30;
    } else if (key == "task_maximum_size") {
        ExtractValues(value, value1, value2, value3);
        panel_config.g_task.maximum_width = StringToLongInt(value1);
        panel_config.g_task.maximum_height = 30;

        if (!value2.empty()) {
            panel_config.g_task.maximum_height = StringToLongInt(value2);
        }
    } else if (key == "task_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.g_task.padding_x_lr_ = panel_config.g_task.padding_x_ =
                                                StringToLongInt(
                                                        value1);

        if (!value2.empty()) {
            panel_config.g_task.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.g_task.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "task_font") {
        panel_config.g_task.font_desc = pango_font_description_from_string(
                                            value.c_str());
    } else if (regex_match("task.*_font_color", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = GetTaskStatus(split[1]);
        g_strfreev(split);
        ExtractValues(value, value1, value2, value3);
        float alpha = 1;

        if (!value2.empty()) {
            alpha = (StringToLongInt(value2) / 100.0);
        }

        GetColor(value1, panel_config.g_task.font[status].color);
        panel_config.g_task.font[status].alpha = alpha;
        panel_config.g_task.config_font_mask |= (1 << status);
    } else if (regex_match("task.*_icon_asb", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = GetTaskStatus(split[1]);
        g_strfreev(split);
        ExtractValues(value, value1, value2, value3);
        panel_config.g_task.alpha[status] = StringToLongInt(value1);
        panel_config.g_task.saturation[status] = StringToLongInt(value2);
        panel_config.g_task.brightness[status] = StringToLongInt(value3);
        panel_config.g_task.config_asb_mask |= (1 << status);
    } else if (regex_match("task.*_background_id", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = GetTaskStatus(split[1]);
        g_strfreev(split);
        panel_config.g_task.background[status] = GetBackgroundFromId(StringToLongInt(
                    value));
        panel_config.g_task.config_background_mask |= (1 << status);

        if (status == TASK_NORMAL) {
            panel_config.g_task.bg_ = panel_config.g_task.background[TASK_NORMAL];
        }
    }
    // "tooltip" is deprecated but here for backwards compatibility
    else if (key == "task_tooltip" || key == "tooltip") {
        panel_config.g_task.tooltip_enabled = StringToLongInt(value);
    }

    /* Systray */
    else if (key == "systray_padding") {
        if (new_config_file == 0 && systray_enabled == 0) {
            systray_enabled = 1;
            panel_items_order.push_back('S');
        }

        ExtractValues(value, value1, value2, value3);
        systray.padding_x_lr_ = systray.padding_x_ = StringToLongInt(value1);

        if (!value2.empty()) {
            systray.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            systray.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "systray_background_id") {
        systray.bg_ = GetBackgroundFromId(StringToLongInt(value));
    } else if (key == "systray_sort") {
        if (value == "descending") {
            systray.sort = -1;
        } else if (value == "ascending") {
            systray.sort = 1;
        } else if (value == "left2right") {
            systray.sort = 2;
        } else  if (value == "right2left") {
            systray.sort = 3;
        }
    } else if (key == "systray_icon_size") {
        systray_max_icon_size = StringToLongInt(value);
    } else if (key == "systray_icon_asb") {
        ExtractValues(value, value1, value2, value3);
        systray.alpha = StringToLongInt(value1);
        systray.saturation = StringToLongInt(value2);
        systray.brightness = StringToLongInt(value3);
    }

    /* Launcher */
    else if (key == "launcher_padding") {
        ExtractValues(value, value1, value2, value3);
        panel_config.launcher_.padding_x_lr_ = panel_config.launcher_.padding_x_ =
                StringToLongInt(value1);

        if (!value2.empty()) {
            panel_config.launcher_.padding_y_ = StringToLongInt(value2);
        }

        if (!value3.empty()) {
            panel_config.launcher_.padding_x_ = StringToLongInt(value3);
        }
    } else if (key == "launcher_background_id") {
        panel_config.launcher_.bg_ = GetBackgroundFromId(StringToLongInt(value));
    } else if (key == "launcher_icon_size") {
        launcher_max_icon_size = StringToLongInt(value);
    } else if (key == "launcher_item_app") {
        panel_config.launcher_.list_apps_.push_back(value);
    } else if (key == "launcher_icon_theme") {
        // if XSETTINGS manager running, tint3 use it.
        if (icon_theme_name.empty()) {
            icon_theme_name = value;
        }
    } else if (key == "launcher_icon_asb") {
        ExtractValues(value, value1, value2, value3);
        launcher_alpha = StringToLongInt(value1);
        launcher_saturation = StringToLongInt(value2);
        launcher_brightness = StringToLongInt(value3);
    } else if (key == "launcher_tooltip") {
        launcher_tooltip_enabled = StringToLongInt(value);
    }

    /* Tooltip */
    else if (key == "tooltip_show_timeout") {
        int timeout_msec = 1000 * StringToFloat(value);
        g_tooltip.show_timeout_msec = timeout_msec;
    } else if (key == "tooltip_hide_timeout") {
        int timeout_msec = 1000 * StringToFloat(value);
        g_tooltip.hide_timeout_msec = timeout_msec;
    } else if (key == "tooltip_padding") {
        ExtractValues(value, value1, value2, value3);

        if (!value1.empty()) {
            g_tooltip.paddingx = StringToLongInt(value1);
        }

        if (!value2.empty()) {
            g_tooltip.paddingy = StringToLongInt(value2);
        }
    } else if (key == "tooltip_background_id") {
        g_tooltip.bg = GetBackgroundFromId(StringToLongInt(value));
    } else if (key == "tooltip_font_color") {
        ExtractValues(value, value1, value2, value3);
        GetColor(value1, g_tooltip.font_color.color);

        if (!value2.empty()) {
            g_tooltip.font_color.alpha = (StringToLongInt(value2) / 100.0);
        } else {
            g_tooltip.font_color.alpha = 0.1;
        }
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
    }

    /* autohide options */
    else if (key == "autohide") {
        panel_autohide = StringToLongInt(value);
    } else if (key == "autohide_show_timeout") {
        panel_autohide_show_timeout = 1000 * StringToFloat(value);
    } else if (key == "autohide_hide_timeout") {
        panel_autohide_hide_timeout = 1000 * StringToFloat(value);
    } else if (key == "strut_policy") {
        if (value == "follow_size") {
            panel_strut_policy = STRUT_FOLLOW_SIZE;
        } else if (value == "none") {
            panel_strut_policy = STRUT_NONE;
        } else {
            panel_strut_policy = STRUT_MINIMUM;
        }
    } else if (key == "autohide_height") {
        panel_autohide_height = StringToLongInt(value);

        if (panel_autohide_height == 0) {
            // autohide need height > 0
            panel_autohide_height = 1;
        }
    }

    // old config option
    else if (key == "systray") {
        if (new_config_file == 0) {
            systray_enabled = StringToLongInt(value);

            if (systray_enabled) {
                panel_items_order.push_back('S');
            }
        }
    } else if (key == "battery") {
        if (new_config_file == 0) {
            battery_enabled = StringToLongInt(value);

            if (battery_enabled) {
                panel_items_order.push_back('B');
            }
        }
    } else {
        std::cerr << "tint3: invalid option \""
                  << key
                  << "\", please upgrade tint3 or correct your configuration file.\n";
    }
}

bool ParseLine(std::string const& line, std::string& key, std::string& value) {
    if (line.empty() || line[0] == '#') {
        return false;
    }

    auto equals_pos = line.find('=');

    if (equals_pos == std::string::npos) {
        return false;
    }

    StringTrim(key.assign(line, 0, equals_pos));
    StringTrim(value.assign(line, equals_pos + 1, std::string::npos));
    return true;
}

} // namespace

namespace config {

bool Read() {
    // follow XDG specification
    // check tint3rc in user directory
    std::string user_config_dir = util::fs::BuildPath({
        util::xdg::basedir::ConfigHome(), "tint3"
    });
    config_path = util::fs::BuildPath({ user_config_dir, "tint3rc" });

    if (util::fs::FileExists(config_path)) {
        return config::ReadFile(config_path);
    }

    // copy tint3rc from system directory to user directory
    std::string system_config_file;

    for (auto const& system_dir : util::xdg::basedir::ConfigDirs()) {
        system_config_file = util::fs::BuildPath({ system_dir, "tint3", "tint3rc" });

        if (util::fs::FileExists(system_config_file)) {
            break;
        }

        system_config_file.clear();
    }

    if (!system_config_file.empty()) {
        // copy file in user directory
        util::fs::CreateDirectory(user_config_dir);
        util::fs::CopyFile(system_config_file, config_path);
        return config::ReadFile(config_path);
    }

    return false;
}

bool ReadFile(std::string const& path) {
    bool read = util::fs::ReadFileByLine(path, [](std::string const & line) {
        std::string key, value;

        if (ParseLine(line, key, value)) {
            // TODO: delete this stupid strdup wrap which is only needed for now
            // because functions as extract_values will modify the input
            // string...
            char* value_str = strdup(value.c_str());
            AddEntry(key, value_str);
            std::free(value_str);
        }
    });

    if (!read) {
        return false;
    }

    // append Taskbar item
    if (new_config_file == 0) {
        taskbar_enabled = 1;
        panel_items_order.insert(0, "T");
    }

    return true;
}

} // namespace config

void DefaultConfig() {
    config_path.clear();
    snapshot_path.clear();
    new_config_file = 0;
}
