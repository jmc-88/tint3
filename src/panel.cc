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

#include <algorithm>

#include "server.h"
#include "config.h"
#include "panel.h"
#include "tooltip.h"
#include "util/log.h"


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
std::string panel_items_order;

int  max_tick_urgent;

// panel's initial config
Panel panel_config;
// panels (one panel per monitor)
Panel* panel1;
int  nb_panel;

std::vector<Background*> backgrounds;

Imlib_Image default_icon;

namespace {

void UpdateStrut(Panel* p) {
    if (panel_strut_policy == STRUT_NONE) {
        XDeleteProperty(server.dsp, p->main_win, server.atoms_["_NET_WM_STRUT"]);
        XDeleteProperty(server.dsp, p->main_win,
                        server.atoms_["_NET_WM_STRUT_PARTIAL"]);
        return;
    }

    // Reserved space
    unsigned int d1, screen_width, screen_height;
    Window d2;
    int d3;
    XGetGeometry(server.dsp, server.root_win, &d2, &d3, &d3, &screen_width,
                 &screen_height, &d1, &d1);
    Monitor monitor = server.monitor[p->monitor];
    long struts [12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    if (panel_horizontal) {
        int height = p->height_ + p->marginy;

        if (panel_strut_policy == STRUT_MINIMUM
            || (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden)) {
            height = p->hidden_height;
        }

        if (panel_position & TOP) {
            struts[2] = height + monitor.y;
            struts[8] = p->posx_;
            // p->width - 1 allowed full screen on monitor 2
            struts[9] = p->posx_ + p->width_ - 1;
        } else {
            struts[3] = height + screen_height - monitor.y - monitor.height;
            struts[10] = p->posx_;
            // p->width - 1 allowed full screen on monitor 2
            struts[11] = p->posx_ + p->width_ - 1;
        }
    } else {
        int width = p->width_ + p->marginx;

        if (panel_strut_policy == STRUT_MINIMUM
            || (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden)) {
            width = p->hidden_width;
        }

        if (panel_position & LEFT) {
            struts[0] = width + monitor.x;
            struts[4] = p->posy_;
            // p->width - 1 allowed full screen on monitor 2
            struts[5] = p->posy_ + p->height_ - 1;
        } else {
            struts[1] = width + screen_width - monitor.x - monitor.width;
            struts[6] = p->posy_;
            // p->width - 1 allowed full screen on monitor 2
            struts[7] = p->posy_ + p->height_ - 1;
        }
    }

    // Old specification : fluxbox need _NET_WM_STRUT.
    XChangeProperty(server.dsp, p->main_win, server.atoms_["_NET_WM_STRUT"],
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &struts, 4);
    XChangeProperty(server.dsp, p->main_win, server.atoms_["_NET_WM_STRUT_PARTIAL"],
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &struts, 12);
}

void StopAutohideTimeout(Panel* p) {
    if (p->autohide_timeout) {
        StopTimeout(p->autohide_timeout);
        p->autohide_timeout = 0;
    }
}

} // namespace


void DefaultPanel() {
    panel1 = 0;
    nb_panel = 0;
    default_icon = nullptr;
    task_dragged = 0;
    panel_horizontal = 1;
    panel_position = CENTER;
    panel_items_order.clear();
    panel_autohide = 0;
    panel_autohide_show_timeout = 0;
    panel_autohide_hide_timeout = 0;
    panel_autohide_height = 5;  // for vertical panels this is of course the width
    panel_strut_policy = STRUT_FOLLOW_SIZE;
    panel_dock = 0;  // default not in the dock
    panel_layer = BOTTOM_LAYER;  // default is bottom layer
    wm_menu = 0;
    max_tick_urgent = 14;

    // append full transparency background
    backgrounds.push_back(new Background());
}

void CleanupPanel() {
    if (!panel1) {
        return;
    }

    CleanupTaskbar();

    // taskbarname_font_desc freed here because cleanup_taskbarname() called on _NET_NUMBER_OF_DESKTOPS
    if (taskbarname_font_desc) {
        pango_font_description_free(taskbarname_font_desc);
    }

    for (int i = 0 ; i < nb_panel ; i++) {
        Panel* p = &panel1[i];

        p->FreeArea();

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

    if (panel1) {
        delete[] panel1;
    }

    if (panel_config.g_task.font_desc) {
        pango_font_description_free(panel_config.g_task.font_desc);
    }
}

void InitPanel() {
    if (panel_config.monitor > (server.nb_monitor - 1)) {
        // server.nb_monitor minimum value is 1 (see get_monitors())
        fprintf(stderr,
                "warning : monitor not found. tint3 default to all monitors.\n");
        panel_config.monitor = 0;
    }

    InitTooltip();
    InitSystray();
    InitLauncher();
    InitClock();
#ifdef ENABLE_BATTERY
    InitBattery();
#endif
    InitTaskbar();

    // number of panels (one monitor or 'all' monitors)
    if (panel_config.monitor >= 0) {
        nb_panel = 1;
    } else {
        nb_panel = server.nb_monitor;
    }

    panel1 = new Panel[nb_panel];

    for (int i = 0 ; i < nb_panel ; i++) {
        panel1[i] = panel_config;
    }

    util::log::Debug()
            << "tint3: nb monitor " << server.nb_monitor
            << ", nb monitor used " << nb_panel
            << ", nb desktop " << server.nb_desktop
            << '\n';

    for (int i = 0 ; i < nb_panel ; i++) {
        auto p = &panel1[i];

        if (panel_config.monitor < 0) {
            p->monitor = i;
        }

        if (p->bg_ == 0) {
            p->bg_ = backgrounds.front();
        }

        p->parent_ = p;
        p->panel_ = p;
        p->on_screen_ = 1;
        p->need_resize_ = true;
        p->size_mode_ = SIZE_BY_LAYOUT;
        p->InitSizeAndPosition();

        // add children according to panel_items
        for (char item : panel_items_order) {
            if (item == 'L') {
                InitLauncherPanel(p);
            }

            if (item == 'T') {
                InitTaskbarPanel(p);
            }

#ifdef ENABLE_BATTERY

            if (item == 'B') {
                InitBatteryPanel(p);
            }

#endif

            if (item == 'S' && i == 0) {
                // TODO : check systray is only on 1 panel
                // at the moment only on panel1[0] allowed
                InitSystrayPanel(p);
                refresh_systray = 1;
            }

            if (item == 'C') {
                InitClockPanel(p);
            }
        }

        p->SetItemsOrder();

        {
            // catch some events
            XSetWindowAttributes attr;
            attr.colormap = server.colormap;
            attr.background_pixel = 0;
            attr.border_pixel = 0;

            unsigned long mask = CWEventMask | CWColormap | CWBackPixel | CWBorderPixel;
            p->main_win = XCreateWindow(server.dsp, server.root_win, p->posx_, p->posy_,
                                        p->width_, p->height_, 0, server.depth, InputOutput, server.visual,
                                        mask, &attr);
        }

        long event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                          ButtonMotionMask;

        if (p->g_task.tooltip_enabled || (launcher_enabled
                                          && launcher_tooltip_enabled)) {
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
        p->SetProperties();
        p->SetBackground();

        if (snapshot_path.empty()) {
            // if we are not in 'snapshot' mode then map new panel
            XMapWindow(server.dsp, p->main_win);
        }

        if (panel_autohide) {
            AddTimeout(panel_autohide_hide_timeout, 0, AutohideHide, p);
        }

        visible_taskbar(p);
    }

    TaskRefreshTasklist();
    ActiveTask();
}


void Panel::InitSizeAndPosition() {
    // detect panel size
    if (panel_horizontal) {
        if (percent_x) {
            width_ = (float)server.monitor[monitor].width * width_ / 100;
        }

        if (percent_y) {
            height_ = (float)server.monitor[monitor].height * height_ / 100;
        }

        if (width_ + marginx > server.monitor[monitor].width) {
            width_ = server.monitor[monitor].width - marginx;
        }

        if (bg_->border.rounded > height_ / 2) {
            printf("panel_background_id rounded is too big... please fix your tint3rc\n");
            /* backgrounds.push_back(*bg); */
            /* bg = backgrounds.back(); */
            bg_->border.rounded = height_ / 2;
        }
    } else {
        int old_panel_height = height_;

        if (percent_x) {
            height_ = (float)server.monitor[monitor].height * width_ / 100;
        } else {
            height_ = width_;
        }

        if (percent_y) {
            width_ = (float)server.monitor[monitor].width * old_panel_height / 100;
        } else {
            width_ = old_panel_height;
        }

        if (height_ + marginy >
            server.monitor[monitor].height) {
            height_ = server.monitor[monitor].height - marginy;
        }

        if (bg_->border.rounded > width_ / 2) {
            printf("panel_background_id rounded is too big... please fix your tint3rc\n");
            /* backgrounds.push_back(*bg); */
            /* bg = backgrounds.back(); */
            bg_->border.rounded = width_ / 2;
        }
    }

    // panel position determined here
    if (panel_position & LEFT) {
        posx_ = server.monitor[monitor].x + marginx;
    } else {
        if (panel_position & RIGHT) {
            posx_ = server.monitor[monitor].x + server.monitor[monitor].width - width_ -
                    marginx;
        } else {
            if (panel_horizontal) {
                posx_ = server.monitor[monitor].x + ((server.monitor[monitor].width - width_) /
                                                     2);
            } else {
                posx_ = server.monitor[monitor].x + marginx;
            }
        }
    }

    if (panel_position & TOP) {
        posy_ = server.monitor[monitor].y + marginy;
    } else {
        if (panel_position & BOTTOM) {
            posy_ = server.monitor[monitor].y + server.monitor[monitor].height - height_ -
                    marginy;
        } else {
            posy_ = server.monitor[monitor].y + ((server.monitor[monitor].height -
                                                  height_) /
                                                 2);
        }
    }

    // autohide or strut_policy=minimum
    int diff = (panel_horizontal ? height_ : width_) - panel_autohide_height;

    if (panel_horizontal) {
        hidden_width = width_;
        hidden_height = height_ - diff;
    } else {
        hidden_width = width_ - diff;
        hidden_height = height_;
    }

    // printf("panel : posx %d, posy %d, width %d, height %d\n", posx, posy, width, height);
}


bool Panel::Resize() {
    ResizeByLayout(0);

    if (panel_mode != MULTI_DESKTOP && taskbar_enabled) {
        // propagate width/height on hidden taskbar
        int width = taskbar[server.desktop].width_;
        int height = taskbar[server.desktop].height_;

        for (int i = 0 ; i < nb_desktop ; ++i) {
            taskbar[i].width_ = width;
            taskbar[i].height_ = height;
            taskbar[i].need_resize_ = true;
        }
    }

    return false;
}


void Panel::Render() {
    SizeByContent();
    SizeByLayout(0, 1);
    Refresh();
}


void Panel::SetItemsOrder() {
    children_.clear();

    for (char item : panel_items_order) {
        if (item == 'L') {
            children_.push_back(&launcher);
        }

        if (item == 'T') {
            for (int j = 0 ; j < nb_desktop ; j++) {
                children_.push_back(&taskbar[j]);
            }
        }

#ifdef ENABLE_BATTERY

        if (item == 'B') {
            children_.push_back(&battery);
        }

#endif

        // FIXME: with the move of this method to the Panel class,
        // this comparison got pretty shitty (this == panel1...)

        if (item == 'S' && this == panel1) {
            // TODO : check systray is only on 1 panel
            // at the moment only on panel1[0] allowed
            children_.push_back(&systray);
        }

        if (item == 'C') {
            children_.push_back(&clock);
        }
    }

    InitRendering(0);
}


void Panel::SetProperties() {
    XStoreName(server.dsp, main_win, "tint3");

    gsize len;
    gchar* name = g_locale_to_utf8("tint3", -1, nullptr, &len, nullptr);

    if (name != nullptr) {
        XChangeProperty(server.dsp, main_win, server.atoms_["_NET_WM_NAME"],
                        server.atoms_["UTF8_STRING"], 8, PropModeReplace, (unsigned char*) name,
                        (int) len);
        g_free(name);
    }

    // Dock
    long val = server.atoms_["_NET_WM_WINDOW_TYPE_DOCK"];
    XChangeProperty(server.dsp, main_win, server.atoms_["_NET_WM_WINDOW_TYPE"],
                    XA_ATOM, 32, PropModeReplace, (unsigned char*) &val, 1);

    // Sticky and below other window
    val = ALLDESKTOP;
    XChangeProperty(server.dsp, main_win, server.atoms_["_NET_WM_DESKTOP"],
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &val, 1);
    Atom state[4];
    state[0] = server.atoms_["_NET_WM_STATE_SKIP_PAGER"];
    state[1] = server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"];
    state[2] = server.atoms_["_NET_WM_STATE_STICKY"];
    state[3] = panel_layer == BOTTOM_LAYER ? server.atoms_["_NET_WM_STATE_BELOW"] :
               server.atoms_["_NET_WM_STATE_ABOVE"];
    int nb_atoms = panel_layer == NORMAL_LAYER ? 3 : 4;
    XChangeProperty(server.dsp, main_win, server.atoms_["_NET_WM_STATE"], XA_ATOM,
                    32, PropModeReplace, (unsigned char*) state, nb_atoms);

    // Unfocusable
    XWMHints wmhints;

    if (panel_dock) {
        wmhints.icon_window = wmhints.window_group = main_win;
        wmhints.flags = StateHint | IconWindowHint;
        wmhints.initial_state = WithdrawnState;
    } else {
        wmhints.flags = InputHint;
        wmhints.input = False;
    }

    XSetWMHints(server.dsp, main_win, &wmhints);

    // Undecorated
    long prop[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(server.dsp, main_win, server.atoms_["_MOTIF_WM_HINTS"],
                    server.atoms_["_MOTIF_WM_HINTS"], 32, PropModeReplace, (unsigned char*) prop,
                    5);

    // XdndAware - Register for Xdnd events
    Atom version = 4;
    XChangeProperty(server.dsp, main_win, server.atoms_["XdndAware"], XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&version, 1);

    UpdateStrut(this);

    // Fixed position and non-resizable window
    // Allow panel move and resize when tint3 reload config file
    int minwidth = panel_autohide ? hidden_width : width_;
    int minheight = panel_autohide ? hidden_height : height_;
    XSizeHints size_hints;
    size_hints.flags = PPosition | PMinSize | PMaxSize;
    size_hints.min_width = minwidth;
    size_hints.max_width = width_;
    size_hints.min_height = minheight;
    size_hints.max_height = height_;
    XSetWMNormalHints(server.dsp, main_win, &size_hints);

    // Set WM_CLASS
    XClassHint* classhint = XAllocClassHint();
    classhint->res_name = kClassHintName;
    classhint->res_class = kClassHintClass;
    XSetClassHint(server.dsp, main_win, classhint);
    XFree(classhint);
}


void Panel::SetBackground() {
    if (pix_) {
        XFreePixmap(server.dsp, pix_);
    }

    pix_ = XCreatePixmap(server.dsp, server.root_win, width_,
                         height_, server.depth);

    int xoff = 0, yoff = 0;

    if (panel_horizontal && panel_position & BOTTOM) {
        yoff = height_ - hidden_height;
    } else if (!panel_horizontal && panel_position & RIGHT) {
        xoff = width_ - hidden_width;
    }

    if (server.real_transparency) {
        clear_pixmap(pix_, 0, 0, width_, height_);
    } else {
        GetRootPixmap();
        // copy background (server.root_pmap) in panel.pix
        Window dummy;
        int  x, y;
        XTranslateCoordinates(server.dsp, main_win, server.root_win, 0, 0, &x, &y,
                              &dummy);

        if (panel_autohide && is_hidden) {
            x -= xoff;
            y -= yoff;
        }

        XSetTSOrigin(server.dsp, server.gc, -x, -y);
        XFillRectangle(server.dsp, pix_, server.gc, 0, 0, width_,
                       height_);
    }

    // draw background panel
    auto cs = cairo_xlib_surface_create(server.dsp, pix_, server.visual,
                                        width_, height_);
    auto c = cairo_create(cs);
    DrawBackground(c);
    cairo_destroy(c);
    cairo_surface_destroy(cs);

    if (panel_autohide) {
        if (hidden_pixmap) {
            XFreePixmap(server.dsp, hidden_pixmap);
        }

        hidden_pixmap = XCreatePixmap(server.dsp, server.root_win, hidden_width,
                                      hidden_height, server.depth);
        XCopyArea(server.dsp, pix_, hidden_pixmap, server.gc, xoff, yoff,
                  hidden_width, hidden_height, 0, 0);
    }

    // redraw panel's object
    for (auto& child : children_) {
        child->SetRedraw();
    }

    // reset task/taskbar 'state_pix'
    for (int i = 0 ; i < nb_desktop ; i++) {
        auto& tskbar = taskbar[i];

        for (int k = 0; k < TASKBAR_STATE_COUNT; ++k) {
            tskbar.reset_state_pixmap(k);
            tskbar.bar_name.reset_state_pixmap(k);
        }

        tskbar.pix_ = 0;
        tskbar.bar_name.pix_ = 0;

        auto begin = tskbar.children_.begin();

        if (taskbarname_enabled) {
            ++begin;
        }

        std::for_each(begin, tskbar.children_.end(), [](Area * child) {
            set_task_redraw(static_cast<Task*>(child));
        });
    }
}


Panel* GetPanel(Window win) {
    for (int i = 0 ; i < nb_panel ; ++i) {
        if (panel1[i].main_win == win) {
            return &panel1[i];
        }
    }

    return nullptr;
}


Taskbar* Panel::ClickTaskbar(int x, int y) {
    if (panel_horizontal) {
        for (int i = 0; i < nb_desktop ; i++) {
            Taskbar* tskbar = &taskbar[i];

            if (tskbar->on_screen_ && x >= tskbar->posx_
                && x <= (tskbar->posx_ + tskbar->width_)) {
                return tskbar;
            }
        }
    } else {
        for (int i = 0; i < nb_desktop ; i++) {
            Taskbar* tskbar = &taskbar[i];

            if (tskbar->on_screen_ && y >= tskbar->posy_
                && y <= (tskbar->posy_ + tskbar->height_)) {
                return tskbar;
            }
        }
    }

    return nullptr;
}


Task* Panel::ClickTask(int x, int y) {
    Taskbar* tskbar = ClickTaskbar(x, y);

    if (tskbar) {
        auto begin = tskbar->children_.begin();

        if (taskbarname_enabled) {
            ++begin;
        }

        for (auto it = begin; it != tskbar->children_.end(); ++it) {
            auto tsk = reinterpret_cast<Task*>(*it);

            if (panel_horizontal) {
                if (tsk->on_screen_ && x >= tsk->posx_
                    && x <= (tsk->posx_ + tsk->width_)) {
                    return tsk;
                }
            } else {
                if (tsk->on_screen_ && y >= tsk->posy_
                    && y <= (tsk->posy_ + tsk->height_)) {
                    return tsk;
                }
            }
        }
    }

    return nullptr;
}


Launcher* Panel::ClickLauncher(int x, int y) {
    if (panel_horizontal) {
        if (launcher.on_screen_ && x >= launcher.posx_
            && x <= (launcher.posx_ + launcher.width_)) {
            return &launcher;
        }
    } else {
        if (launcher.on_screen_ && y >= launcher.posy_
            && y <= (launcher.posy_ + launcher.height_)) {
            return &launcher;
        }
    }

    return nullptr;
}


LauncherIcon* Panel::ClickLauncherIcon(int x, int y) {
    auto launcher = ClickLauncher(x, y);

    if (launcher) {
        for (auto const& launcher_icon : launcher->list_icons_) {
            bool insideX = (x >= (launcher->posx_ + launcher_icon->x_)
                            && x <= (launcher->posx_ + launcher_icon->x_ + launcher_icon->icon_size_));
            bool insideY = (y >= (launcher->posy_ + launcher_icon->y_)
                            && y <= (launcher->posy_ + launcher_icon->y_ + launcher_icon->icon_size_));

            if (insideX && insideY) {
                return launcher_icon;
            }
        }
    }

    return nullptr;
}


bool Panel::ClickPadding(int x, int y) {
    if (panel_horizontal) {
        if (x < padding_x_lr_ || x > width_ - padding_x_lr_) {
            return true;
        }
    } else {
        if (y < padding_x_lr_ || y > height_ - padding_x_lr_) {
            return true;
        }
    }

    return false;
}


bool Panel::ClickClock(int x, int y) {
    if (panel_horizontal) {
        if (clock.on_screen_ && x >= clock.posx_
            && x <= (clock.posx_ + clock.width_)) {
            return true;
        }
    } else {
        if (clock.on_screen_ && y >= clock.posy_
            && y <= (clock.posy_ + clock.height_)) {
            return true;
        }
    }

    return false;
}


Area* Panel::ClickArea(int x, int y) {
    Area* new_result = this;
    Area* result;

    do {
        result = new_result;

        for (auto it = result->children_.begin(); it != result->children_.end(); ++it) {
            Area* a = (*it);

            if (a->on_screen_ && x >= a->posx_ && x <= (a->posx_ + a->width_)
                && y >= a->posy_ && y <= (a->posy_ + a->height_)) {
                new_result = a;
                break;
            }
        }
    } while (new_result != result);

    return result;
}


void AutohideShow(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    StopAutohideTimeout(panel);
    panel->is_hidden = 0;

    if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
        UpdateStrut(panel);
    }

    XMapSubwindows(server.dsp, panel->main_win);  // systray windows

    if (panel_horizontal) {
        if (panel_position & TOP) {
            XResizeWindow(server.dsp, panel->main_win, panel->width_,
                          panel->height_);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx_, panel->posy_,
                              panel->width_, panel->height_);
        }
    } else {
        if (panel_position & LEFT) {
            XResizeWindow(server.dsp, panel->main_win, panel->width_,
                          panel->height_);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx_, panel->posy_,
                              panel->width_, panel->height_);
        }
    }

    refresh_systray =
        1;   // ugly hack, because we actually only need to call XSetBackgroundPixmap
    panel_refresh = 1;
}


void AutohideHide(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    StopAutohideTimeout(panel);
    panel->is_hidden = 1;

    if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
        UpdateStrut(panel);
    }

