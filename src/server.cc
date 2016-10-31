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
#include <X11/extensions/Xrender.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "config.h"
#include "server.h"
#include "util/common.h"
#include "util/log.h"
#include "util/window.h"

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
    "_NET_SYSTEM_TRAY_VISUAL", "_NET_WM_CM_S0", "_NET_WM_DESKTOP",
    "_NET_WM_ICON", "_NET_WM_ICON_GEOMETRY", "_NET_WM_NAME", "_NET_WM_PID",
    "_NET_WM_STATE", "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_BELOW",
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

int ServerCatchError(Display* d, XErrorEvent* ev) {
  char error_text[1024 + 1] = {'\0'};
  XGetErrorText(d, ev->error_code, error_text, sizeof(error_text) - 1);

  util::log::Error() << " -> Xlib error ("
                     << static_cast<unsigned int>(ev->error_code)
                     << "): " << error_text << '\n';
  return 0;
}

void Server::InitAtoms() {
  Atom atom_list[kAtomCount] = {None};

  if (!XInternAtoms(dsp, (char**)kAtomList, kAtomCount, False, atom_list)) {
    util::log::Error() << "tint3: XInternAtoms failed\n";
  }

  atoms_.clear();

  for (int i = 0; i < kAtomCount; ++i) {
    atoms_.insert(std::make_pair(kAtomList[i], atom_list[i]));
  }

  std::string name;
  Atom atom;

  name.assign(StringBuilder() << "_XSETTINGS_S" << DefaultScreen(dsp));
  atom = XInternAtom(dsp, name.c_str(), False);
  atoms_.insert(std::make_pair("_XSETTINGS_SCREEN", atom));

  if (atom == None) {
    util::log::Error() << "tint3: XInternAtom(\"" << name << "\") failed\n";
  }

  name.assign(StringBuilder() << "_NET_SYSTEM_TRAY_S" << DefaultScreen(dsp));
  atom = XInternAtom(dsp, name.c_str(), False);
  atoms_.insert(std::make_pair("_NET_SYSTEM_TRAY_SCREEN", atom));

  if (atom == None) {
    util::log::Error() << "tint3: XInternAtom(\"" << name << "\") failed\n";
  }
}

