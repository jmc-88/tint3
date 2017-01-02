/**************************************************************************
*
* Tint3 : common windows function
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
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

#include <Imlib2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "panel.hh"
#include "server.hh"
#include "taskbar/taskbar.hh"
#include "util/common.hh"
#include "util/window.hh"

namespace util {
namespace window {

void SetActive(Window win) {
  SendEvent32(win, server.atoms_["_NET_ACTIVE_WINDOW"], 2, CurrentTime, 0);
}

int GetDesktop(Window win) {
  return GetProperty32<int>(win, server.atoms_["_NET_WM_DESKTOP"], XA_CARDINAL);
}

void SetDesktop(Window win, int desktop) {
  SendEvent32(win, server.atoms_["_NET_WM_DESKTOP"], desktop, 2, 0);
}

void SetClose(Window win) {
  SendEvent32(win, server.atoms_["_NET_CLOSE_WINDOW"], 0, 2, 0);
}

void ToggleShade(Window win) {
  SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
              server.atoms_["_NET_WM_STATE_SHADED"], 0);
}

void MaximizeRestore(Window win) {
  SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
              server.atoms_["_NET_WM_STATE_MAXIMIZED_VERT"], 0);
  SendEvent32(win, server.atoms_["_NET_WM_STATE"], 2,
              server.atoms_["_NET_WM_STATE_MAXIMIZED_HORZ"], 0);
}

int IsHidden(Window win) {
  int state_count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atoms_["_NET_WM_STATE"],
                                    XA_ATOM, &state_count);

  for (int i = 0; i < state_count; ++i) {
    if (at.get()[i] == server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"]) {
      return 1;
    }

    // do not add transient_for windows if the transient window is already in
    // the taskbar
    Window window = win;

    while (XGetTransientForHint(server.dsp, window, &window)) {
      if (!TaskGetTasks(window).empty()) {
        return 1;
      }
    }
  }

  int type_count = 0;
  at = ServerGetProperty<Atom>(win, server.atoms_["_NET_WM_WINDOW_TYPE"],
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

  for (Panel& p : panels) {
    if (p.main_win_ == win) {
      return 1;
    }
  }

  // specification
  // Windows with neither _NET_WM_WINDOW_TYPE nor WM_TRANSIENT_FOR set
  // MUST be taken as top-level window.
  return 0;
}

namespace {

bool IsInsideMonitor(int x, int y, Monitor const& monitor) {
  bool inside_x = (x >= monitor.x && x <= monitor.x + monitor.width);
  bool inside_y = (y >= monitor.y && y <= monitor.y + monitor.height);
  return (inside_x && inside_y);
}

int FindMonitorIndex(int x, int y) {
  for (unsigned int i = 0; i < server.num_monitors; i++) {
    if (IsInsideMonitor(x, y, server.monitor[i])) {
      return i;
    }
  }
  return -1;
}

}  // namespace

unsigned int GetMonitor(Window win) {
  int x, y;
  Window src;
  XTranslateCoordinates(server.dsp, win, server.root_win, 0, 0, &x, &y, &src);

  int i = FindMonitorIndex(x + 2, y + 2);
  return (i != -1) ? i : 0;
}

int IsIconified(Window win) {
  // EWMH specification : minimization of windows use _NET_WM_STATE_HIDDEN.
  // WM_STATE is not accurate for shaded window and in multi_desktop mode.
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atoms_["_NET_WM_STATE"],
                                    XA_ATOM, &count);

  for (int i = 0; i < count; i++) {
    if (at.get()[i] == server.atoms_["_NET_WM_STATE_HIDDEN"]) {
      return 1;
    }
  }

  return 0;
}

int IsUrgent(Window win) {
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atoms_["_NET_WM_STATE"],
                                    XA_ATOM, &count);

  for (int i = 0; i < count; i++) {
    if (at.get()[i] == server.atoms_["_NET_WM_STATE_DEMANDS_ATTENTION"]) {
      return 1;
    }
  }

  return 0;
}

int IsSkipTaskbar(Window win) {
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atoms_["_NET_WM_STATE"],
                                    XA_ATOM, &count);

  for (int i = 0; i < count; ++i) {
    if (at.get()[i] == server.atoms_["_NET_WM_STATE_SKIP_TASKBAR"]) {
      return 1;
    }
  }

  return 0;
}

Window GetActive() {
  return GetProperty32<Window>(server.root_win,
                               server.atoms_["_NET_ACTIVE_WINDOW"], XA_WINDOW);
}

int IsActive(Window win) { return GetActive() == win; }

}  // namespace util
}  // namespace window

void SetDesktop(int desktop) {
  SendEvent32(server.root_win, server.atoms_["_NET_CURRENT_DESKTOP"], desktop,
              0, 0);
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

std::vector<std::string> ServerGetDesktopNames() {
  int count = 0;
  auto data_ptr = ServerGetProperty<char>(server.root_win,
                                          server.atoms_["_NET_DESKTOP_NAMES"],
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

void GetTextSize(PangoFontDescription* font, int* height_ink, int* height,
                 int panel_height, std::string const& text) {
  auto pmap = XCreatePixmap(server.dsp, server.root_win, panel_height,
                            panel_height, server.depth);

  auto cs = cairo_xlib_surface_create(server.dsp, pmap, server.visual,
                                      panel_height, panel_height);
  auto c = cairo_create(cs);

  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), font);
  pango_layout_set_text(layout.get(), text.c_str(), text.length());

  PangoRectangle rect_ink, rect;
  pango_layout_get_pixel_extents(layout.get(), &rect_ink, &rect);
  (*height_ink) = rect_ink.height;
  (*height) = rect.height;

  cairo_destroy(c);
  cairo_surface_destroy(cs);
  XFreePixmap(server.dsp, pmap);
}

void GetTextSize2(PangoFontDescription* font, int* height_ink, int* height,
                  int* width, int panel_height, int panel_width,
                  std::string const& text) {
  auto pmap = XCreatePixmap(server.dsp, server.root_win, panel_height,
                            panel_height, server.depth);

  auto cs = cairo_xlib_surface_create(server.dsp, pmap, server.visual,
                                      panel_height, panel_width);
  auto c = cairo_create(cs);

  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), font);
  pango_layout_set_text(layout.get(), text.c_str(), text.length());

  PangoRectangle rect_ink, rect;
  pango_layout_get_pixel_extents(layout.get(), &rect_ink, &rect);
  (*height_ink) = rect_ink.height;
  (*height) = rect.height;
  (*width) = rect.width;

  cairo_destroy(c);
  cairo_surface_destroy(cs);
  XFreePixmap(server.dsp, pmap);
}
