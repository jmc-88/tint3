/**************************************************************************
*
* Copyright (C) 2008 Pål Staurland (staura@gmail.com)
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "server.h"
#include "config.h"
#include "panel.h"
#include "tooltip.h"
#include "util/log.h"

namespace {

static char kClassHintName[] = "tint3";
static char kClassHintClass[] = "Tint3";

}  // namespace

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

int max_tick_urgent;

// panel's initial config
Panel panel_config;
// panels (one panel per monitor)
Panel* panel1;
int nb_panel;

std::vector<Background*> backgrounds;

Imlib_Image default_icon;

namespace {

void UpdateStrut(Panel* p) {
  if (panel_strut_policy == STRUT_NONE) {
    XDeleteProperty(server.dsp, p->main_win_, server.atoms_["_NET_WM_STRUT"]);
    XDeleteProperty(server.dsp, p->main_win_,
                    server.atoms_["_NET_WM_STRUT_PARTIAL"]);
    return;
  }

  // Reserved space
  unsigned int d1, screen_width, screen_height;
  Window d2;
  int d3;
  XGetGeometry(server.dsp, server.root_win, &d2, &d3, &d3, &screen_width,
               &screen_height, &d1, &d1);
  Monitor monitor = server.monitor[p->monitor_];
  long struts[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  if (panel_horizontal) {
    int height = p->height_ + p->margin_y_;

    if (panel_strut_policy == STRUT_MINIMUM ||
        (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden_)) {
      height = p->hidden_height_;
    }

    if (panel_position & TOP) {
      struts[2] = height + monitor.y;
      struts[8] = p->root_x_;
      // p->width - 1 allowed full screen on monitor 2
      struts[9] = p->root_x_ + p->width_ - 1;
    } else {
      struts[3] = height + screen_height - monitor.y - monitor.height;
      struts[10] = p->root_x_;
      // p->width - 1 allowed full screen on monitor 2
      struts[11] = p->root_x_ + p->width_ - 1;
    }
  } else {
    int width = p->width_ + p->margin_x_;

    if (panel_strut_policy == STRUT_MINIMUM ||
        (panel_strut_policy == STRUT_FOLLOW_SIZE && p->is_hidden_)) {
      width = p->hidden_width_;
    }

    if (panel_position & LEFT) {
      struts[0] = width + monitor.x;
      struts[4] = p->root_y_;
      // p->width - 1 allowed full screen on monitor 2
      struts[5] = p->root_y_ + p->height_ - 1;
    } else {
      struts[1] = width + screen_width - monitor.x - monitor.width;
      struts[6] = p->root_y_;
      // p->width - 1 allowed full screen on monitor 2
      struts[7] = p->root_y_ + p->height_ - 1;
    }
  }

  // Old specification : fluxbox need _NET_WM_STRUT.
  XChangeProperty(server.dsp, p->main_win_, server.atoms_["_NET_WM_STRUT"],
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&struts, 4);
  XChangeProperty(server.dsp, p->main_win_,
                  server.atoms_["_NET_WM_STRUT_PARTIAL"], XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char*)&struts, 12);
}

void StopAutohideTimeout(Panel* p) {
  if (p->autohide_timeout_ != nullptr) {
    StopTimeout(p->autohide_timeout_);
    p->autohide_timeout_ = nullptr;
  }
}

}  // namespace

void DefaultPanel() {
  panel1 = nullptr;
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
  panel_dock = 0;              // default not in the dock
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

  // taskbarname_font_desc freed here because cleanup_taskbarname() called on
  // _NET_NUMBER_OF_DESKTOPS
  if (taskbarname_font_desc) {
    pango_font_description_free(taskbarname_font_desc);
  }

  for (int i = 0; i < nb_panel; i++) {
    Panel& p = panel1[i];

    p.FreeArea();

    if (p.temp_pmap) {
      XFreePixmap(server.dsp, p.temp_pmap);
    }

    if (p.hidden_pixmap_) {
      XFreePixmap(server.dsp, p.hidden_pixmap_);
    }

    if (p.main_win_) {
      XDestroyWindow(server.dsp, p.main_win_);
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
  if (panel_config.monitor_ > server.nb_monitor - 1) {
    // server.nb_monitor minimum value is 1 (see get_monitors())
    util::log::Error() << "warning: monitor not found, "
                       << "defaulting to all monitors.\n";
    panel_config.monitor_ = 0;
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
  if (panel_config.monitor_ >= 0) {
    nb_panel = 1;
  } else {
    nb_panel = server.nb_monitor;
  }

  panel1 = new Panel[nb_panel];

  for (int i = 0; i < nb_panel; i++) {
    panel1[i] = panel_config;
  }

  util::log::Debug() << "tint3: nb monitor " << server.nb_monitor
                     << ", nb monitor used " << nb_panel << ", nb desktop "
                     << server.nb_desktop << '\n';

  for (int i = 0; i < nb_panel; i++) {
    auto p = &panel1[i];

    if (panel_config.monitor_ < 0) {
      p->monitor_ = i;
    }

    if (p->bg_ == nullptr) {
      p->bg_ = backgrounds.front();
    }

    p->parent_ = p;
    p->panel_ = p;
    p->on_screen_ = true;
    p->need_resize_ = true;
    p->size_mode_ = kSizeByLayout;
    p->InitSizeAndPosition();

    // add children according to panel_items
    util::log::Debug() << "Setting panel items: " << panel_items_order << '\n';

    for (char item : panel_items_order) {
      if (item == 'L') {
        Launcher::InitPanel(p);
      }

      if (item == 'T') {
        Taskbar::InitPanel(p);
      }

#ifdef ENABLE_BATTERY

      if (item == 'B') {
        Battery::InitPanel(p);
      }

#endif

      if (item == 'S' && i == 0) {
        // TODO : check systray is only on 1 panel
        // at the moment only on panel1[0] allowed
        Systraybar::InitPanel(p);
        refresh_systray = 1;
      }

      if (item == 'C') {
        Clock::InitPanel(p);
      }
    }

    p->SetItemsOrder();

    {
      // catch some events
      XSetWindowAttributes attr;
      std::memset(&attr, 0, sizeof(attr));
      attr.colormap = server.colormap;
      attr.background_pixel = 0;
      attr.border_pixel = 0;

      unsigned long mask =
          CWEventMask | CWColormap | CWBackPixel | CWBorderPixel;
      p->main_win_ = XCreateWindow(
          server.dsp, server.root_win, p->root_x_, p->root_y_, p->width_,
          p->height_, 0, server.depth, InputOutput, server.visual, mask, &attr);
    }

    long event_mask =
        ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;

    if (p->g_task.tooltip_enabled ||
        (launcher_enabled && launcher_tooltip_enabled)) {
      event_mask |= PointerMotionMask | LeaveWindowMask;
    }

    if (panel_autohide) {
      event_mask |= LeaveWindowMask | EnterWindowMask;
    }

    {
      XSetWindowAttributes attr;
      std::memset(&attr, 0, sizeof(attr));
      attr.event_mask = event_mask;
      XChangeWindowAttributes(server.dsp, p->main_win_, CWEventMask, &attr);
    }

    if (!server.gc) {
      XGCValues gcv;
      server.gc = XCreateGC(server.dsp, p->main_win_, 0, &gcv);
    }

    // printf("panel %d : %d, %d, %d, %d\n", i, p->posx, p->posy, p->width,
    // p->height);
    p->SetProperties();
    p->SetBackground();

    if (snapshot_path.empty()) {
      // if we are not in 'snapshot' mode then map new panel
      XMapWindow(server.dsp, p->main_win_);
    }

    if (panel_autohide) {
      AddTimeout(panel_autohide_hide_timeout, 0, AutohideHide, p);
    }

    p->UpdateTaskbarVisibility();
  }

  TaskRefreshTasklist();
  ActiveTask();
}

void Panel::InitSizeAndPosition() {
  // detect panel size
  if (panel_horizontal) {
    if (percent_x) {
      width_ = (float)server.monitor[monitor_].width * width_ / 100;
    }

    if (percent_y) {
      height_ = (float)server.monitor[monitor_].height * height_ / 100;
    }

    if (width_ + margin_x_ > server.monitor[monitor_].width) {
      width_ = server.monitor[monitor_].width - margin_x_;
    }

    if (bg_->border.rounded > height_ / 2) {
      printf(
          "panel_background_id rounded is too big... please fix your "
          "tint3rc\n");
      /* backgrounds.push_back(*bg); */
      /* bg = backgrounds.back(); */
      bg_->border.rounded = height_ / 2;
    }
  } else {
    int old_panel_height = height_;

    if (percent_x) {
      height_ = (float)server.monitor[monitor_].height * width_ / 100;
    } else {
      height_ = width_;
    }

    if (percent_y) {
      width_ = (float)server.monitor[monitor_].width * old_panel_height / 100;
    } else {
      width_ = old_panel_height;
    }

    if (height_ + margin_y_ > server.monitor[monitor_].height) {
      height_ = server.monitor[monitor_].height - margin_y_;
    }

    if (bg_->border.rounded > width_ / 2) {
      printf(
          "panel_background_id rounded is too big... please fix your "
          "tint3rc\n");
      /* backgrounds.push_back(*bg); */
      /* bg = backgrounds.back(); */
      bg_->border.rounded = width_ / 2;
    }
  }

  // panel position determined here
  if (panel_position & LEFT) {
    root_x_ = server.monitor[monitor_].x + margin_x_;
  } else {
    if (panel_position & RIGHT) {
      root_x_ = server.monitor[monitor_].x + server.monitor[monitor_].width -
                width_ - margin_x_;
    } else {
      if (panel_horizontal) {
        root_x_ = server.monitor[monitor_].x +
                  ((server.monitor[monitor_].width - width_) / 2);
      } else {
        root_x_ = server.monitor[monitor_].x + margin_x_;
      }
    }
  }

  if (panel_position & TOP) {
    root_y_ = server.monitor[monitor_].y + margin_y_;
  } else {
    if (panel_position & BOTTOM) {
      root_y_ = server.monitor[monitor_].y + server.monitor[monitor_].height -
                height_ - margin_y_;
    } else {
      root_y_ = server.monitor[monitor_].y +
                ((server.monitor[monitor_].height - height_) / 2);
    }
  }

  // autohide or strut_policy=minimum
  int diff = (panel_horizontal ? height_ : width_) - panel_autohide_height;

  if (panel_horizontal) {
    hidden_width_ = width_;
    hidden_height_ = height_ - diff;
  } else {
    hidden_width_ = width_ - diff;
    hidden_height_ = height_;
  }

  // printf("panel : posx %d, posy %d, width %d, height %d\n", posx, posy,
  // width, height);
}

