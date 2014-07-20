/**************************************************************************
*
* Copyright (C) 2008 Pål Staurland (staura@gmail.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

#include "server.h"
#include "config.h"
#include "panel.h"
#include "tooltip.h"


namespace {

static char kClassHintName[]  = "tint3";
static char kClassHintClass[] = "Tint3";

}


int signal_pending;
// --------------------------------------------------
// mouse events
int mouse_middle;
int mouse_right;
int mouse_scroll_up;
int mouse_scroll_down;
int mouse_tilt_left;
int mouse_tilt_right;

int panel_mode;
int wm_menu;
int panel_dock;
int panel_layer;
int panel_position;
int panel_horizontal;
int panel_refresh;
int task_dragged;

int panel_autohide;
int panel_autohide_show_timeout;
int panel_autohide_hide_timeout;
int panel_autohide_height;
int panel_strut_policy;
char* panel_items_order;

int  max_tick_urgent;

// panel's initial config
Panel panel_config;
// panels (one panel per monitor)
Panel* panel1;
int  nb_panel;

std::vector<Background*> backgrounds;

Imlib_Image default_icon;

void default_panel() {
    panel1 = 0;
    nb_panel = 0;
    default_icon = NULL;
    task_dragged = 0;
    panel_horizontal = 1;
    panel_position = CENTER;
    panel_items_order = 0;
    panel_autohide = 0;
    panel_autohide_show_timeout = 0;
    panel_autohide_hide_timeout = 0;
    panel_autohide_height = 5;  // for vertical panels this is of course the width
    panel_strut_policy = STRUT_FOLLOW_SIZE;
    panel_dock = 0;  // default not in the dock
    panel_layer = BOTTOM_LAYER;  // default is bottom layer
    wm_menu = 0;
    max_tick_urgent = 14;

    memset(&panel_config, 0, sizeof(Panel));

    // append full transparency background
    auto transparent_bg = static_cast<Background*>(
            calloc(1, sizeof(Background)));
    backgrounds.push_back(transparent_bg);
}

void cleanup_panel() {
    if (!panel1) {
        return;
    }

    cleanup_taskbar();

    // taskbarname_font_desc freed here because cleanup_taskbarname() called on _NET_NUMBER_OF_DESKTOPS
    if (taskbarname_font_desc) {
        pango_font_description_free(taskbarname_font_desc);
    }

    int i;
    Panel* p;

    for (i = 0 ; i < nb_panel ; i++) {
        p = &panel1[i];

        free_area(p);

        if (p->temp_pmap) {
            XFreePixmap(server.dsp, p->temp_pmap);
        }

        if (p->hidden_pixmap) {
            XFreePixmap(server.dsp, p->hidden_pixmap);
        }

        if (p->main_win) {
            XDestroyWindow(server.dsp, p->main_win);
        }
    }

    if (panel_items_order) {
        g_free(panel_items_order);
    }

    if (panel1) {
        delete[] panel1;
    }

    if (panel_config.g_task.font_desc) {
        pango_font_description_free(panel_config.g_task.font_desc);
    }
}

void init_panel() {
    int i, k;
    Panel* p;

    if (panel_config.monitor > (server.nb_monitor - 1)) {
        // server.nb_monitor minimum value is 1 (see get_monitors())
        fprintf(stderr,
                "warning : monitor not found. tint3 default to all monitors.\n");
        panel_config.monitor = 0;
    }

    init_tooltip();
    init_systray();
    init_launcher();
    init_clock();
#ifdef ENABLE_BATTERY
    init_battery();
#endif
    init_taskbar();

    // number of panels (one monitor or 'all' monitors)
    if (panel_config.monitor >= 0) {
        nb_panel = 1;
    } else {
        nb_panel = server.nb_monitor;
    }

    panel1 = new Panel[nb_panel];

    for (i = 0 ; i < nb_panel ; i++) {
        memcpy(&panel1[i], &panel_config, sizeof(Panel));
    }

    fprintf(stderr, "tint3 : nb monitor %d, nb monitor used %d, nb desktop %d\n",
            server.nb_monitor, nb_panel, server.nb_desktop);

    for (i = 0 ; i < nb_panel ; i++) {
        p = &panel1[i];

        if (panel_config.monitor < 0) {
            p->monitor = i;
        }

        if (p->bg == 0) {
            p->bg = backgrounds.front();
        }

        p->parent = p;
        p->panel = p;
        p->on_screen = 1;
        p->resize = 1;
        p->size_mode = SIZE_BY_LAYOUT;
        p->_resize = resize_panel;
        init_panel_size_and_position(p);

        // add childs according to panel_items
        for (k = 0 ; k < strlen(panel_items_order) ; k++) {
            if (panel_items_order[k] == 'L') {
                init_launcher_panel(p);
            }

            if (panel_items_order[k] == 'T') {
                init_taskbar_panel(p);
            }

#ifdef ENABLE_BATTERY

            if (panel_items_order[k] == 'B') {
                init_battery_panel(p);
            }

#endif

            if (panel_items_order[k] == 'S' && i == 0) {
                // TODO : check systray is only on 1 panel
                // at the moment only on panel1[0] allowed
                init_systray_panel(p);
                refresh_systray = 1;
            }

            if (panel_items_order[k] == 'C') {
                init_clock_panel(p);
            }
        }

        set_panel_items_order(p);

        {
            // catch some events
            XSetWindowAttributes attr;
            attr.colormap = server.colormap;
            attr.background_pixel = 0;
            attr.border_pixel = 0;

            unsigned long mask = CWEventMask | CWColormap | CWBackPixel | CWBorderPixel;
            p->main_win = XCreateWindow(server.dsp, server.root_win, p->posx, p->posy,
                    p->width, p->height, 0, server.depth, InputOutput, server.visual,
                    mask, &attr);
        }

        long event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                          ButtonMotionMask;

        if (p->g_task.tooltip_enabled || p->clock._get_tooltip_text
            || (launcher_enabled && launcher_tooltip_enabled)) {
            event_mask |= PointerMotionMask | LeaveWindowMask;
        }

        if (panel_autohide) {
            event_mask |= LeaveWindowMask | EnterWindowMask;
        }

        {
            XSetWindowAttributes attr;
            attr.event_mask = event_mask;
            XChangeWindowAttributes(server.dsp, p->main_win, CWEventMask, &attr);
        }

        if (!server.gc) {
            XGCValues  gcv;
            server.gc = XCreateGC(server.dsp, p->main_win, 0, &gcv);
        }

        //printf("panel %d : %d, %d, %d, %d\n", i, p->posx, p->posy, p->width, p->height);
        set_panel_properties(p);
        set_panel_background(p);

        if (snapshot_path == 0) {
            // if we are not in 'snapshot' mode then map new panel
            XMapWindow(server.dsp, p->main_win);
        }

        if (panel_autohide) {
            add_timeout(panel_autohide_hide_timeout, 0, autohide_hide, p);
        }

        visible_taskbar(p);
    }

    task_refresh_tasklist();
    active_task();
}


void init_panel_size_and_position(Panel* panel) {
    // detect panel size
    if (panel_horizontal) {
        if (panel->pourcentx) {
            panel->width = (float)server.monitor[panel->monitor].width *
                                panel->width / 100;
        }

        if (panel->pourcenty) {
            panel->height = (float)server.monitor[panel->monitor].height *
                                 panel->height / 100;
        }

        if (panel->width + panel->marginx > server.monitor[panel->monitor].width) {
            panel->width = server.monitor[panel->monitor].width - panel->marginx;
        }

        if (panel->bg->border.rounded > panel->height / 2) {
            printf("panel_background_id rounded is too big... please fix your tint3rc\n");
            /* backgrounds.push_back(*panel->bg); */
            /* panel->bg = backgrounds.back(); */
            panel->bg->border.rounded = panel->height / 2;
        }
    } else {
        int old_panel_height = panel->height;

        if (panel->pourcentx) {
            panel->height = (float)server.monitor[panel->monitor].height *
                                 panel->width / 100;
        } else {
            panel->height = panel->width;
        }

        if (panel->pourcenty) {
            panel->width = (float)server.monitor[panel->monitor].width *
                                old_panel_height / 100;
        } else {
            panel->width = old_panel_height;
        }

        if (panel->height + panel->marginy >
            server.monitor[panel->monitor].height) {
            panel->height = server.monitor[panel->monitor].height - panel->marginy;
        }

        if (panel->bg->border.rounded > panel->width / 2) {
            printf("panel_background_id rounded is too big... please fix your tint3rc\n");
            /* backgrounds.push_back(*panel->bg); */
            /* panel->bg = backgrounds.back(); */
            panel->bg->border.rounded = panel->width / 2;
        }
    }

    // panel position determined here
    if (panel_position & LEFT) {
        panel->posx = server.monitor[panel->monitor].x + panel->marginx;
    } else {
        if (panel_position & RIGHT) {
            panel->posx = server.monitor[panel->monitor].x +
                          server.monitor[panel->monitor].width - panel->width - panel->marginx;
        } else {
            if (panel_horizontal) {
                panel->posx = server.monitor[panel->monitor].x + ((
                                  server.monitor[panel->monitor].width - panel->width) / 2);
            } else {
                panel->posx = server.monitor[panel->monitor].x + panel->marginx;
            }
        }
    }

    if (panel_position & TOP) {
        panel->posy = server.monitor[panel->monitor].y + panel->marginy;
    } else {
        if (panel_position & BOTTOM) {
            panel->posy = server.monitor[panel->monitor].y +
                          server.monitor[panel->monitor].height - panel->height - panel->marginy;
        } else {
            panel->posy = server.monitor[panel->monitor].y + ((
                              server.monitor[panel->monitor].height - panel->height) / 2);
        }
    }

    // autohide or strut_policy=minimum
    int diff = (panel_horizontal ? panel->height : panel->width) -
               panel_autohide_height;

    if (panel_horizontal) {
        panel->hidden_width = panel->width;
        panel->hidden_height = panel->height - diff;
    } else {
        panel->hidden_width = panel->width - diff;
        panel->hidden_height = panel->height;
    }

    // printf("panel : posx %d, posy %d, width %d, height %d\n", panel->posx, panel->posy, panel->width, panel->height);
}


