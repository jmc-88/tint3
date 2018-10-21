/**************************************************************************
 *
 * Tint3 panel
 *
 * Copyright (C) 2007 Pål Staurland (staura@gmail.com)
 * Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
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

#include <X11/extensions/Xrandr.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"

#include "config.hh"
#include "server.hh"
#include "util/common.hh"
#include "util/log.hh"
#include "util/window.hh"

namespace {

static constexpr char const* const kAtomList[] = {
    /* X11 */
    "_XEMBED", "_XEMBED_INFO", "_XROOTMAP_ID", "_XROOTPMAP_ID",
    "_XSETTINGS_SETTINGS",

    /* NETWM */
    "_NET_ACTIVE_WINDOW", "_NET_CLIENT_LIST", "_NET_CLOSE_WINDOW",
    "_NET_CURRENT_DESKTOP", "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_NAMES",
    "_NET_DESKTOP_VIEWPORT", "_NET_NUMBER_OF_DESKTOPS",
    "_NET_SUPPORTING_WM_CHECK", "_NET_SYSTEM_TRAY_MESSAGE_DATA",
    "_NET_SYSTEM_TRAY_OPCODE", "_NET_SYSTEM_TRAY_ORIENTATION",
    "_NET_SYSTEM_TRAY_VISUAL", "_NET_WM_DESKTOP", "_NET_WM_ICON",
    "_NET_WM_ICON_GEOMETRY", "_NET_WM_NAME", "_NET_WM_PID", "_NET_WM_STATE",
    "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_BELOW",
    "_NET_WM_STATE_DEMANDS_ATTENTION", "_NET_WM_STATE_HIDDEN",
    "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_STATE_MODAL", "_NET_WM_STATE_SHADED", "_NET_WM_STATE_SKIP_PAGER",
    "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_STICKY", "_NET_WM_STRUT",
    "_NET_WM_STRUT_PARTIAL", "_NET_WM_VISIBLE_NAME", "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_DOCK", "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",

    /* Window Manager */
    "WM_HINTS", "WM_NAME", "WM_STATE",

    /* Drag and Drop */
    "TARGETS", "XdndActionCopy", "XdndAware", "XdndDrop", "XdndEnter",
    "XdndFinished", "XdndLeave", "XdndPosition", "XdndSelection", "XdndStatus",
    "XdndTypeList",

    /* Miscellaneous */
    "MANAGER", "UTF8_STRING", "_MOTIF_WM_HINTS", "__SWM_VROOT"};

static constexpr int kAtomCount = (sizeof(kAtomList) / sizeof(kAtomList[0]));

}  // namespace

Server server;

void Server::InitAtoms() {
  Atom atom_list[kAtomCount] = {None};

  if (!XInternAtoms(dsp, (char**)kAtomList, kAtomCount, False, atom_list)) {
    util::log::Error() << "tint3: XInternAtoms failed\n";
  }

  atoms_.clear();

  for (int i = 0; i < kAtomCount; ++i) {
    atoms_.insert(std::make_pair(kAtomList[i], atom_list[i]));
  }

  std::string name = absl::StrCat("_NET_WM_CM_S", DefaultScreen(dsp));
  Atom atom = XInternAtom(dsp, name.c_str(), False);
  atoms_.insert(std::make_pair("_NET_WM_CM_SCREEN", atom));

  if (atom == None) {
    util::log::Error() << "tint3: XInternAtom(\"" << name << "\") failed\n";
  }

  name = absl::StrCat("_XSETTINGS_S", DefaultScreen(dsp));
  atom = XInternAtom(dsp, name.c_str(), False);
  atoms_.insert(std::make_pair("_XSETTINGS_SCREEN", atom));

  if (atom == None) {
    util::log::Error() << "tint3: XInternAtom(\"" << name << "\") failed\n";
  }

  name = absl::StrCat("_NET_SYSTEM_TRAY_S", DefaultScreen(dsp));
  atom = XInternAtom(dsp, name.c_str(), False);
  atoms_.insert(std::make_pair("_NET_SYSTEM_TRAY_SCREEN", atom));

  if (atom == None) {
    util::log::Error() << "tint3: XInternAtom(\"" << name << "\") failed\n";
  }
}

void Server::Cleanup() {
  colormap = {};
  colormap32 = {};
  monitor.clear();

  if (gc) {
    XFreeGC(dsp, gc);
    gc = nullptr;
  }

  if (dsp) {
    XCloseDisplay(dsp);
    dsp = nullptr;
  }
}

