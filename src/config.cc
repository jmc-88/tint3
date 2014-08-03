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


void default_config() {
    config_path.clear();
    snapshot_path.clear();
    new_config_file = 0;
}


void get_action(char* event, int* action) {
    if (strcmp(event, "none") == 0) {
        *action = NONE;
    } else if (strcmp(event, "close") == 0) {
        *action = CLOSE;
    } else if (strcmp(event, "toggle") == 0) {
        *action = TOGGLE;
    } else if (strcmp(event, "iconify") == 0) {
        *action = ICONIFY;
    } else if (strcmp(event, "shade") == 0) {
        *action = SHADE;
    } else if (strcmp(event, "toggle_iconify") == 0) {
        *action = TOGGLE_ICONIFY;
    } else if (strcmp(event, "maximize_restore") == 0) {
        *action = MAXIMIZE_RESTORE;
    } else if (strcmp(event, "desktop_left") == 0) {
        *action = DESKTOP_LEFT;
    } else if (strcmp(event, "desktop_right") == 0) {
        *action = DESKTOP_RIGHT;
    } else if (strcmp(event, "next_task") == 0) {
        *action = NEXT_TASK;
    } else if (strcmp(event, "prev_task") == 0) {
        *action = PREV_TASK;
    }
}


int get_task_status(char* status) {
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


int config_get_monitor(char* monitor) {
    if (strcmp(monitor, "all") != 0) {
        char* endptr;
        int ret_int = strtol(monitor, &endptr, 10);

        if (*endptr == 0) {
            return ret_int - 1;
        } else {
            // monitor specified by name, not by index
            for (int i = 0; i < server.nb_monitor; ++i) {
                if (server.monitor[i].names == 0) {
                    // xrandr can't identify monitors
                    continue;
                }

                for (int j = 0; server.monitor[i].names[j] != 0; ++j) {
                    if (strcmp(monitor, server.monitor[i].names[j]) == 0) {
                        return i;
                    }
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

namespace {

Background* get_background_from_id(size_t id) {
    try {
        return backgrounds.at(id);
    } catch (std::out_of_range) {
        return backgrounds.front();
    }
}

Background* get_background_from_id(char const* id) {
    return get_background_from_id(atoi(id));
}

} // namespace

void AddEntry(std::string const& key, char* value) {
    char* value1 = 0, *value2 = 0, *value3 = 0;

    /* Background and border */
    if (key == "rounded") {
        // 'rounded' is the first parameter => alloc a new background
        auto bg = new Background();
        bg->border.rounded = atoi(value);
        backgrounds.push_back(bg);
    } else if (key == "border_width") {
        backgrounds.back()->border.width = atoi(value);
    } else if (key == "background_color") {
        auto bg = backgrounds.back();
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, bg->back.color);

        if (value2) {
            bg->back.alpha = (atoi(value2) / 100.0);
        } else {
            bg->back.alpha = 0.5;
        }
    } else if (key == "border_color") {
        auto bg = backgrounds.back();
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, bg->border.color);

        if (value2) {
            bg->border.alpha = (atoi(value2) / 100.0);
        } else {
            bg->border.alpha = 0.5;
        }
    }

    /* Panel */
    else if (key == "panel_monitor") {
        panel_config.monitor = config_get_monitor(value);
    } else if (key == "panel_size") {
        extract_values(value, &value1, &value2, &value3);

        char* b;

        if ((b = strchr(value1, '%'))) {
            b[0] = '\0';
            panel_config.pourcentx = 1;
        }

        panel_config.width = atoi(value1);

        if (panel_config.width == 0) {
            // full width mode
            panel_config.width = 100;
            panel_config.pourcentx = 1;
        }

        if (value2) {
            if ((b = strchr(value2, '%'))) {
                b[0] = '\0';
                panel_config.pourcenty = 1;
            }

            panel_config.height = atoi(value2);
        }
    } else if (key == "panel_items") {
        new_config_file = 1;
        panel_items_order.assign(value);

        for (char item : panel_items_order) {
            if (item == 'L') {
                launcher_enabled = 1;
            }

            if (item == 'T') {
                taskbar_enabled = 1;
            }

            if (item == 'B') {
#ifdef ENABLE_BATTERY
                battery_enabled = 1;
#else
                fprintf(stderr, "tint3 is build without battery support\n");
#endif
            }

            if (item == 'S') {
                // systray disabled in snapshot mode
                if (snapshot_path.empty()) {
                    systray_enabled = 1;
                }
            }

            if (item == 'C') {
                clock_enabled = 1;
            }
        }
    } else if (key == "panel_margin") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.marginx = atoi(value1);

        if (value2) {
            panel_config.marginy = atoi(value2);
        }
    } else if (key == "panel_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.paddingxlr = panel_config.paddingx = atoi(value1);

        if (value2) {
            panel_config.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.paddingx = atoi(value3);
        }
    } else if (key == "panel_position") {
        extract_values(value, &value1, &value2, &value3);

        if (strcmp(value1, "top") == 0) {
            panel_position = TOP;
        } else {
            if (strcmp(value1, "bottom") == 0) {
                panel_position = BOTTOM;
            } else {
                panel_position = CENTER;
            }
        }

        if (!value2) {
            panel_position |= CENTER;
        } else {
            if (strcmp(value2, "left") == 0) {
                panel_position |= LEFT;
            } else {
                if (strcmp(value2, "right") == 0) {
                    panel_position |= RIGHT;
                } else {
                    panel_position |= CENTER;
                }
            }
        }

        if (!value3) {
            panel_horizontal = 1;
        } else {
            if (strcmp(value3, "vertical") == 0) {
                panel_horizontal = 0;
            } else {
                panel_horizontal = 1;
            }
        }
    } else if (key == "font_shadow") {
        panel_config.g_task.font_shadow = atoi(value);
    } else if (key == "panel_background_id") {
        panel_config.bg = get_background_from_id(value);
    } else if (key == "wm_menu") {
        wm_menu = atoi(value);
    } else if (key == "panel_dock") {
        panel_dock = atoi(value);
    } else if (key == "urgent_nb_of_blink") {
        max_tick_urgent = atoi(value);
    } else if (key == "panel_layer") {
        if (strcmp(value, "bottom") == 0) {
            panel_layer = BOTTOM_LAYER;
        } else if (strcmp(value, "top") == 0) {
            panel_layer = TOP_LAYER;
        } else {
            panel_layer = NORMAL_LAYER;
        }
    }

    /* Battery */
    else if (key == "battery_low_status") {
#ifdef ENABLE_BATTERY
        battery_low_status = atoi(value);

        if (battery_low_status < 0 || battery_low_status > 100) {
            battery_low_status = 0;
        }

#endif
    } else if (key == "battery_low_cmd") {
#ifdef ENABLE_BATTERY

        if (strlen(value) > 0) {
            battery_low_cmd = strdup(value);
        }

#endif
    } else if (key == "bat1_font") {
#ifdef ENABLE_BATTERY
        bat1_font_desc = pango_font_description_from_string(value);
#endif
    } else if (key == "bat2_font") {
#ifdef ENABLE_BATTERY
        bat2_font_desc = pango_font_description_from_string(value);
#endif
    } else if (key == "battery_font_color") {
#ifdef ENABLE_BATTERY
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, panel_config.battery.font.color);

        if (value2) {
            panel_config.battery.font.alpha = (atoi(value2) / 100.0);
        } else {
            panel_config.battery.font.alpha = 0.5;
        }

#endif
    } else if (key == "battery_padding") {
#ifdef ENABLE_BATTERY
        extract_values(value, &value1, &value2, &value3);
        panel_config.battery.paddingxlr = panel_config.battery.paddingx =
                                              atoi(value1);

        if (value2) {
            panel_config.battery.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.battery.paddingx = atoi(value3);
        }

#endif
    } else if (key == "battery_background_id") {
#ifdef ENABLE_BATTERY
        panel_config.battery.bg = get_background_from_id(value);
#endif
    } else if (key == "battery_hide") {
#ifdef ENABLE_BATTERY
        percentage_hide = atoi(value);

        if (percentage_hide == 0) {
            percentage_hide = 101;
        }

#endif
    }

    /* Clock */
    else if (key == "time1_format") {
        if (new_config_file == 0) {
            clock_enabled = 1;
            panel_items_order.push_back('C');
        }

        if (strlen(value) > 0) {
            time1_format = strdup(value);
            clock_enabled = 1;
        }
    } else if (key == "time2_format") {
        if (strlen(value) > 0) {
            time2_format = strdup(value);
        }
    } else if (key == "time1_font") {
        time1_font_desc = pango_font_description_from_string(value);
    } else if (key == "time1_timezone") {
        if (strlen(value) > 0) {
            time1_timezone = strdup(value);
        }
    } else if (key == "time2_timezone") {
        if (strlen(value) > 0) {
            time2_timezone = strdup(value);
        }
    } else if (key == "time2_font") {
        time2_font_desc = pango_font_description_from_string(value);
    } else if (key == "clock_font_color") {
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, panel_config.clock.font.color);

        if (value2) {
            panel_config.clock.font.alpha = (atoi(value2) / 100.0);
        } else {
            panel_config.clock.font.alpha = 0.5;
        }
    } else if (key == "clock_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.clock.paddingxlr = panel_config.clock.paddingx = atoi(
                                            value1);

        if (value2) {
            panel_config.clock.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.clock.paddingx = atoi(value3);
        }
    } else if (key == "clock_background_id") {
        panel_config.clock.bg = get_background_from_id(value);
    } else if (key == "clock_tooltip") {
        if (strlen(value) > 0) {
            time_tooltip_format = strdup(value);
        }
    } else if (key == "clock_tooltip_timezone") {
        if (strlen(value) > 0) {
            time_tooltip_timezone = strdup(value);
        }
    } else if (key == "clock_lclick_command") {
        if (strlen(value) > 0) {
            clock_lclick_command = strdup(value);
        }
    } else if (key == "clock_rclick_command") {
        if (strlen(value) > 0) {
            clock_rclick_command = strdup(value);
        }
    }

    /* Taskbar */
    else if (key == "taskbar_mode") {
        if (strcmp(value, "multi_desktop") == 0) {
            panel_mode = MULTI_DESKTOP;
        } else {
            panel_mode = SINGLE_DESKTOP;
        }
    } else if (key == "taskbar_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.g_taskbar.paddingxlr = panel_config.g_taskbar.paddingx =
                                                atoi(value1);

        if (value2) {
            panel_config.g_taskbar.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.g_taskbar.paddingx = atoi(value3);
        }
    } else if (key == "taskbar_background_id") {
        panel_config.g_taskbar.background[TASKBAR_NORMAL] = get_background_from_id(
                    value);

        if (panel_config.g_taskbar.background[TASKBAR_ACTIVE] == 0) {
            panel_config.g_taskbar.background[TASKBAR_ACTIVE] =
                panel_config.g_taskbar.background[TASKBAR_NORMAL];
        }
    } else if (key == "taskbar_active_background_id") {
        panel_config.g_taskbar.background[TASKBAR_ACTIVE] = get_background_from_id(
                    value);
    } else if (key == "taskbar_name") {
        taskbarname_enabled = atoi(value);
    } else if (key == "taskbar_name_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.g_taskbar.area_name.paddingxlr =
            panel_config.g_taskbar.area_name.paddingx = atoi(value1);
    } else if (key == "taskbar_name_background_id") {
        panel_config.g_taskbar.background_name[TASKBAR_NORMAL] = get_background_from_id(
                    value);

        if (panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] == 0) {
            panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] =
                panel_config.g_taskbar.background_name[TASKBAR_NORMAL];
        }
    } else if (key == "taskbar_name_active_background_id") {
        panel_config.g_taskbar.background_name[TASKBAR_ACTIVE] = get_background_from_id(
                    value);
    } else if (key == "taskbar_name_font") {
        taskbarname_font_desc = pango_font_description_from_string(value);
    } else if (key == "taskbar_name_font_color") {
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, taskbarname_font.color);

        if (value2) {
            taskbarname_font.alpha = (atoi(value2) / 100.0);
        } else {
            taskbarname_font.alpha = 0.5;
        }
    } else if (key == "taskbar_name_active_font_color") {
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, taskbarname_active_font.color);

        if (value2) {
            taskbarname_active_font.alpha = (atoi(value2) / 100.0);
        } else {
            taskbarname_active_font.alpha = 0.5;
        }
    }

    /* Task */
    else if (key == "task_text") {
        panel_config.g_task.text = atoi(value);
    } else if (key == "task_icon") {
        panel_config.g_task.icon = atoi(value);
    } else if (key == "task_centered") {
        panel_config.g_task.centered = atoi(value);
    } else if (key == "task_width") {
        // old parameter : just for backward compatibility
        panel_config.g_task.maximum_width = atoi(value);
        panel_config.g_task.maximum_height = 30;
    } else if (key == "task_maximum_size") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.g_task.maximum_width = atoi(value1);
        panel_config.g_task.maximum_height = 30;

        if (value2) {
            panel_config.g_task.maximum_height = atoi(value2);
        }
    } else if (key == "task_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.g_task.paddingxlr = panel_config.g_task.paddingx = atoi(
                                             value1);

        if (value2) {
            panel_config.g_task.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.g_task.paddingx = atoi(value3);
        }
    } else if (key == "task_font") {
        panel_config.g_task.font_desc = pango_font_description_from_string(value);
    } else if (regex_match("task.*_font_color", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = get_task_status(split[1]);
        g_strfreev(split);
        extract_values(value, &value1, &value2, &value3);
        float alpha = 1;

        if (value2) {
            alpha = (atoi(value2) / 100.0);
        }

        get_color(value1, panel_config.g_task.font[status].color);
        panel_config.g_task.font[status].alpha = alpha;
        panel_config.g_task.config_font_mask |= (1 << status);
    } else if (regex_match("task.*_icon_asb", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = get_task_status(split[1]);
        g_strfreev(split);
        extract_values(value, &value1, &value2, &value3);
        panel_config.g_task.alpha[status] = atoi(value1);
        panel_config.g_task.saturation[status] = atoi(value2);
        panel_config.g_task.brightness[status] = atoi(value3);
        panel_config.g_task.config_asb_mask |= (1 << status);
    } else if (regex_match("task.*_background_id", key.c_str())) {
        gchar** split = regex_split("_", key.c_str());
        int status = get_task_status(split[1]);
        g_strfreev(split);
        panel_config.g_task.background[status] = get_background_from_id(value);
        panel_config.g_task.config_background_mask |= (1 << status);

        if (status == TASK_NORMAL) {
            panel_config.g_task.bg = panel_config.g_task.background[TASK_NORMAL];
        }
    }
    // "tooltip" is deprecated but here for backwards compatibility
    else if (key == "task_tooltip" || key == "tooltip") {
        panel_config.g_task.tooltip_enabled = atoi(value);
    }

    /* Systray */
    else if (key == "systray_padding") {
        if (new_config_file == 0 && systray_enabled == 0) {
            systray_enabled = 1;
            panel_items_order.push_back('S');
        }

        extract_values(value, &value1, &value2, &value3);
        systray.paddingxlr = systray.paddingx = atoi(value1);

        if (value2) {
            systray.paddingy = atoi(value2);
        }

        if (value3) {
            systray.paddingx = atoi(value3);
        }
    } else if (key == "systray_background_id") {
        systray.bg = get_background_from_id(value);
    } else if (key == "systray_sort") {
        if (strcmp(value, "descending") == 0) {
            systray.sort = -1;
        } else if (strcmp(value, "ascending") == 0) {
            systray.sort = 1;
        } else if (strcmp(value, "left2right") == 0) {
            systray.sort = 2;
        } else  if (strcmp(value, "right2left") == 0) {
            systray.sort = 3;
        }
    } else if (key == "systray_icon_size") {
        systray_max_icon_size = atoi(value);
    } else if (key == "systray_icon_asb") {
        extract_values(value, &value1, &value2, &value3);
        systray.alpha = atoi(value1);
        systray.saturation = atoi(value2);
        systray.brightness = atoi(value3);
    }

    /* Launcher */
    else if (key == "launcher_padding") {
        extract_values(value, &value1, &value2, &value3);
        panel_config.launcher.paddingxlr = panel_config.launcher.paddingx =
                                               atoi(value1);

        if (value2) {
            panel_config.launcher.paddingy = atoi(value2);
        }

        if (value3) {
            panel_config.launcher.paddingx = atoi(value3);
        }
    } else if (key == "launcher_background_id") {
        panel_config.launcher.bg = get_background_from_id(value);
    } else if (key == "launcher_icon_size") {
        launcher_max_icon_size = atoi(value);
    } else if (key == "launcher_item_app") {
        panel_config.launcher.list_apps.push_back(value);
    } else if (key == "launcher_icon_theme") {
        // if XSETTINGS manager running, tint3 use it.
        if (!icon_theme_name) {
            icon_theme_name = strdup(value);
        }
    } else if (key == "launcher_icon_asb") {
        extract_values(value, &value1, &value2, &value3);
        launcher_alpha = atoi(value1);
        launcher_saturation = atoi(value2);
        launcher_brightness = atoi(value3);
    } else if (key == "launcher_tooltip") {
        launcher_tooltip_enabled = atoi(value);
    }

    /* Tooltip */
    else if (key == "tooltip_show_timeout") {
        int timeout_msec = 1000 * atof(value);
        g_tooltip.show_timeout_msec = timeout_msec;
    } else if (key == "tooltip_hide_timeout") {
        int timeout_msec = 1000 * atof(value);
        g_tooltip.hide_timeout_msec = timeout_msec;
    } else if (key == "tooltip_padding") {
        extract_values(value, &value1, &value2, &value3);

        if (value1) {
            g_tooltip.paddingx = atoi(value1);
        }

        if (value2) {
            g_tooltip.paddingy = atoi(value2);
        }
    } else if (key == "tooltip_background_id") {
        g_tooltip.bg = get_background_from_id(value);
    } else if (key == "tooltip_font_color") {
        extract_values(value, &value1, &value2, &value3);
        get_color(value1, g_tooltip.font_color.color);

        if (value2) {
            g_tooltip.font_color.alpha = (atoi(value2) / 100.0);
        } else {
            g_tooltip.font_color.alpha = 0.1;
        }
    } else if (key == "tooltip_font") {
        g_tooltip.font_desc = pango_font_description_from_string(value);
    }

    /* Mouse actions */
    else if (key == "mouse_middle") {
        get_action(value, &mouse_middle);
    } else if (key == "mouse_right") {
        get_action(value, &mouse_right);
    } else if (key == "mouse_scroll_up") {
        get_action(value, &mouse_scroll_up);
    } else if (key == "mouse_scroll_down") {
        get_action(value, &mouse_scroll_down);
    }

    /* autohide options */
    else if (key == "autohide") {
        panel_autohide = atoi(value);
    } else if (key == "autohide_show_timeout") {
        panel_autohide_show_timeout = 1000 * atof(value);
    } else if (key == "autohide_hide_timeout") {
        panel_autohide_hide_timeout = 1000 * atof(value);
    } else if (key == "strut_policy") {
        if (strcmp(value, "follow_size") == 0) {
            panel_strut_policy = STRUT_FOLLOW_SIZE;
        } else if (strcmp(value, "none") == 0) {
            panel_strut_policy = STRUT_NONE;
        } else {
            panel_strut_policy = STRUT_MINIMUM;
        }
    } else if (key == "autohide_height") {
        panel_autohide_height = atoi(value);

        if (panel_autohide_height == 0) {
            // autohide need height > 0
            panel_autohide_height = 1;
        }
    }

    // old config option
    else if (key == "systray") {
        if (new_config_file == 0) {
            systray_enabled = atoi(value);

            if (systray_enabled) {
                panel_items_order.push_back('S');
            }
        }
    } else if (key == "battery") {
        if (new_config_file == 0) {
            battery_enabled = atoi(value);

            if (battery_enabled) {
                panel_items_order.push_back('B');
            }
        }
    } else {
        std::cerr << "tint3: invalid option \""
                  << key
                  << "\", please upgrade tint3 or correct your configuration file.\n";
    }

    if (value1) {
        free(value1);
    }

    if (value2) {
        free(value2);
    }

    if (value3) {
        free(value3);
    }
}


bool config_read() {
    // follow XDG specification
    // check tint3rc in user directory
    std::string user_config_dir = fs::BuildPath({
        xdg::basedir::ConfigHome(), "tint3"
    });
    config_path = fs::BuildPath({ user_config_dir, "tint3rc" });

    if (fs::FileExists(config_path)) {
        return config_read_file(config_path);
    }

    // copy tint3rc from system directory to user directory
    std::string system_config_file;

    for (auto const& system_dir : xdg::basedir::ConfigDirs()) {
        system_config_file = fs::BuildPath({ system_dir, "tint3", "tint3rc" });

        if (fs::FileExists(system_config_file)) {
            break;
        }

        system_config_file.clear();
    }

    if (!system_config_file.empty()) {
        // copy file in user directory
        fs::CreateDirectory(user_config_dir);
        fs::CopyFile(system_config_file, config_path);
        return config_read_file(config_path);
    }

    return false;
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

bool config_read_file(std::string const& path) {
    bool read = fs::ReadFileByLine(path, [](std::string const & line) {
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