int resize_panel(void* obj) {
    resize_by_layout(obj, 0);

    //printf("resize_panel\n");
    if (panel_mode != MULTI_DESKTOP && taskbar_enabled) {
        // propagate width/height on hidden taskbar
        int i, width, height;
        Panel* panel = (Panel*)obj;
        width = panel->taskbar[server.desktop].width;
        height = panel->taskbar[server.desktop].height;

        for (i = 0 ; i < panel->nb_desktop ; i++) {
            panel->taskbar[i].width = width;
            panel->taskbar[i].height = height;
            panel->taskbar[i].resize = 1;
        }
    }

    return 0;
}


void update_strut(Panel* p) {
    if (panel_strut_policy == STRUT_NONE) {
        XDeleteProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT);
        XDeleteProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT_PARTIAL);
        return;
    }

    // Reserved space
    unsigned int d1, screen_width, screen_height;
    Window d2;
    int d3;
    XGetGeometry(server.dsp, server.root_win, &d2, &d3, &d3, &screen_width,
                 &screen_height, &d1, &d1);
    Monitor monitor = server.monitor[p->monitor];
    long   struts [12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    if (panel_horizontal) {
        int height = p->height + p->marginy;

        if (panel_strut_policy == STRUT_MINIMUM
            || (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden)) {
            height = p->hidden_height;
        }

        if (panel_position & TOP) {
            struts[2] = height + monitor.y;
            struts[8] = p->posx;
            // p->width - 1 allowed full screen on monitor 2
            struts[9] = p->posx + p->width - 1;
        } else {
            struts[3] = height + screen_height - monitor.y - monitor.height;
            struts[10] = p->posx;
            // p->width - 1 allowed full screen on monitor 2
            struts[11] = p->posx + p->width - 1;
        }
    } else {
        int width = p->width + p->marginx;

        if (panel_strut_policy == STRUT_MINIMUM
            || (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden)) {
            width = p->hidden_width;
        }

        if (panel_position & LEFT) {
            struts[0] = width + monitor.x;
            struts[4] = p->posy;
            // p->width - 1 allowed full screen on monitor 2
            struts[5] = p->posy + p->height - 1;
        } else {
            struts[1] = width + screen_width - monitor.x - monitor.width;
            struts[6] = p->posy;
            // p->width - 1 allowed full screen on monitor 2
            struts[7] = p->posy + p->height - 1;
        }
    }

    // Old specification : fluxbox need _NET_WM_STRUT.
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &struts, 4);
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STRUT_PARTIAL,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &struts, 12);
}