    XUnmapSubwindows(server.dsp, panel->main_win);  // systray windows
    int diff = (panel_horizontal ? panel->height_ : panel->width_) -
               panel_autohide_height;

    //printf("autohide_hide : diff %d, w %d, h %d\n", diff, panel->hidden_width, panel->hidden_height);
    if (panel_horizontal) {
        if (panel_position & TOP) {
            XResizeWindow(server.dsp, panel->main_win, panel->hidden_width,
                          panel->hidden_height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx_,
                              panel->posy_ + diff,
                              panel->hidden_width, panel->hidden_height);
        }
    } else {
        if (panel_position & LEFT) {
            XResizeWindow(server.dsp, panel->main_win, panel->hidden_width,
                          panel->hidden_height);
        } else {
            XMoveResizeWindow(server.dsp, panel->main_win, panel->posx_ + diff,
                              panel->posy_,
                              panel->hidden_width, panel->hidden_height);
        }
    }

    panel_refresh = 1;
}


void AutohideTriggerShow(Panel* p) {
    if (!p) {
        return;
    }

    if (p->autohide_timeout) {
        ChangeTimeout(p->autohide_timeout, panel_autohide_show_timeout, 0,
                      AutohideShow, p);
    } else {
        p->autohide_timeout = AddTimeout(panel_autohide_show_timeout, 0, AutohideShow,
                                         p);
    }
}


void AutohideTriggerHide(Panel* p) {
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
        ChangeTimeout(p->autohide_timeout, panel_autohide_hide_timeout, 0,
                      AutohideHide, p);
    } else {
        p->autohide_timeout = AddTimeout(panel_autohide_hide_timeout, 0, AutohideHide,
                                         p);
    }
}