void Server::Cleanup() {
  if (colormap) {
    XFreeColormap(dsp, colormap);
  }

  if (colormap32) {
    XFreeColormap(dsp, colormap32);
  }

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

  XSendEvent(server.dsp, server.root_win, False,
             SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void GetRootPixmap() {
  Pixmap ret = None;
  Atom pixmap_atoms[] = {server.atoms_["_XROOTPMAP_ID"],
                         server.atoms_["_XROOTMAP_ID"]};

  for (Atom const& atom : pixmap_atoms) {
    auto res = ServerGetProperty<Pixmap>(server.root_win, atom, XA_PIXMAP, 0);

    if (res != nullptr) {
      ret = (*res);
      break;
    }
  }

  server.root_pmap = ret;

  if (server.root_pmap == None) {
    util::log::Error() << "tint3: pixmap background detection failed\n";
    return;
  }

  XGCValues gcv;
  gcv.ts_x_origin = 0;
  gcv.ts_y_origin = 0;
  gcv.fill_style = FillTiled;
  uint mask = GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle | GCTile;

  gcv.tile = server.root_pmap;
  XChangeGC(server.dsp, server.gc, mask, &gcv);
}

bool MonitorIncludes(Monitor const& m1, Monitor const& m2) {
  bool inside_x = (m1.x >= m2.x) && ((m1.x + m1.width) <= (m2.x + m2.width));
  bool inside_y = (m1.y >= m2.y) && ((m1.y + m1.height) <= (m2.y + m2.height));
  return (!inside_x || !inside_y);
}

void GetMonitors() {
  int nbmonitor = 0;

  if (XineramaIsActive(server.dsp)) {
    util::x11::ClientData<XineramaScreenInfo> info(
        XineramaQueryScreens(server.dsp, &nbmonitor));
    XRRScreenResources* res =
        XRRGetScreenResourcesCurrent(server.dsp, server.root_win);

    if (res != nullptr && res->ncrtc >= nbmonitor) {
      // use xrandr to identify monitors (does not work with proprietery nvidia
      // drivers)
      util::log::Debug() << "XRandR: found CRTCs: " << res->ncrtc << '\n';
      server.monitor.resize(res->ncrtc);

      for (int i = 0; i < res->ncrtc; ++i) {
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(server.dsp, res, res->crtcs[i]);
        server.monitor[i].x = crtc_info->x;
        server.monitor[i].y = crtc_info->y;
        server.monitor[i].width = crtc_info->width;
        server.monitor[i].height = crtc_info->height;

        for (int j = 0; j < crtc_info->noutput; ++j) {
          XRROutputInfo* output_info =
              XRRGetOutputInfo(server.dsp, res, crtc_info->outputs[j]);
          util::log::Debug() << "XRandR: linking output " << output_info->name
                             << " with CRTC " << i << '\n';
          server.monitor[i].names.push_back(output_info->name);
          XRRFreeOutputInfo(output_info);
        }

        XRRFreeCrtcInfo(crtc_info);
      }

      nbmonitor = res->ncrtc;
    } else if (info != nullptr && nbmonitor > 0) {
      server.monitor.resize(nbmonitor);

      for (int i = 0; i < nbmonitor; ++i) {
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
    for (i = 0; i < nbmonitor; ++i) {
      for (int j = 0; j < i; ++j) {
        if (!MonitorIncludes(server.monitor[i], server.monitor[j])) {
          goto next;
        }
      }
    }

  next:

    for (int j = i; j < nbmonitor; ++j) {
      server.monitor[j].names.clear();
    }

    server.nb_monitor = i;
    server.monitor.resize(server.nb_monitor);
    std::sort(server.monitor.begin(), server.monitor.end(), MonitorIncludes);

    if (res) {
      XRRFreeScreenResources(res);
    }
  }

  if (!server.nb_monitor) {
    server.nb_monitor = 1;
    server.monitor.resize(server.nb_monitor);
    server.monitor[0].x = server.monitor[0].y = 0;
    server.monitor[0].width = DisplayWidth(server.dsp, server.screen);
    server.monitor[0].height = DisplayHeight(server.dsp, server.screen);
    server.monitor[0].names.clear();
  }
}

void GetDesktops() {
  // detect number of desktops
  // wait 15s to leave some time for window manager startup
  for (int i = 0; i < 15; i++) {
    server.nb_desktop = server.GetNumberOfDesktops();

    if (server.nb_desktop > 0) {
      break;
    }

    sleep(1);
  }

  if (server.nb_desktop == 0) {
    server.nb_desktop = 1;
    util::log::Error() << "WM doesn't respect NETWM specs. "
                       << "tint3 will default to 1 desktop.\n";
  }
}

int Server::GetCurrentDesktop() {
  return GetProperty32<int>(root_win, atoms_["_NET_CURRENT_DESKTOP"],
                            XA_CARDINAL);
}

int Server::GetNumberOfDesktops() {
  return GetProperty32<int>(root_win, atoms_["_NET_NUMBER_OF_DESKTOPS"],
                            XA_CARDINAL);
}

void Server::InitGC(Window win) {
  if (!gc) {
    XGCValues gcv;
    gc = XCreateGC(dsp, win, 0, &gcv);
  }
}

void Server::InitVisual(bool snapshot_mode) {
  // inspired by freedesktops fdclock ;)
  XVisualInfo templ;
  templ.screen = screen;
  templ.depth = 32;
  templ.c_class = TrueColor;

  int nvi;
  util::x11::ClientData<XVisualInfo> xvi(XGetVisualInfo(
      dsp, VisualScreenMask | VisualDepthMask | VisualClassMask, &templ, &nvi));

  Visual* xvi_visual = nullptr;

  if (xvi != nullptr) {
    for (int i = 0; i < nvi; i++) {
      auto format = XRenderFindVisualFormat(dsp, xvi.get()[i].visual);

      if (format->type == PictTypeDirect && format->direct.alphaMask) {
        xvi_visual = xvi.get()[i].visual;
        break;
      }
    }
  }

  // check composite manager
  composite_manager = XGetSelectionOwner(dsp, atoms_["_NET_WM_CM_S0"]);

  if (colormap) {
    XFreeColormap(dsp, colormap);
  }

  if (colormap32) {
    XFreeColormap(dsp, colormap32);
  }

  if (xvi_visual) {
    visual32 = xvi_visual;
    colormap32 = XCreateColormap(dsp, root_win, xvi_visual, AllocNone);
  }

  if (xvi_visual && composite_manager != None && !snapshot_mode) {
    XSetWindowAttributes attrs;
    attrs.event_mask = StructureNotifyMask;
    XChangeWindowAttributes(dsp, composite_manager, CWEventMask, &attrs);

    real_transparency = true;
    depth = 32;
    std::cout << "Real transparency: on, depth: " << depth << '\n';
    colormap = XCreateColormap(dsp, root_win, xvi_visual, AllocNone);
    visual = xvi_visual;
  } else {
    // no composite manager or snapshot mode => fake transparency
    real_transparency = false;
    depth = DefaultDepth(dsp, screen);
    std::cout << "Real transparency: off, depth: " << depth << '\n';
    colormap = DefaultColormap(dsp, screen);
    visual = DefaultVisual(dsp, screen);
  }
}