void set_panel_items_order(Panel* p) {
    int k, j;

    if (p->list) {
        g_slist_free(p->list);
        p->list = 0;
    }

    for (k = 0 ; k < strlen(panel_items_order) ; k++) {
        if (panel_items_order[k] == 'L') {
            p->list = g_slist_append(p->list, &p->launcher);
        }

        if (panel_items_order[k] == 'T') {
            for (j = 0 ; j < p->nb_desktop ; j++) {
                p->list = g_slist_append(p->list, &p->taskbar[j]);
            }
        }

#ifdef ENABLE_BATTERY

        if (panel_items_order[k] == 'B') {
            p->list = g_slist_append(p->list, &p->battery);
        }

#endif

        if (panel_items_order[k] == 'S' && p == panel1) {
            // TODO : check systray is only on 1 panel
            // at the moment only on panel1[0] allowed
            p->list = g_slist_append(p->list, &systray);
        }

        if (panel_items_order[k] == 'C') {
            p->list = g_slist_append(p->list, &p->clock);
        }
    }

    init_rendering(p, 0);
}


void set_panel_properties(Panel* p) {
    XStoreName(server.dsp, p->main_win, "tint3");

    gsize len;
    gchar* name = g_locale_to_utf8("tint3", -1, NULL, &len, NULL);

    if (name != NULL) {
        XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_NAME,
                        server.atom.UTF8_STRING, 8, PropModeReplace, (unsigned char*) name, (int) len);
        g_free(name);
    }

    // Dock
    long val = server.atom._NET_WM_WINDOW_TYPE_DOCK;
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_WINDOW_TYPE,
                    XA_ATOM, 32, PropModeReplace, (unsigned char*) &val, 1);

    // Sticky and below other window
    val = ALLDESKTOP;
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_DESKTOP,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &val, 1);
    Atom state[4];
    state[0] = server.atom._NET_WM_STATE_SKIP_PAGER;
    state[1] = server.atom._NET_WM_STATE_SKIP_TASKBAR;
    state[2] = server.atom._NET_WM_STATE_STICKY;
    state[3] = panel_layer == BOTTOM_LAYER ? server.atom._NET_WM_STATE_BELOW :
               server.atom._NET_WM_STATE_ABOVE;
    int nb_atoms = panel_layer == NORMAL_LAYER ? 3 : 4;
    XChangeProperty(server.dsp, p->main_win, server.atom._NET_WM_STATE, XA_ATOM,
                    32, PropModeReplace, (unsigned char*) state, nb_atoms);

    // Unfocusable
    XWMHints wmhints;

    if (panel_dock) {
        wmhints.icon_window = wmhints.window_group = p->main_win;
        wmhints.flags = StateHint | IconWindowHint;
        wmhints.initial_state = WithdrawnState;
    } else {
        wmhints.flags = InputHint;
        wmhints.input = False;
    }

    XSetWMHints(server.dsp, p->main_win, &wmhints);

    // Undecorated
    long prop[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(server.dsp, p->main_win, server.atom._MOTIF_WM_HINTS,
                    server.atom._MOTIF_WM_HINTS, 32, PropModeReplace, (unsigned char*) prop, 5);

    // XdndAware - Register for Xdnd events
    Atom version = 4;
    XChangeProperty(server.dsp, p->main_win, server.atom.XdndAware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&version, 1);

    update_strut(p);

    // Fixed position and non-resizable window
    // Allow panel move and resize when tint3 reload config file
    int minwidth = panel_autohide ? p->hidden_width : p->width;
    int minheight = panel_autohide ? p->hidden_height : p->height;
    XSizeHints size_hints;
    size_hints.flags = PPosition | PMinSize | PMaxSize;
    size_hints.min_width = minwidth;
    size_hints.max_width = p->width;
    size_hints.min_height = minheight;
    size_hints.max_height = p->height;
    XSetWMNormalHints(server.dsp, p->main_win, &size_hints);

    // Set WM_CLASS
    XClassHint* classhint = XAllocClassHint();
    classhint->res_name = kClassHintName;
    classhint->res_class = kClassHintClass;
    XSetClassHint(server.dsp, p->main_win, classhint);
    XFree(classhint);
}