void SendEvent32(Window win, Atom at, long data1, long data2, long data3) {
  XEvent event;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.display = server.dsp;
  event.xclient.window = win;
  event.xclient.message_type = at;

  event.xclient.format = 32;
  event.xclient.data.l[0] = data1;
  event.xclient.data.l[1] = data2;
  event.xclient.data.l[2] = data3;
  event.xclient.data.l[3] = 0;
  event.xclient.data.l[4] = 0;

  XSendEvent(server.dsp, server.root_window(), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void Server::GetRootPixmap() {
  Pixmap ret = None;
  Atom pixmap_atoms[] = {atom("_XROOTPMAP_ID"), atom("_XROOTMAP_ID")};

  for (Atom const& atom : pixmap_atoms) {
    auto res = GetProperty<Pixmap>(root_window(), atom, XA_PIXMAP, 0);

    if (res != nullptr) {
      ret = (*res);
      break;
    }
  }

  root_pmap = ret;
  if (root_pmap == None) {
    util::log::Error() << "tint3: pixmap background detection failed\n";
    return;
  }

  XGCValues gcv;
  gcv.ts_x_origin = 0;
  gcv.ts_y_origin = 0;
  gcv.fill_style = FillTiled;
  uint mask = GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle | GCTile;

  gcv.tile = root_pmap;
  XChangeGC(dsp, gc, mask, &gcv);
}

bool MonitorIncludes(Monitor const& m1, Monitor const& m2) {
  bool inside_x = (m1.x >= m2.x) && ((m1.x + m1.width) <= (m2.x + m2.width));
  bool inside_y = (m1.y >= m2.y) && ((m1.y + m1.height) <= (m2.y + m2.height));
  return (!inside_x || !inside_y);
}

void GetMonitors() {
  using CrtcInfoPtr = std::unique_ptr<XRRCrtcInfo, decltype(&XRRFreeCrtcInfo)>;
  using OutputInfoPtr =
      std::unique_ptr<XRROutputInfo, decltype(&XRRFreeOutputInfo)>;
  using ScreenResourcesPtr =
      std::unique_ptr<XRRScreenResources, decltype(&XRRFreeScreenResources)>;

  int num_monitors = 0;

  if (XineramaIsActive(server.dsp)) {
    util::x11::ClientData<XineramaScreenInfo> info(
        XineramaQueryScreens(server.dsp, &num_monitors));
    ScreenResourcesPtr res{
        XRRGetScreenResourcesCurrent(server.dsp, server.root_window()),
        XRRFreeScreenResources};

    if (res != nullptr && res->ncrtc >= num_monitors) {
      // use xrandr to identify monitors (does not work with proprietery nvidia
      // drivers)
      util::log::Debug() << "XRandR: found CRTCs: " << res->ncrtc << '\n';
      server.monitor.clear();

      for (int i = 0; i < res->ncrtc; ++i) {
        CrtcInfoPtr crtc_info{
            XRRGetCrtcInfo(server.dsp, res.get(), res->crtcs[i]),
            XRRFreeCrtcInfo};

        // Skip disabled outputs, which XRandR indicates with a width and height
        // of zero. For reference:
        //  https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt?id=57d3ab1aa7daea9b351dd8bf41ad94522c786d79#n975
        if (crtc_info->width == 0 || crtc_info->height == 0) {
          util::log::Debug()
              << "XRandR: skipping disabled output " << i << '\n';
          continue;
        }

        Monitor current_monitor;
        current_monitor.number = i;
        current_monitor.x = crtc_info->x;
        current_monitor.y = crtc_info->y;
        current_monitor.width = crtc_info->width;
        current_monitor.height = crtc_info->height;

        for (int j = 0; j < crtc_info->noutput; ++j) {
          OutputInfoPtr output_info{
              XRRGetOutputInfo(server.dsp, res.get(), crtc_info->outputs[j]),
              XRRFreeOutputInfo};
          util::log::Debug() << "XRandR: linking output " << output_info->name
                             << " with CRTC " << i << '\n';
          current_monitor.names.push_back(output_info->name);
        }

        server.monitor.push_back(current_monitor);
      }

      num_monitors = server.monitor.size();
    } else if (info != nullptr && num_monitors > 0) {
      server.monitor.resize(num_monitors);

      for (int i = 0; i < num_monitors; ++i) {
        server.monitor[i].x = info.get()[i].x_org;
        server.monitor[i].y = info.get()[i].y_org;
        server.monitor[i].width = info.get()[i].width;
        server.monitor[i].height = info.get()[i].height;
        server.monitor[i].names.clear();
      }
    }

    // order monitors
    std::sort(server.monitor.begin(), server.monitor.end(), MonitorIncludes);

    int i;

    // remove monitor included into another one
    for (i = 0; i < num_monitors; ++i) {
      for (int j = 0; j < i; ++j) {
        if (!MonitorIncludes(server.monitor[i], server.monitor[j])) {
          goto next;
        }
      }
    }

  next:
    server.num_monitors = i;
    server.monitor.resize(server.num_monitors);
    std::sort(server.monitor.begin(), server.monitor.end(), MonitorIncludes);
  }

  if (!server.num_monitors) {
    server.num_monitors = 1;
    server.monitor.resize(server.num_monitors);
    server.monitor[0].x = server.monitor[0].y = 0;
    server.monitor[0].width = DisplayWidth(server.dsp, server.screen);
    server.monitor[0].height = DisplayHeight(server.dsp, server.screen);
    server.monitor[0].names.clear();
  }
}

void Server::InitDesktops() {
  // detect number of desktops
  // wait 15s to leave some time for window manager startup
  for (int i = 0; i < 15; i++) {
    num_desktops_ = GetNumberOfDesktops();

    if (num_desktops_ > 0) {
      break;
    }

    absl::SleepFor(absl::Seconds(1));
  }

  if (num_desktops_ == 0) {
    num_desktops_ = 1;
    util::log::Error() << "WM doesn't respect NETWM specs. "
                       << "tint3 will default to 1 desktop.\n";
  }
}

unsigned int Server::desktop() const { return desktop_; }

void Server::UpdateCurrentDesktop() {
  desktop_ = GetProperty32<int>(root_window_, atoms_["_NET_CURRENT_DESKTOP"],
                                XA_CARDINAL);
}

unsigned int Server::num_desktops() const { return num_desktops_; }

void Server::UpdateNumberOfDesktops() {
  num_desktops_ = GetNumberOfDesktops();

  if (desktop_ >= num_desktops_) {
    desktop_ = num_desktops_ - 1;
  }
}

int Server::GetNumberOfDesktops() {
  return GetProperty32<int>(root_window_, atoms_["_NET_NUMBER_OF_DESKTOPS"],
                            XA_CARDINAL);
}

std::vector<std::string> Server::GetDesktopNames() const {
  int count = 0;
  auto data_ptr = ServerGetProperty<char>(
      root_window(), atom("_NET_DESKTOP_NAMES"), atom("UTF8_STRING"), &count);

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

  // fill in missing desktop names with desktop numbers
  for (unsigned int j = names.size(); j < num_desktops(); ++j) {
    names.push_back(util::string::Representation(j + 1));
  }

  return names;
}

void Server::InitGC(Window win) {
  if (!gc) {
    XGCValues gcv;
    gc = XCreateGC(dsp, win, 0, &gcv);
  }
}

void Server::InitVisual() {
  // check composite manager
  composite_manager = XGetSelectionOwner(dsp, atoms_["_NET_WM_CM_SCREEN"]);

  Visual* xvi_visual = util::x11::GetTrueColorVisual(dsp, screen);
  if (xvi_visual) {
    visual32 = xvi_visual;
    colormap32 =
        util::x11::Colormap::Create(dsp, root_window_, xvi_visual, AllocNone);
  }

  if (xvi_visual && composite_manager != None) {
    XSetWindowAttributes attrs;
    attrs.event_mask = StructureNotifyMask;
    XChangeWindowAttributes(dsp, composite_manager, CWEventMask, &attrs);

    real_transparency = true;
    depth = 32;
    std::cout << "Real transparency: on, depth: " << depth << '\n';
    colormap =
        util::x11::Colormap::Create(dsp, root_window_, xvi_visual, AllocNone);
    visual = xvi_visual;
  } else {
    // no composite manager -> fake transparency
    real_transparency = false;
    depth = DefaultDepth(dsp, screen);
    std::cout << "Real transparency: off, depth: " << depth << '\n';
    colormap = util::x11::Colormap::DefaultForScreen(dsp, screen);
    visual = DefaultVisual(dsp, screen);
  }
}

void Server::InitX11() {
  InitAtoms();
  screen = DefaultScreen(dsp);
  UpdateRootWindow();
  UpdateCurrentDesktop();
  InitVisual();
}

Window Server::root_window() const { return root_window_; }

void Server::UpdateRootWindow() {
  root_window_ = RootWindow(dsp, screen);
  XSelectInput(dsp, root_window_, PropertyChangeMask | StructureNotifyMask);
}

Atom Server::atom(std::string const& name) const { return atoms_.at(name); }
