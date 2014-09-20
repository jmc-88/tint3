/**************************************************************************
*
* Tint3 : clock
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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

#include <string.h>
#include <stdio.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>
#include <stdlib.h>

#include <string>

#include "window.h"
#include "server.h"
#include "panel.h"
#include "clock.h"
#include "timer.h"
#include "common.h"


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
static char buf_tooltip[512];
bool clock_enabled;
static Timeout* clock_timeout;


void default_clock() {
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

void cleanup_clock() {
    if (time1_font_desc) {
        pango_font_description_free(time1_font_desc);
    }

    if (time2_font_desc) {
        pango_font_description_free(time2_font_desc);
    }

    if (clock_timeout) {
        stop_timeout(clock_timeout);
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


void update_clocks_sec(void* arg) {
    gettimeofday(&time_clock, 0);
    int i;

    if (!time1_format.empty()) {
        for (i = 0 ; i < nb_panel ; i++) {
            panel1[i].clock.need_resize = true;
        }
    }

    panel_refresh = 1;
}

void update_clocks_min(void* arg) {
    // remember old_sec because after suspend/hibernate the clock should be updated directly, and not
    // on next minute change
    time_t old_sec = time_clock.tv_sec;
    gettimeofday(&time_clock, 0);

    if (time_clock.tv_sec % 60 == 0 || time_clock.tv_sec - old_sec > 60) {
        int i;

        if (!time1_format.empty()) {
            for (i = 0 ; i < nb_panel ; i++) {
                panel1[i].clock.need_resize = true;
            }
        }

        panel_refresh = 1;
    }
}

struct tm* clock_gettime_for_tz(std::string const& timezone) {
    if (timezone.empty()) {
        return localtime(&time_clock.tv_sec);
    }

    const char* old_tz = getenv("TZ");
    setenv("TZ", timezone.c_str(), 1);
    struct tm* result = localtime(&time_clock.tv_sec);

    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }

    return result;
}

const char* clock_get_tooltip(void* obj) {
    strftime(buf_tooltip, sizeof(buf_tooltip), time_tooltip_format.c_str(),
             clock_gettime_for_tz(time_tooltip_timezone));
    return buf_tooltip;
}


void init_clock() {
    if (time1_format.empty() || clock_timeout != nullptr) {
        return;
    }

    bool has_seconds_format = time1_format.find('S') != std::string::npos
                              || time1_format.find('T') != std::string::npos
                              || time1_format.find('r') != std::string::npos;

    if (has_seconds_format) {
        clock_timeout = add_timeout(10, 1000, update_clocks_sec, 0);
    } else {
        clock_timeout = add_timeout(10, 1000, update_clocks_min, 0);
    }
}


void init_clock_panel(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    Clock* clock = &panel->clock;

    if (clock->bg == 0) {
        clock->bg = backgrounds.front();
    }

    clock->parent = panel;
    clock->panel = panel;
    clock->size_mode = SIZE_BY_CONTENT;

    // check consistency
    if (time1_format.empty()) {
        return;
    }

    clock->need_resize = true;
    clock->on_screen = 1;

    if (!time_tooltip_format.empty()) {
        clock->_get_tooltip_text = clock_get_tooltip;
        strftime(buf_tooltip, sizeof(buf_tooltip), time_tooltip_format.c_str(),
                 clock_gettime_for_tz(time_tooltip_timezone));
    }
}


void Clock::draw_foreground(cairo_t* c) {
    PangoLayout* layout = pango_cairo_create_layout(c);

    // draw layout
    pango_layout_set_font_description(layout, time1_font_desc);
    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_text(layout, buf_time, strlen(buf_time));

    cairo_set_source_rgba(c, font.color[0], font.color[1],
                          font.color[2], font.alpha);

    pango_cairo_update_layout(c, layout);
    cairo_move_to(c, 0, time1_posy);
    pango_cairo_show_layout(c, layout);

    if (!time2_format.empty()) {
        pango_layout_set_font_description(layout, time2_font_desc);
        pango_layout_set_indent(layout, 0);
        pango_layout_set_text(layout, buf_date, strlen(buf_date));
        pango_layout_set_width(layout, width * PANGO_SCALE);

        pango_cairo_update_layout(c, layout);
        cairo_move_to(c, 0, time2_posy);
        pango_cairo_show_layout(c, layout);
    }

    g_object_unref(layout);
}


bool Clock::resize() {
    need_redraw = true;

    strftime(buf_time, sizeof(buf_time), time1_format.c_str(),
             clock_gettime_for_tz(time1_timezone));

    int time_height_ink = 0, time_height = 0, time_width = 0;
    int date_height_ink = 0, date_height = 0, date_width = 0;
    get_text_size2(time1_font_desc, &time_height_ink, &time_height, &time_width,
                   panel->height, panel->width, buf_time, strlen(buf_time));

    if (!time2_format.empty()) {
        strftime(buf_date, sizeof(buf_date), time2_format.c_str(),
                 clock_gettime_for_tz(time2_timezone));
        get_text_size2(time2_font_desc, &date_height_ink, &date_height, &date_width,
                       panel->height, panel->width, buf_date, strlen(buf_date));
    }

    if (panel_horizontal) {
        int new_size = (time_width > date_width) ? time_width : date_width;
        new_size += (2 * paddingxlr) + (2 * bg->border.width);

        if (new_size > width || new_size < (width - 6)) {
            // we try to limit the number of resize
            width = new_size + 1;
            time1_posy = (height - time_height) / 2;

            if (!time2_format.empty()) {
                time1_posy -= (date_height) / 2;
                time2_posy = time1_posy + time_height;
            }

            return true;
        }
    } else {
        int new_size = time_height + date_height + (2 * (paddingxlr +
                       bg->border.width));

        if (new_size != height) {
            // we try to limit the number of resize
            height = new_size;
            time1_posy = (height - time_height) / 2;

            if (!time2_format.empty()) {
                time1_posy -= (date_height) / 2;
                time2_posy = time1_posy + time_height;
            }

            return true;
        }
    }

    return false;
}


void clock_action(int button) {
    if (button == 1) {
        tint_exec(clock_lclick_command);
    } else if (button == 2) {
        tint_exec(clock_rclick_command);
    }
}