void set_panel_background(Panel* p) {
    if (p->pix) {
        XFreePixmap(server.dsp, p->pix);
    }

    p->pix = XCreatePixmap(server.dsp, server.root_win, p->width,
                                p->height, server.depth);

    int xoff = 0, yoff = 0;

    if (panel_horizontal && panel_position & BOTTOM) {
        yoff = p->height - p->hidden_height;
    } else if (!panel_horizontal && panel_position & RIGHT) {
        xoff = p->width - p->hidden_width;
    }

    if (server.real_transparency) {
        clear_pixmap(p->pix, 0, 0, p->width, p->height);
    } else {
        get_root_pixmap();
        // copy background (server.root_pmap) in panel.pix
        Window dummy;
        int  x, y;
        XTranslateCoordinates(server.dsp, p->main_win, server.root_win, 0, 0, &x, &y,
                              &dummy);

        if (panel_autohide && p->is_hidden) {
            x -= xoff;
            y -= yoff;
        }

        XSetTSOrigin(server.dsp, server.gc, -x, -y);
        XFillRectangle(server.dsp, p->pix, server.gc, 0, 0, p->width,
                       p->height);
    }

    // draw background panel
    cairo_surface_t* cs;
    cairo_t* c;
    cs = cairo_xlib_surface_create(server.dsp, p->pix, server.visual,
                                   p->width, p->height);
    c = cairo_create(cs);
    draw_background(p, c);
    cairo_destroy(c);
    cairo_surface_destroy(cs);

    if (panel_autohide) {
        if (p->hidden_pixmap) {
            XFreePixmap(server.dsp, p->hidden_pixmap);
        }

        p->hidden_pixmap = XCreatePixmap(server.dsp, server.root_win, p->hidden_width,
                                         p->hidden_height, server.depth);
        XCopyArea(server.dsp, p->pix, p->hidden_pixmap, server.gc, xoff, yoff,
                  p->hidden_width, p->hidden_height, 0, 0);
    }

    // redraw panel's object
    GSList* l0;
    Area* a;

    for (l0 = p->list; l0 ; l0 = l0->next) {
        a = static_cast<Area*>(l0->data);
        set_redraw(a);
    }

    // reset task/taskbar 'state_pix'
    int i, k;
    Taskbar* tskbar;

    for (i = 0 ; i < p->nb_desktop ; i++) {
        tskbar = &p->taskbar[i];

        for (k = 0; k < TASKBAR_STATE_COUNT; ++k) {
            if (tskbar->state_pix[k]) {
                XFreePixmap(server.dsp, tskbar->state_pix[k]);
            }

            tskbar->state_pix[k] = 0;

            if (tskbar->bar_name.state_pix[k]) {
                XFreePixmap(server.dsp, tskbar->bar_name.state_pix[k]);
            }

            tskbar->bar_name.state_pix[k] = 0;
        }

        tskbar->pix = 0;
        tskbar->bar_name.pix = 0;
        l0 = tskbar->list;

        if (taskbarname_enabled) {
            l0 = l0->next;
        }

        for (; l0 ; l0 = l0->next) {
            set_task_redraw((Task*)l0->data);
        }
    }
}


