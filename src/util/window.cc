/**************************************************************************
*
* Tint3 : common windows function
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Imlib2.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include "common.h"
#include "window.h"
#include "panel.h"
#include "../server.h"
#include "../taskbar/taskbar.h"



void SetActive(Window win) {
    SendEvent32(win, server.atoms_["_NET_ACTIVE_WINDOW"], 2, CurrentTime, 0);
}


void SetDesktop(int desktop) {
    SendEvent32(server.root_win, server.atoms_["_NET_CURRENT_DESKTOP"], desktop, 0,
                0);
}


void WindowSetDesktop(Window win, int desktop) {
    SendEvent32(win, server.atoms_["_NET_WM_DESKTOP"], desktop, 2, 0);
}


void SetClose(Window win) {
    SendEvent32(win, server.atoms_["_NET_CLOSE_WINDOW"], 0, 2, 0);
}


void WindowToggleShade(Window win) {
    SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
                server.atoms_["_NET_WM_STATE_SHADED"], 0);
}


void WindowMaximizeRestore(Window win) {
    SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
                server.atoms_["_NET_WM_STATE_MAXIMIZED_VERT"], 0);
    SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
                server.atoms_["_NET_WM_STATE_MAXIMIZED_HORZ"], 0);
}


int WindowIsHidden(Window win) {
    int state_count;
    auto at = ServerGetProperty<Atom>(
                  win, server.atoms_["_NET_WM_STATE"],
                  XA_ATOM, &state_count);

    for (int i = 0; i < state_count; ++i) {
        if (at.get()[i] == server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"]) {
            return 1;
        }

        // do not add transient_for windows if the transient window is already in the taskbar
        Window window = win;

        while (XGetTransientForHint(server.dsp, window, &window)) {
            if (TaskGetTasks(window)) {
                return 1;
            }
        }
    }

    int type_count;
    at = ServerGetProperty<Atom>(
             win, server.atoms_["_NET_WM_WINDOW_TYPE"],
             XA_ATOM, &type_count);

    for (int i = 0; i < type_count; ++i) {
        if (at.get()[i] == server.atoms_["_NET_WM_WINDOW_TYPE_DOCK"] ||
            at.get()[i] == server.atoms_["_NET_WM_WINDOW_TYPE_DESKTOP"] ||
            at.get()[i] == server.atoms_["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
            at.get()[i] == server.atoms_["_NET_WM_WINDOW_TYPE_MENU"] ||
            at.get()[i] == server.atoms_["_NET_WM_WINDOW_TYPE_SPLASH"]) {
            return 1;
        }
    }

    for (int i = 0 ; i < nb_panel ; ++i) {
        if (panel1[i].main_win_ == win) {
            return 1;
        }
    }

    // specification
    // Windows with neither _NET_WM_WINDOW_TYPE nor WM_TRANSIENT_FOR set
    // MUST be taken as top-level window.
    return 0;
}


int WindowGetMonitor(Window win) {
    int i, x, y;
    Window src;

    XTranslateCoordinates(server.dsp, win, server.root_win, 0, 0, &x, &y, &src);
    x += 2;
    y += 2;

    for (i = 0; i < server.nb_monitor; i++) {
        if (x >= server.monitor[i].x
            && x <= (server.monitor[i].x + server.monitor[i].width))
            if (y >= server.monitor[i].y
                && y <= (server.monitor[i].y + server.monitor[i].height)) {
                break;
            }
    }

    //printf("window %lx : ecran %d, (%d, %d)\n", win, i, x, y);
    if (i == server.nb_monitor) {
        return 0;
    } else {
        return i;
    }
}


int WindowIsIconified(Window win) {
    // EWMH specification : minimization of windows use _NET_WM_STATE_HIDDEN.
    // WM_STATE is not accurate for shaded window and in multi_desktop mode.
    int count;
    auto at = ServerGetProperty<Atom>(
                  win, server.atoms_["_NET_WM_STATE"],
                  XA_ATOM, &count);

    for (int i = 0; i < count; i++) {
        if (at.get()[i] == server.atoms_["_NET_WM_STATE_HIDDEN"]) {
            return 1;
        }
    }

    return 0;
}


int WindowIsUrgent(Window win) {
    int count;
    auto at = ServerGetProperty<Atom>(
                  win, server.atoms_["_NET_WM_STATE"],
                  XA_ATOM, &count);

    for (int i = 0; i < count; i++) {
        if (at.get()[i] == server.atoms_["_NET_WM_STATE_DEMANDS_ATTENTION"]) {
            return 1;
        }
    }

    return 0;
}


int WindowIsSkipTaskbar(Window win) {
    int count;
    auto at = ServerGetProperty<Atom>(
                  win, server.atoms_["_NET_WM_STATE"],
                  XA_ATOM, &count);

    for (int i = 0; i < count; ++i) {
        if (at.get()[i] == server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"]) {
            return 1;
        }
    }

    return 0;
}


std::vector<std::string> ServerGetDesktopNames() {
    int count;
    auto data_ptr = ServerGetProperty<char>(
                        server.root_win, server.atoms_["_NET_DESKTOP_NAMES"],
                        server.atoms_["UTF8_STRING"], &count);

    std::vector<std::string> names;

    // data_ptr contains strings separated by NUL characters, so we can just add
    // one and add its length to a counter, then repeat until the data has been
    // fully consumed
    if (data_ptr != nullptr) {
        names.push_back(data_ptr.get());

        int j = (names.back().length() + 1);

        while (j < count - 1) {
            names.push_back(data_ptr.get() + j);
            j += (names.back().length() + 1);
        }
    }

    return names;
}


Window WindowGetActive() {
    return GetProperty32<Window>(server.root_win,
                                 server.atoms_["_NET_ACTIVE_WINDOW"],
                                 XA_WINDOW);
}


int WindowIsActive(Window win) {
    return WindowGetActive() == win;
}


int GetIconCount(gulong* data, int num) {
    int count = 0;
    int pos = 0;

    while (pos + 2 < num) {
        int w = data[pos++];
        int h = data[pos++];
        pos += w * h;

        if (pos > num || w <= 0 || h <= 0) {
            break;
        }

        count++;
    }

    return count;
}


gulong* GetBestIcon(gulong* data, int icon_count, int num, int* iw, int* ih,
                    int best_icon_size) {
    int width[icon_count];
    int height[icon_count];
    gulong* icon_data[icon_count];

    /* List up icons */
    int pos = 0;

    for (int i = icon_count - 1; i >= 0; --i) {
        int w = data[pos++];
        int h = data[pos++];

        if (pos + w * h > num) {
            break;
        }

        width[i] = w;
        height[i] = h;
        icon_data[i] = &data[pos];

        pos += w * h;
    }

    /* Try to find exact size */
    int icon_num = -1;

    for (int i = 0; i < icon_count; i++) {
        if (width[i] == best_icon_size) {
            icon_num = i;
            break;
        }
    }

    /* Take the biggest or whatever */
    if (icon_num < 0) {
        int highest = 0;

        for (int i = 0; i < icon_count; i++) {
            if (width[i] > highest) {
                icon_num = i;
                highest = width[i];
            }
        }
    }

    (*iw) = width[icon_num];
    (*ih) = height[icon_num];
    return icon_data[icon_num];
}


