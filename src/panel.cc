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

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "config.hh"
#include "panel.hh"
#include "server.hh"
#include "tooltip.hh"
#include "util/log.hh"
#include "util/x11.hh"

namespace {

char kClassHintName[] = "tint3";
char kClassHintClass[] = "Tint3";

}  // namespace

// --------------------------------------------------
// mouse events
MouseAction mouse_middle;
MouseAction mouse_right;
MouseAction mouse_scroll_up;
MouseAction mouse_scroll_down;
MouseAction mouse_tilt_left;
MouseAction mouse_tilt_right;

PanelMode panel_mode;
int wm_menu;
int panel_dock;
PanelLayer panel_layer;
PanelHorizontalPosition panel_horizontal_position;
PanelVerticalPosition panel_vertical_position;
bool panel_horizontal;
bool panel_refresh;
bool task_dragged;

bool panel_autohide;
int panel_autohide_show_timeout;
int panel_autohide_hide_timeout;
int panel_autohide_height;
PanelStrutPolicy panel_strut_policy;
std::string panel_items_order;

int max_tick_urgent;

// panel's initial config
Panel panel_config;
// panels (one panel per monitor)
std::vector<Panel> panels;

std::vector<Background> backgrounds;

util::imlib2::Image default_icon;

namespace {

void UpdateStrut(Panel* p) {
  if (panel_strut_policy == PanelStrutPolicy::kNone) {
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

    if (panel_strut_policy == PanelStrutPolicy::kMinimum ||
        (panel_strut_policy == PanelStrutPolicy::kFollowSize &&
         p->is_hidden_)) {
      height = p->hidden_height_;
    }

    if (panel_vertical_position == PanelVerticalPosition::kTop) {
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

    if (panel_strut_policy == PanelStrutPolicy::kMinimum ||
        (panel_strut_policy == PanelStrutPolicy::kFollowSize &&
         p->is_hidden_)) {
      width = p->hidden_width_;
    }

    if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
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

}  // namespace

void DefaultPanel() {
  panels.clear();
  default_icon.Free();
  task_dragged = false;
  panel_horizontal = true;
  panel_vertical_position = PanelVerticalPosition::kBottom;
  panel_horizontal_position = PanelHorizontalPosition::kCenter;
  panel_items_order.clear();
  panel_autohide = false;
  panel_autohide_show_timeout = 0;
  panel_autohide_hide_timeout = 0;
  panel_autohide_height = 5;  // for vertical panels this is of course the width
  panel_strut_policy = PanelStrutPolicy::kFollowSize;
  panel_dock = 0;                     // default not in the dock
  panel_layer = PanelLayer::kBottom;  // default is bottom layer
  wm_menu = 0;
  max_tick_urgent = 14;

  // append full transparency background
  backgrounds.clear();
  backgrounds.push_back(Background{});
}

void CleanupPanel() {
  if (panels.empty()) {
    return;
  }

  CleanupTaskbar();

  // taskbarname_font_desc freed here because cleanup_taskbarname() called on
  // _NET_NUMBER_OF_DESKTOPS
  if (taskbarname_font_desc) {
    pango_font_description_free(taskbarname_font_desc);
  }

  for (Panel& p : panels) {
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

  panels.clear();

  if (panel_config.g_task.font_desc) {
    pango_font_description_free(panel_config.g_task.font_desc);
  }

  backgrounds.clear();
}

void InitPanel(Timer& timer) {
  if (panel_config.monitor_ != Panel::kAllMonitors &&
      panel_config.monitor_ > server.num_monitors - 1) {
    // server.num_monitors minimum value is 1 (see get_monitors())
    util::log::Error() << "warning: monitor not found, "
                       << "defaulting to all monitors.\n";
    panel_config.monitor_ = 0;
  }

  InitTooltip();
  InitSystray(timer);
  InitLauncher();
  InitClock(timer);
#ifdef ENABLE_BATTERY
  InitBattery(timer);
#endif
  InitTaskbar();

  // number of panels (one monitor or 'all' monitors)
  unsigned int num_panels = server.num_monitors;
  if (panel_config.monitor_ >= 0) {
    num_panels = 1;
  }

  panels.resize(num_panels);
  panels.shrink_to_fit();
  std::fill(panels.begin(), panels.end(), panel_config);

  util::log::Debug() << "tint3: num_monitors " << server.num_monitors
                     << ", num_monitors used " << panels.size()
                     << ", num_desktops " << server.num_desktops << '\n';

  for (unsigned int i = 0; i < num_panels; ++i) {
    Panel& p = panels[i];

    if (panel_config.monitor_ == Panel::kAllMonitors) {
      p.monitor_ = i;
    }

    p.set_panel_index(i);
    p.parent_ = &p;
    p.panel_ = &p;
    p.on_screen_ = true;
    p.need_resize_ = true;
    p.size_mode_ = SizeMode::kByLayout;
    p.InitSizeAndPosition();

    // add children according to panel_items
    util::log::Debug() << "Setting panel items: " << panel_items_order << '\n';

    for (char item : panel_items_order) {
      if (item == 'L') {
        Launcher::InitPanel(&p);
      }

      if (item == 'T') {
        Taskbar::InitPanel(&p);
      }

#ifdef ENABLE_BATTERY

      if (item == 'B') {
        Battery::InitPanel(&p);
      }

#endif

      if (item == 'S' && i == 0) {
        // TODO : check systray is only on 1 panel
        // at the moment only on panels[0] allowed
        systray.SetParentPanel(&p);
        systray.set_should_refresh(true);
      }

      if (item == 'C') {
        Clock::InitPanel(&p);
      }
    }

    p.SetItemsOrder();

    {
      // catch some events
      XSetWindowAttributes attr;
      std::memset(&attr, 0, sizeof(attr));
      attr.colormap = server.colormap;
      attr.background_pixel = 0;
      attr.border_pixel = 0;

      unsigned long mask =
          CWEventMask | CWColormap | CWBackPixel | CWBorderPixel;
      p.main_win_ = util::x11::CreateWindow(
          server.root_win, p.root_x_, p.root_y_, p.width_, p.height_, 0,
          server.depth, InputOutput, server.visual, mask, &attr);
    }

    long event_mask =
        ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;

    if (p.g_task.tooltip_enabled ||
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
      XChangeWindowAttributes(server.dsp, p.main_win_, CWEventMask, &attr);
    }

    server.InitGC(p.main_win_);
    p.SetProperties();
    p.SetBackground();
    XMapWindow(server.dsp, p.main_win_);

    if (panel_autohide) {
      timer.SetTimeout(std::chrono::milliseconds(panel_autohide_hide_timeout),
                       [&p]() -> bool { return AutohideHide(&p); });
    }

    p.UpdateTaskbarVisibility();
  }

  TaskRefreshTasklist(timer);
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

    if (bg_.border().rounded() > height_ / 2) {
      util::log::Error() << "panel_background_id rounded is too big: please "
                            "fix your tint3rc.\n";
      bg_.border().set_rounded(height_ / 2);
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

    if (bg_.border().rounded() > width_ / 2) {
      util::log::Error() << "panel_background_id rounded is too big: please "
                            "fix your tint3rc.\n";
      bg_.border().set_rounded(width_ / 2);
    }
  }

  // panel position determined here
  if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
    root_x_ = server.monitor[monitor_].x + margin_x_;
  } else if (panel_horizontal_position == PanelHorizontalPosition::kRight) {
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

  if (panel_vertical_position == PanelVerticalPosition::kTop) {
    root_y_ = server.monitor[monitor_].y + margin_y_;
  } else if (panel_vertical_position == PanelVerticalPosition::kBottom) {
    root_y_ = server.monitor[monitor_].y + server.monitor[monitor_].height -
              height_ - margin_y_;
  } else {
    root_y_ = server.monitor[monitor_].y +
              ((server.monitor[monitor_].height - height_) / 2);
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
}

bool Panel::Resize() {
  ResizeByLayout(0);

  if (panel_mode != PanelMode::kMultiDesktop && taskbar_enabled) {
    // propagate width/height on hidden taskbar
    int width = taskbar_[server.desktop].width_;
    int height = taskbar_[server.desktop].height_;

    for (unsigned int i = 0; i < nb_desktop_; ++i) {
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
      for (unsigned int j = 0; j < nb_desktop_; j++) {
        children_.push_back(&taskbar_[j]);
      }
    }

#ifdef ENABLE_BATTERY

    if (item == 'B') {
      children_.push_back(&battery_);
    }

#endif

    if (item == 'S' && panel_index_ == 0) {
      // TODO : check systray is only on 1 panel
      // at the moment only on panels[0] allowed
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
  val = kAllDesktops;
  XChangeProperty(server.dsp, main_win_, server.atoms_["_NET_WM_DESKTOP"],
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&val, 1);
  Atom state[4];
  state[0] = server.atoms_["_NET_WM_STATE_SKIP_PAGER"];
  state[1] = server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"];
  state[2] = server.atoms_["_NET_WM_STATE_STICKY"];
  state[3] = panel_layer == PanelLayer::kBottom
                 ? server.atoms_["_NET_WM_STATE_BELOW"]
                 : server.atoms_["_NET_WM_STATE_ABOVE"];
  int nb_atoms = panel_layer == PanelLayer::kNormal ? 3 : 4;
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
  if (pix_ != None) {
    XFreePixmap(server.dsp, pix_);
  }

  pix_ =
      XCreatePixmap(server.dsp, server.root_win, width_, height_, server.depth);

  int xoff = 0, yoff = 0;

  if (panel_horizontal &&
      panel_vertical_position == PanelVerticalPosition::kBottom) {
    yoff = height_ - hidden_height_;
  } else if (!panel_horizontal &&
             panel_horizontal_position == PanelHorizontalPosition::kRight) {
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
  for (unsigned int i = 0; i < nb_desktop_; i++) {
    auto& tskbar = taskbar_[i];

    for (int k = 0; k < kTaskbarCount; ++k) {
      tskbar.reset_state_pixmap(k);
      tskbar.bar_name.reset_state_pixmap(k);
    }

    tskbar.pix_ = None;
    tskbar.bar_name.pix_ = None;

    auto begin = tskbar.children_.begin();

    if (taskbarname_enabled) {
      ++begin;
    }

    std::for_each(begin, tskbar.children_.end(), [](Area* child) {
      SetTaskRedraw(static_cast<Task*>(child));
    });
  }
}

void Panel::UpdateTaskbarVisibility() {
  for (unsigned int j = 0; j < nb_desktop_; j++) {
    Taskbar& tskbar = taskbar_[j];

    if (panel_mode != PanelMode::kMultiDesktop &&
        tskbar.desktop != server.desktop) {
      // SINGLE_DESKTOP and not current desktop
      tskbar.on_screen_ = false;
    } else {
      tskbar.on_screen_ = true;
    }
  }

  panel_refresh = true;
}

Panel* GetPanel(Window win) {
  for (Panel& p : panels) {
    if (p.main_win_ == win) {
      return &p;
    }
  }
  return nullptr;
}

Taskbar* Panel::ClickTaskbar(int x, int y) {
  for (unsigned int i = 0; i < nb_desktop_; i++) {
    if (taskbar_[i].IsPointInside(x, y)) {
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
      if ((*it)->IsPointInside(x, y)) {
        return reinterpret_cast<Task*>(*it);
      }
    }
  }

  return nullptr;
}

bool Panel::ClickLauncher(int x, int y) {
  return launcher_.IsPointInside(x, y);
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

bool Panel::ClickClock(int x, int y) { return clock_.IsPointInside(x, y); }

bool Panel::HandlesClick(XButtonEvent* e) {
  Task* task = ClickTask(e->x, e->y);

  if (task) {
    return ((e->button == 1) ||
            (e->button == 2 && mouse_middle != MouseAction::kNone) ||
            (e->button == 3 && mouse_right != MouseAction::kNone) ||
            (e->button == 4 && mouse_scroll_up != MouseAction::kNone) ||
            (e->button == 5 && mouse_scroll_down != MouseAction::kNone));
  }

  LauncherIcon* icon = ClickLauncherIcon(e->x, e->y);

  if (icon != nullptr) {
    return (e->button == 1);
  }

  // no launcher/task clicked --> check if taskbar clicked
  Taskbar* tskbar = ClickTaskbar(e->x, e->y);

  if (tskbar != nullptr && e->button == 1 &&
      panel_mode == PanelMode::kMultiDesktop) {
    return 1;
  }

  if (ClickClock(e->x, e->y)) {
    bool clock_lclick = (e->button == 1 && !clock_lclick_command.empty());
    bool clock_rclick = (e->button == 3 && !clock_rclick_command.empty());
    return (clock_lclick || clock_rclick);
  }

  return false;
}

void Panel::set_panel_index(unsigned int i) { panel_index_ = i; }

#ifdef _TINT3_DEBUG

std::string Panel::GetFriendlyName() const { return "Panel"; }

#endif  // _TINT3_DEBUG

bool AutohideShow(Panel* panel) {
  panel->is_hidden_ = 0;

  if (panel_strut_policy == PanelStrutPolicy::kFollowSize) {
    UpdateStrut(panel);
  }

  XMapSubwindows(server.dsp, panel->main_win_);  // systray windows

  if (panel_horizontal) {
    if (panel_vertical_position == PanelVerticalPosition::kTop) {
      XResizeWindow(server.dsp, panel->main_win_, panel->width_,
                    panel->height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_, panel->width_, panel->height_);
    }
  } else {
    if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
      XResizeWindow(server.dsp, panel->main_win_, panel->width_,
                    panel->height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_, panel->width_, panel->height_);
    }
  }

  // ugly hack, because we actually only need to call XSetBackgroundPixmap
  systray.set_should_refresh(true);
  panel_refresh = true;
  return false;
}

bool AutohideHide(Panel* panel) {
  panel->is_hidden_ = 1;

  if (panel_strut_policy == PanelStrutPolicy::kFollowSize) {
    UpdateStrut(panel);
  }

  XUnmapSubwindows(server.dsp, panel->main_win_);  // systray windows
  int diff = (panel_horizontal ? panel->height_ : panel->width_) -
             panel_autohide_height;

  if (panel_horizontal) {
    if (panel_vertical_position == PanelVerticalPosition::kTop) {
      XResizeWindow(server.dsp, panel->main_win_, panel->hidden_width_,
                    panel->hidden_height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_,
                        panel->root_y_ + diff, panel->hidden_width_,
                        panel->hidden_height_);
    }
  } else {
    if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
      XResizeWindow(server.dsp, panel->main_win_, panel->hidden_width_,
                    panel->hidden_height_);
    } else {
      XMoveResizeWindow(server.dsp, panel->main_win_, panel->root_x_ + diff,
                        panel->root_y_, panel->hidden_width_,
                        panel->hidden_height_);
    }
  }

  panel_refresh = true;
  return false;
}

void AutohideTriggerShow(Panel* p, Timer& timer) {
  if (!p) {
    return;
  }

  p->autohide_timeout_ =
      timer.SetTimeout(std::chrono::milliseconds(panel_autohide_show_timeout),
                       [p]() -> bool { return AutohideShow(p); });
}

void AutohideTriggerHide(Panel* p, Timer& timer) {
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

  p->autohide_timeout_ =
      timer.SetTimeout(std::chrono::milliseconds(panel_autohide_hide_timeout),
                       [p]() -> bool { return AutohideHide(p); });
}