Panel* get_panel(Window win) {
    int i;

    for (i = 0 ; i < nb_panel ; i++) {
        if (panel1[i].main_win == win) {
            return &panel1[i];
        }
    }

    return 0;
}


Taskbar* click_taskbar(Panel* panel, int x, int y) {
    Taskbar* tskbar;
    int i;

    if (panel_horizontal) {
        for (i = 0; i < panel->nb_desktop ; i++) {
            tskbar = &panel->taskbar[i];

            if (tskbar->on_screen && x >= tskbar->posx
                && x <= (tskbar->posx + tskbar->width)) {
                return tskbar;
            }
        }
    } else {
        for (i = 0; i < panel->nb_desktop ; i++) {
            tskbar = &panel->taskbar[i];

            if (tskbar->on_screen && y >= tskbar->posy
                && y <= (tskbar->posy + tskbar->height)) {
                return tskbar;
            }
        }
    }

    return NULL;
}


Task* click_task(Panel* panel, int x, int y) {
    Taskbar* tskbar = click_taskbar(panel, x, y);

    if (tskbar) {
        GSList* l0 = tskbar->list;

        if (taskbarname_enabled) {
            l0 = l0->next;
        }

        for (; l0 ; l0 = l0->next) {
            Task* tsk = static_cast<Task*>(l0->data);

            if (panel_horizontal) {
                if (tsk->on_screen && x >= tsk->posx
                    && x <= (tsk->posx + tsk->width)) {
                    return tsk;
                }
            } else {
                if (tsk->on_screen && y >= tsk->posy
                    && y <= (tsk->posy + tsk->height)) {
                    return tsk;
                }
            }
        }
    }

    return NULL;
}


