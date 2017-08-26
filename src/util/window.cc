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
#include <pango/pangocairo.h>

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
  SendEvent32(win, server.atom("_NET_ACTIVE_WINDOW"), 2, CurrentTime, 0);
}

int GetDesktop(Window win) {
  return GetProperty32<int>(win, server.atom("_NET_WM_DESKTOP"), XA_CARDINAL);
}

void SetDesktop(Window win, int desktop) {
  SendEvent32(win, server.atom("_NET_WM_DESKTOP"), desktop, 2, 0);
}

void SetClose(Window win) {
  SendEvent32(win, server.atom("_NET_CLOSE_WINDOW"), 0, 2, 0);
}

void ToggleShade(Window win) {
  SendEvent32(win, server.atom("_NET_WM_STATE"), 2,
              server.atom("_NET_WM_STATE_SHADED"), 0);
}

void MaximizeRestore(Window win) {
  SendEvent32(win, server.atom("_NET_WM_STATE"), 2,
              server.atom("_NET_WM_STATE_MAXIMIZED_VERT"), 0);
  SendEvent32(win, server.atom("_NET_WM_STATE"), 2,
              server.atom("_NET_WM_STATE_MAXIMIZED_HORZ"), 0);
}

bool IsHidden(Window win) {
  int state_count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atom("_NET_WM_STATE"), XA_ATOM,
                                    &state_count);

  for (int i = 0; i < state_count; ++i) {
    if (at.get()[i] == server.atom("_NET_WM_STATE_SKIP_TASKBAR")) {
      return true;
    }

    // do not add transient_for windows if the transient window is already in
    // the taskbar
    Window window = win;

    while (XGetTransientForHint(server.dsp, window, &window)) {
      if (!TaskGetTasks(window).empty()) {
        return true;
      }
    }
  }

  int type_count = 0;
  at = ServerGetProperty<Atom>(win, server.atom("_NET_WM_WINDOW_TYPE"), XA_ATOM,
                               &type_count);

  for (int i = 0; i < type_count; ++i) {
    if (at.get()[i] == server.atom("_NET_WM_WINDOW_TYPE_DOCK") ||
        at.get()[i] == server.atom("_NET_WM_WINDOW_TYPE_DESKTOP") ||
        at.get()[i] == server.atom("_NET_WM_WINDOW_TYPE_TOOLBAR") ||
        at.get()[i] == server.atom("_NET_WM_WINDOW_TYPE_MENU") ||
        at.get()[i] == server.atom("_NET_WM_WINDOW_TYPE_SPLASH")) {
      return true;
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
  return false;
}

namespace {

bool IsInsideMonitor(int x, int y, Monitor const& monitor) {
  bool inside_x =
      (x >= monitor.x && x <= monitor.x + static_cast<int>(monitor.width));
  bool inside_y =
      (y >= monitor.y && y <= monitor.y + static_cast<int>(monitor.height));
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
  XTranslateCoordinates(server.dsp, win, server.root_window(), 0, 0, &x, &y,
                        &src);

  int i = FindMonitorIndex(x + 2, y + 2);
  return (i != -1) ? i : 0;
}

bool IsIconified(Window win) {
  // EWMH specification : minimization of windows use _NET_WM_STATE_HIDDEN.
  // WM_STATE is not accurate for shaded window and in multi_desktop mode.
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atom("_NET_WM_STATE"), XA_ATOM,
                                    &count);

  for (int i = 0; i < count; i++) {
    if (at.get()[i] == server.atom("_NET_WM_STATE_HIDDEN")) {
      return true;
    }
  }

  return false;
}

bool IsUrgent(Window win) {
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atom("_NET_WM_STATE"), XA_ATOM,
                                    &count);

  for (int i = 0; i < count; i++) {
    if (at.get()[i] == server.atom("_NET_WM_STATE_DEMANDS_ATTENTION")) {
      return true;
    }
  }

  return false;
}

bool IsSkipTaskbar(Window win) {
  int count = 0;
  auto at = ServerGetProperty<Atom>(win, server.atom("_NET_WM_STATE"), XA_ATOM,
                                    &count);

  for (int i = 0; i < count; ++i) {
    if (at.get()[i] == server.atom("_NET_WM_STATE_SKIP_TASKBAR")) {
      return true;
    }
  }

  return false;
}

Window GetActive() {
  return GetProperty32<Window>(server.root_window(),
                               server.atom("_NET_ACTIVE_WINDOW"), XA_WINDOW);
}

bool IsActive(Window win) { return GetActive() == win; }

}  // namespace util
}  // namespace window

void SetDesktop(int desktop) {
  SendEvent32(server.root_window(), server.atom("_NET_CURRENT_DESKTOP"),
              desktop, 0, 0);
}

int GetIconCount(unsigned long* data, int num) {
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

unsigned long* GetBestIcon(unsigned long* data, int icon_count, int num,
                           int* iw, int* ih, int best_icon_size) {
  int width[icon_count];
  int height[icon_count];
  unsigned long* icon_data[icon_count];

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

void GetTextSize(util::pango::FontDescriptionPtr const& font,
                 std::string const& text, int* width, int* height) {
  cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* c = cairo_create(cs);

  util::GObjectPtr<PangoLayout> layout{pango_cairo_create_layout(c)};
  pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_NONE);
  pango_layout_set_font_description(layout.get(), font());
  pango_layout_set_text(layout.get(), text.c_str(), -1);

  PangoRectangle r1, r2;
  pango_layout_get_pixel_extents(layout.get(), &r1, &r2);
  if (width) {
    (*width) = r2.width;
  }
  if (height) {
    (*height) = r2.height;
  }

  cairo_destroy(c);
  cairo_surface_destroy(cs);
}