void GetTextSize(PangoFontDescription* font, int* height_ink, int* height,
                 int panel_height, char const* text, int len) {
    auto pmap = XCreatePixmap(server.dsp, server.root_win, panel_height,
                              panel_height, server.depth);

    auto cs = cairo_xlib_surface_create(server.dsp, pmap,
                                        server.visual, panel_height, panel_height);
    auto c = cairo_create(cs);

    auto layout = pango_cairo_create_layout(c);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text, len);

    PangoRectangle rect_ink, rect;
    pango_layout_get_pixel_extents(layout, &rect_ink, &rect);
    (*height_ink) = rect_ink.height;
    (*height) = rect.height;
    //printf("dimension : %d - %d\n", rect_ink.height, rect.height);

    g_object_unref(layout);
    cairo_destroy(c);
    cairo_surface_destroy(cs);
    XFreePixmap(server.dsp, pmap);
}


void GetTextSize2(PangoFontDescription* font, int* height_ink, int* height,
                  int* width, int panel_height, int panel_width, char const* text, int len) {
    auto pmap = XCreatePixmap(server.dsp, server.root_win, panel_height,
                              panel_height, server.depth);

    auto cs = cairo_xlib_surface_create(server.dsp, pmap,
                                        server.visual, panel_height, panel_width);
    auto c = cairo_create(cs);

    auto layout = pango_cairo_create_layout(c);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text, len);

    PangoRectangle rect_ink, rect;
    pango_layout_get_pixel_extents(layout, &rect_ink, &rect);
    (*height_ink) = rect_ink.height;
    (*height) = rect.height;
    (*width) = rect.width;
    //printf("dimension : %d - %d\n", rect_ink.height, rect.height);

    g_object_unref(layout);
    cairo_destroy(c);
    cairo_surface_destroy(cs);
    XFreePixmap(server.dsp, pmap);
}