Launcher* click_launcher(Panel* panel, int x, int y) {
    Launcher* launcher = &panel->launcher;

    if (panel_horizontal) {
        if (launcher->area.on_screen && x >= launcher->area.posx
            && x <= (launcher->area.posx + launcher->area.width)) {
            return launcher;
        }
    } else {
        if (launcher->area.on_screen && y >= launcher->area.posy
            && y <= (launcher->area.posy + launcher->area.height)) {
            return launcher;
        }
    }

    return NULL;
}


LauncherIcon* click_launcher_icon(Panel* panel, int x, int y) {
    Launcher* launcher = click_launcher(panel, x, y);
    GSList* l0;

    //printf("Click x=%d y=%d\n", x, y);
    if (launcher) {
        for (l0 = launcher->list_icons; l0 ; l0 = l0->next) {
            LauncherIcon* icon = static_cast<LauncherIcon*>(l0->data);

            if (x >= (launcher->area.posx + icon->x)
                && x <= (launcher->area.posx + icon->x + icon->icon_size) &&
                y >= (launcher->area.posy + icon->y)
                && y <= (launcher->area.posy + icon->y + icon->icon_size)) {
                //printf("Hit rect x=%d y=%d xmax=%d ymax=%d\n", launcher->area.posx + icon->x, launcher->area.posy + icon->y, launcher->area.posx + icon->x + icon->width, launcher->area.posy + icon->y + icon->height);
                return icon;
            }
        }
    }

    return NULL;
}


int click_padding(Panel* panel, int x, int y) {
    if (panel_horizontal) {
        if (x < panel->paddingxlr
            || x > panel->width - panel->paddingxlr) {
            return 1;
        }
    } else {
        if (y < panel->paddingxlr
            || y > panel->height - panel->paddingxlr) {
            return 1;
        }
    }

    return 0;
}