bool Panel::Resize() {
  ResizeByLayout(0);

  if (panel_mode != MULTI_DESKTOP && taskbar_enabled) {
    // propagate width/height on hidden taskbar
    int width = taskbar_[server.desktop].width_;
    int height = taskbar_[server.desktop].height_;

    for (int i = 0; i < nb_desktop_; ++i) {
      taskbar_[i].width_ = width;
      taskbar_[i].height_ = height;
      taskbar_[i].need_resize_ = true;
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
      children_.push_back(&launcher_);
    }

    if (item == 'T') {
      for (int j = 0; j < nb_desktop_; j++) {
        children_.push_back(&taskbar_[j]);
      }
    }

#ifdef ENABLE_BATTERY

    if (item == 'B') {
      children_.push_back(&battery_);
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
      children_.push_back(&clock_);
    }
  }

  InitRendering(0);
}

void Panel::SetProperties() {
  XStoreName(server.dsp, main_win_, "tint3");

  gsize len;
  gchar* name = g_locale_to_utf8("tint3", -1, nullptr, &len, nullptr);

  if (name != nullptr) {
    XChangeProperty(server.dsp, main_win_, server.atoms_["_NET_WM_NAME"],
                    server.atoms_["UTF8_STRING"], 8, PropModeReplace,
                    (unsigned char*)name, (int)len);
    g_free(name);
  }

  // Dock
  long val = server.atoms_["_NET_WM_WINDOW_TYPE_DOCK"];
  XChangeProperty(server.dsp, main_win_, server.atoms_["_NET_WM_WINDOW_TYPE"],
                  XA_ATOM, 32, PropModeReplace, (unsigned char*)&val, 1);

  // Sticky and below other window
  val = ALLDESKTOP;
  XChangeProperty(server.dsp, main_win_, server.atoms_["_NET_WM_DESKTOP"],
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&val, 1);
  Atom state[4];
  state[0] = server.atoms_["_NET_WM_STATE_SKIP_PAGER"];
  state[1] = server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"];
  state[2] = server.atoms_["_NET_WM_STATE_STICKY"];
  state[3] = panel_layer == BOTTOM_LAYER ? server.atoms_["_NET_WM_STATE_BELOW"]
                                         : server.atoms_["_NET_WM_STATE_ABOVE"];
  int nb_atoms = panel_layer == NORMAL_LAYER ? 3 : 4;
  XChangeProperty(server.dsp, main_win_, server.atoms_["_NET_WM_STATE"],
                  XA_ATOM, 32, PropModeReplace, (unsigned char*)state,
                  nb_atoms);

  // Unfocusable
  XWMHints wmhints;

  if (panel_dock) {
    wmhints.icon_window = wmhints.window_group = main_win_;
    wmhints.flags = StateHint | IconWindowHint;
    wmhints.initial_state = WithdrawnState;
  } else {
    wmhints.flags = InputHint;
    wmhints.input = False;
  }

  XSetWMHints(server.dsp, main_win_, &wmhints);

  // Undecorated
  long prop[5] = {2, 0, 0, 0, 0};
  XChangeProperty(server.dsp, main_win_, server.atoms_["_MOTIF_WM_HINTS"],
                  server.atoms_["_MOTIF_WM_HINTS"], 32, PropModeReplace,
                  (unsigned char*)prop, 5);

  // XdndAware - Register for Xdnd events
  Atom version = 4;
  XChangeProperty(server.dsp, main_win_, server.atoms_["XdndAware"], XA_ATOM,
                  32, PropModeReplace, (unsigned char*)&version, 1);

  UpdateStrut(this);

  // Fixed position and non-resizable window
  // Allow panel move and resize when tint3 reload config file
  int minwidth = panel_autohide ? hidden_width_ : width_;
  int minheight = panel_autohide ? hidden_height_ : height_;
  XSizeHints size_hints;
  size_hints.flags = PPosition | PMinSize | PMaxSize;
  size_hints.min_width = minwidth;
  size_hints.max_width = width_;
  size_hints.min_height = minheight;
  size_hints.max_height = height_;
  XSetWMNormalHints(server.dsp, main_win_, &size_hints);

  // Set WM_CLASS
  util::x11::ClientData<XClassHint> classhint(XAllocClassHint());
  classhint->res_name = kClassHintName;
  classhint->res_class = kClassHintClass;
  XSetClassHint(server.dsp, main_win_, classhint.get());
}