int click_clock(Panel* panel, int x, int y) {
    Clock clk = panel->clock;

    if (panel_horizontal) {
        if (clk.on_screen && x >= clk.posx
            && x <= (clk.posx + clk.width)) {
            return TRUE;
        }
    } else {
        if (clk.on_screen && y >= clk.posy
            && y <= (clk.posy + clk.height)) {
            return TRUE;
        }
    }

    return FALSE;
}


Area* click_area(Panel* panel, int x, int y) {
    Area* result = panel;
    Area* new_result = result;

    do {
        result = new_result;
        GSList* it = result->list;

        while (it) {
            Area* a = static_cast<Area*>(it->data);

            if (a->on_screen && x >= a->posx && x <= (a->posx + a->width)
                && y >= a->posy && y <= (a->posy + a->height)) {
                new_result = a;
                break;
            }

            it = it->next;
        }
    } while (new_result != result);

    return result;
}


void stop_autohide_timeout(Panel* p) {
    if (p->autohide_timeout) {
        stop_timeout(p->autohide_timeout);
        p->autohide_timeout = 0;
    }
}


void autohide_show(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    stop_autohide_timeout(panel);
    panel->is_hidden = 0;

    if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
        update_strut(panel);
    }

    XMapSubwindows(server.dsp, panel->main_win);  // systray windows

    if (panel_horizontal) {
        if (panel_position & TOP) {
            XResizeWindow(server.dsp, panel->main_win, panel->width,
                          panel->height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx, panel->posy,
                              panel->width, panel->height);
        }
    } else {
        if (panel_position & LEFT) {
            XResizeWindow(server.dsp, panel->main_win, panel->width,
                          panel->height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx, panel->posy,
                              panel->width, panel->height);
        }
    }

    refresh_systray =
        1;   // ugly hack, because we actually only need to call XSetBackgroundPixmap
    panel_refresh = 1;
}


void autohide_hide(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    stop_autohide_timeout(panel);
    panel->is_hidden = 1;

    if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
        update_strut(panel);
    }

    XUnmapSubwindows(server.dsp, panel->main_win);  // systray windows
    int diff = (panel_horizontal ? panel->height : panel->width) -
               panel_autohide_height;

    //printf("autohide_hide : diff %d, w %d, h %d\n", diff, panel->hidden_width, panel->hidden_height);
    if (panel_horizontal) {
        if (panel_position & TOP) {
            XResizeWindow(server.dsp, panel->main_win, panel->hidden_width,
                          panel->hidden_height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx, panel->posy + diff,
                              panel->hidden_width, panel->hidden_height);
        }
    } else {
        if (panel_position & LEFT) {
            XResizeWindow(server.dsp, panel->main_win, panel->hidden_width,
                          panel->hidden_height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx + diff, panel->posy,
                              panel->hidden_width, panel->hidden_height);
        }
    }

    panel_refresh = 1;
}


void autohide_trigger_show(Panel* p) {
    if (!p) {
        return;
    }

    if (p->autohide_timeout) {
        change_timeout(p->autohide_timeout, panel_autohide_show_timeout, 0,
                       autohide_show, p);
    } else {
        p->autohide_timeout = add_timeout(panel_autohide_show_timeout, 0, autohide_show,
                                          p);
    }
}


void autohide_trigger_hide(Panel* p) {
    if (!p) {
        return;
    }

    Window root, child;
    int xr, yr, xw, yw;
    unsigned int mask;

    if (XQueryPointer(server.dsp, p->main_win, &root, &child, &xr, &yr, &xw, &yw,
                      &mask)) {
        if (child) {
            return;  // mouse over one of the system tray icons
        }
    }

    if (p->autohide_timeout) {
        change_timeout(p->autohide_timeout, panel_autohide_hide_timeout, 0,
                       autohide_hide, p);
    } else {
        p->autohide_timeout = add_timeout(panel_autohide_hide_timeout, 0, autohide_hide,
                                          p);
    }
}