void Panel::SetBackground() {
  if (pix_) {
    XFreePixmap(server.dsp, pix_);
  }

  pix_ =
      XCreatePixmap(server.dsp, server.root_win, width_, height_, server.depth);

  int xoff = 0, yoff = 0;

  if (panel_horizontal && panel_position & BOTTOM) {
    yoff = height_ - hidden_height_;
  } else if (!panel_horizontal && panel_position & RIGHT) {
    xoff = width_ - hidden_width_;
  }

  if (server.real_transparency) {
    ClearPixmap(pix_, 0, 0, width_, height_);
  } else {
    GetRootPixmap();
    // copy background (server.root_pmap) in panel.pix
    Window dummy;
    int x, y;
    XTranslateCoordinates(server.dsp, main_win_, server.root_win, 0, 0, &x, &y,
                          &dummy);

    if (panel_autohide && is_hidden_) {
      x -= xoff;
      y -= yoff;
    }

    XSetTSOrigin(server.dsp, server.gc, -x, -y);
    XFillRectangle(server.dsp, pix_, server.gc, 0, 0, width_, height_);
  }

  // draw background panel
  auto cs = cairo_xlib_surface_create(server.dsp, pix_, server.visual, width_,
                                      height_);
  auto c = cairo_create(cs);
  DrawBackground(c);
  cairo_destroy(c);
  cairo_surface_destroy(cs);

  if (panel_autohide) {
    if (hidden_pixmap_) {
      XFreePixmap(server.dsp, hidden_pixmap_);
    }

    hidden_pixmap_ = XCreatePixmap(server.dsp, server.root_win, hidden_width_,
                                   hidden_height_, server.depth);
    XCopyArea(server.dsp, pix_, hidden_pixmap_, server.gc, xoff, yoff,
              hidden_width_, hidden_height_, 0, 0);
  }

  // redraw panel's object
  for (auto& child : children_) {
    child->SetRedraw();
  }

  // reset task/taskbar 'state_pix'
  for (int i = 0; i < nb_desktop_; i++) {
    auto& tskbar = taskbar_[i];

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

    std::for_each(begin, tskbar.children_.end(), [](Area* child) {
      set_task_redraw(static_cast<Task*>(child));
    });
  }
}

void Panel::UpdateTaskbarVisibility() {
  for (int j = 0; j < nb_desktop_; j++) {
    Taskbar& tskbar = taskbar_[j];

    if (panel_mode != MULTI_DESKTOP && tskbar.desktop != server.desktop) {
      // SINGLE_DESKTOP and not current desktop
      tskbar.on_screen_ = false;
    } else {
      tskbar.on_screen_ = true;
    }
  }

  panel_refresh = 1;
}

Panel* GetPanel(Window win) {
  for (int i = 0; i < nb_panel; ++i) {
    if (panel1[i].main_win_ == win) {
      return &panel1[i];
    }
  }

  return nullptr;
}

Taskbar* Panel::ClickTaskbar(int x, int y) {
  for (int i = 0; i < nb_desktop_; i++) {
    if (taskbar_[i].IsClickInside(x, y)) {
      return &taskbar_[i];
    }
  }

  return nullptr;
}

Task* Panel::ClickTask(int x, int y) {
  Taskbar* tskbar = ClickTaskbar(x, y);

  if (tskbar != nullptr) {
    auto begin = tskbar->children_.begin();

    if (taskbarname_enabled) {
      ++begin;
    }

    for (auto it = begin; it != tskbar->children_.end(); ++it) {
      if ((*it)->IsClickInside(x, y)) {
        return reinterpret_cast<Task*>(*it);
      }
    }
  }

  return nullptr;
}

bool Panel::ClickLauncher(int x, int y) {
  return launcher_.IsClickInside(x, y);
}

LauncherIcon* Panel::ClickLauncherIcon(int x, int y) {
  if (ClickLauncher(x, y)) {
    for (auto& launcher_icon : launcher_.list_icons_) {
      int base_x = (launcher_.panel_x_ + launcher_icon->x_);
      bool inside_x =
          (x >= base_x && x <= (base_x + launcher_icon->icon_size_));
      int base_y = (launcher_.panel_y_ + launcher_icon->y_);
      bool inside_y =
          (y >= base_y && y <= (base_y + launcher_icon->icon_size_));

      if (inside_x && inside_y) {
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

bool Panel::ClickClock(int x, int y) { return clock_.IsClickInside(x, y); }

Area* Panel::ClickArea(int x, int y) {
  Area* new_result = this;
  Area* result = nullptr;

  do {
    result = new_result;

    for (auto& child : result->children_) {
      if (child->IsClickInside(x, y)) {
        new_result = child;
        break;
      }
    }
  } while (new_result != result);

  return result;
}

bool Panel::HandlesClick(XButtonEvent* e) {
  Task* task = ClickTask(e->x, e->y);

  if (task) {
    return ((e->button == 1) || (e->button == 2 && mouse_middle != 0) ||
            (e->button == 3 && mouse_right != 0) ||
            (e->button == 4 && mouse_scroll_up != 0) ||
            (e->button == 5 && mouse_scroll_down != 0));
  }

  LauncherIcon* icon = ClickLauncherIcon(e->x, e->y);

  if (icon != nullptr) {
    return (e->button == 1);
  }

  // no launcher/task clicked --> check if taskbar clicked
  Taskbar* tskbar = ClickTaskbar(e->x, e->y);

  if (tskbar != nullptr && e->button == 1 && panel_mode == MULTI_DESKTOP) {
    return 1;
  }

  if (ClickClock(e->x, e->y)) {
    bool clock_lclick = (e->button == 1 && !clock_lclick_command.empty());
    bool clock_rclick = (e->button == 3 && !clock_rclick_command.empty());
    return (clock_lclick || clock_rclick);
  }

  return false;
}

void AutohideShow(void* p) {
  Panel* panel = static_cast<Panel*>(p);
  StopAutohideTimeout(panel);
  panel->is_hidden_ = 0;

  if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
    UpdateStrut(panel);
  }

  XMapSubwindows(server.dsp, panel->main_win_);  // systray windows

  if (panel_horizontal) {
    if (panel_position & TOP) {
      XResizeWindow(server.dsp, panel->main_win_, panel->width_,
                    panel->height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_, panel->width_, panel->height_);
    }
  } else {
    if (panel_position & LEFT) {
      XResizeWindow(server.dsp, panel->main_win_, panel->width_,
                    panel->height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_, panel->width_, panel->height_);
    }
  }

  // ugly hack, because we actually only need to call XSetBackgroundPixmap
  refresh_systray = 1;
  panel_refresh = 1;
}

void AutohideHide(void* p) {
  Panel* panel = static_cast<Panel*>(p);
  StopAutohideTimeout(panel);
  panel->is_hidden_ = 1;

  if (panel_strut_policy == STRUT_FOLLOW_SIZE) {
    UpdateStrut(panel);
  }

  XUnmapSubwindows(server.dsp, panel->main_win_);  // systray windows
  int diff = (panel_horizontal ? panel->height_ : panel->width_) -
             panel_autohide_height;

  // printf("autohide_hide : diff %d, w %d, h %d\n", diff, panel->hidden_width,
  // panel->hidden_height);
  if (panel_horizontal) {
    if (panel_position & TOP) {
      XResizeWindow(server.dsp, panel->main_win_, panel->hidden_width_,
                    panel->hidden_height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_ + diff, panel->hidden_width_,
                        panel->hidden_height_);
    }
  } else {
    if (panel_position & LEFT) {
      XResizeWindow(server.dsp, panel->main_win_, panel->hidden_width_,
                    panel->hidden_height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_ + diff,
                        panel->root_y_, panel->hidden_width_,
                        panel->hidden_height_);
    }
  }

  panel_refresh = 1;
}

void AutohideTriggerShow(Panel* p) {
  if (!p) {
    return;
  }

  if (p->autohide_timeout_ != nullptr) {
    ChangeTimeout(p->autohide_timeout_, panel_autohide_show_timeout, 0,
                  AutohideShow, p);
  } else {
    p->autohide_timeout_ =
        AddTimeout(panel_autohide_show_timeout, 0, AutohideShow, p);
  }
}

void AutohideTriggerHide(Panel* p) {
  if (!p) {
    return;
  }

  Window root, child;
  int xr, yr, xw, yw;
  unsigned int mask;

  if (XQueryPointer(server.dsp, p->main_win_, &root, &child, &xr, &yr, &xw, &yw,
                    &mask)) {
    if (child) {
      return;  // mouse over one of the system tray icons
    }
  }

  if (p->autohide_timeout_ != nullptr) {
    ChangeTimeout(p->autohide_timeout_, panel_autohide_hide_timeout, 0,
                  AutohideHide, p);
  } else {
    p->autohide_timeout_ =
        AddTimeout(panel_autohide_hide_timeout, 0, AutohideHide, p);
  }
}
