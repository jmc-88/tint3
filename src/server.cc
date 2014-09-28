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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xrandr.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "server.h"
#include "config.h"
#include "util/common.h"
#include "util/log.h"
#include "util/window.h"

namespace {

static std::vector<const char*> kAtomList = {
    /* X11 */
    "_XEMBED", "_XEMBED_INFO", "_XROOTMAP_ID", "_XROOTPMAP_ID",
    "_XSETTINGS_SETTINGS",

    /* NETWM */
    "_NET_ACTIVE_WINDOW", "_NET_CLIENT_LIST", "_NET_CLOSE_WINDOW",
    "_NET_CURRENT_DESKTOP", "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_NAMES",
    "_NET_DESKTOP_VIEWPORT", "_NET_NUMBER_OF_DESKTOPS",
    "_NET_SUPPORTING_WM_CHECK", "_NET_SYSTEM_TRAY_MESSAGE_DATA",
    "_NET_SYSTEM_TRAY_OPCODE", "_NET_SYSTEM_TRAY_ORIENTATION", "_NET_WM_CM_S0",
    "_NET_WM_DESKTOP", "_NET_WM_ICON", "_NET_WM_ICON_GEOMETRY", "_NET_WM_NAME",
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
    "XdndFinished", "XdndLeave", "XdndPosition", "XdndSelection",
    "XdndStatus", "XdndTypeList",

    /* Miscellaneous */
    "MANAGER", "UTF8_STRING", "_MOTIF_WM_HINTS", "__SWM_VROOT"
};

Atom GetAtom(Display* display, const char* atom_name) {
    return XInternAtom(display, atom_name, False);
}

}  // namespace

Server server;

int ServerCatchError(Display* d, XErrorEvent* ev) {
    return 0;
}

void Server::InitAtoms() {
    for (auto& atom_name : kAtomList) {
        atoms_.insert(std::make_pair(atom_name, GetAtom(dsp, atom_name)));
    }

    std::string name;

    name.assign(StringBuilder() << "_XSETTINGS_S" << DefaultScreen(dsp));
    atoms_.insert(std::make_pair(
                      "_XSETTINGS_SCREEN", GetAtom(dsp, name.c_str())));

    name.assign(StringBuilder() << "_NET_SYSTEM_TRAY_S" << DefaultScreen(dsp));
    atoms_.insert(std::make_pair(
                      "_NET_SYSTEM_TRAY_SCREEN", GetAtom(dsp, name.c_str())));
}


void Server::Cleanup() {
    if (colormap) {
        XFreeColormap(dsp, colormap);
    }

    if (colormap32) {
        XFreeColormap(dsp, colormap32);
    }

    if (monitor.size() != 0) {
        for (int i = 0; i < nb_monitor; ++i) {
            if (monitor[i].names) {
                g_strfreev(monitor[i].names);
            }
        }

        // TODO: remove this when I'm done getting rid of global state...
        monitor.clear();
    }

    if (gc) {
        XFreeGC(dsp, gc);
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


int GetProperty32(Window win, Atom at, Atom type) {
    if (!win) {
        return 0;
    }

    Atom type_ret;
    int format_ret = 0, data = 0;
    unsigned long nitems_ret = 0;
    unsigned long bafter_ret = 0;
    unsigned char* prop_value = 0;

    int result = XGetWindowProperty(server.dsp, win, at, 0, 0x7fffffff, False, type,
                                    &type_ret, &format_ret, &nitems_ret, &bafter_ret, &prop_value);

    if (result == Success && prop_value) {
        data = ((gulong*)prop_value)[0];
        XFree(prop_value);
    }

    return data;
}


void* ServerGetProperty(Window win, Atom at, Atom type, int* num_results) {
    Atom type_ret;
    int format_ret = 0;
    unsigned long nitems_ret = 0;
    unsigned long bafter_ret = 0;
    unsigned char* prop_value;
    int result;

    if (!win) {
        return 0;
    }

    result = XGetWindowProperty(server.dsp, win, at, 0, 0x7fffffff, False, type,
                                &type_ret, &format_ret, &nitems_ret, &bafter_ret, &prop_value);

    // Send back resultcount
    if (num_results) {
        *num_results = (int)nitems_ret;
    }

    if (result == Success && prop_value) {
        return prop_value;
    } else {
        return 0;
    }
}


void GetRootPixmap() {
    Pixmap ret = None;
    Atom pixmap_atoms[] = {
        server.atoms_["_XROOTPMAP_ID"],
        server.atoms_["_XROOTMAP_ID"]
    };

    for (size_t i = 0; i < sizeof(pixmap_atoms) / sizeof(Atom); ++i) {
        void* res = ServerGetProperty(server.root_win, pixmap_atoms[i], XA_PIXMAP, 0);

        if (res) {
            ret = *(static_cast<Pixmap*>(res));
            XFree(res);
            break;
        }
    }

    server.root_pmap = ret;

    if (server.root_pmap == None) {
        util::log::Error() << "tint3: pixmap background detection failed\n";
        return;
    }

    XGCValues  gcv;
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
    int i, j, nbmonitor;

    if (XineramaIsActive(server.dsp)) {
        XineramaScreenInfo* info = XineramaQueryScreens(server.dsp, &nbmonitor);
        XRRScreenResources* res = XRRGetScreenResourcesCurrent(server.dsp,
                                  server.root_win);

        if (res && res->ncrtc >= nbmonitor) {
            // use xrandr to identify monitors (does not work with proprietery nvidia drivers)
            printf("xRandr: Found crtc's: %d\n", res->ncrtc);
            server.monitor.resize(res->ncrtc);

            for (i = 0; i < res->ncrtc; ++i) {
                XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(server.dsp, res, res->crtcs[i]);
                server.monitor[i].x = crtc_info->x;
                server.monitor[i].y = crtc_info->y;
                server.monitor[i].width = crtc_info->width;
                server.monitor[i].height = crtc_info->height;
                server.monitor[i].names = (char**) calloc(crtc_info->noutput + 1,
                                          sizeof(char*));

                for (j = 0; j < crtc_info->noutput; ++j) {
                    XRROutputInfo* output_info = XRRGetOutputInfo(server.dsp, res,
                                                 crtc_info->outputs[j]);
                    printf("xRandr: Linking output %s with crtc %d\n", output_info->name, i);
                    server.monitor[i].names[j] = g_strdup(output_info->name);
                    XRRFreeOutputInfo(output_info);
                }

                server.monitor[i].names[j] = nullptr;
                XRRFreeCrtcInfo(crtc_info);
            }

            nbmonitor = res->ncrtc;
        } else if (info && nbmonitor > 0) {
            server.monitor.resize(nbmonitor);

            for (i = 0 ; i < nbmonitor; ++i) {
                server.monitor[i].x = info[i].x_org;
                server.monitor[i].y = info[i].y_org;
                server.monitor[i].width = info[i].width;
                server.monitor[i].height = info[i].height;
                server.monitor[i].names = nullptr;
            }
        }

        // order monitors
        std::sort(server.monitor.begin(), server.monitor.end(), MonitorIncludes);

        // remove monitor included into another one
        for (i = 0; i < nbmonitor; ++i) {
            for (j = 0; j < i; ++j) {
                if (!MonitorIncludes(server.monitor[i], server.monitor[j])) {
                    goto next;
                }
            }
        }

next:

        for (j = i; j < nbmonitor; ++j) {
            if (server.monitor[j].names) {
                g_strfreev(server.monitor[j].names);
            }
        }

        server.nb_monitor = i;
        server.monitor.resize(server.nb_monitor);
        std::sort(server.monitor.begin(), server.monitor.end(), MonitorIncludes);

        if (res) {
            XRRFreeScreenResources(res);
        }

        XFree(info);
    }

    if (!server.nb_monitor) {
        server.nb_monitor = 1;
        server.monitor.resize(server.nb_monitor);
        server.monitor[0].x = server.monitor[0].y = 0;
        server.monitor[0].width = DisplayWidth(server.dsp, server.screen);
        server.monitor[0].height = DisplayHeight(server.dsp, server.screen);
        server.monitor[0].names = 0;
    }
}


void GetDesktops() {
    // detect number of desktops
    // wait 15s to leave some time for window manager startup
    for (int i = 0 ; i < 15 ; i++) {
        server.nb_desktop = server.GetDesktop();

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
    return GetProperty32(root_win, atoms_["_NET_CURRENT_DESKTOP"], XA_CARDINAL);
}

int Server::GetDesktop() {
    return GetDesktopFromWindow(root_win);
}

int Server::GetDesktopFromWindow(Window win) {
    return GetProperty32(win, atoms_["_NET_NUMBER_OF_DESKTOPS"], XA_CARDINAL);
}

void Server::InitVisual() {
    // inspired by freedesktops fdclock ;)
    XVisualInfo templ;
    templ.screen = screen;
    templ.depth = 32;
    templ.c_class = TrueColor;

    int nvi;
    auto xvi = XGetVisualInfo(dsp,
                              VisualScreenMask | VisualDepthMask | VisualClassMask,
                              &templ,
                              &nvi);

    Visual* xvi_visual = nullptr;

    if (xvi) {
        for (int i = 0; i < nvi; i++) {
            auto format = XRenderFindVisualFormat(dsp, xvi[i].visual);

            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                xvi_visual = xvi[i].visual;
                break;
            }
        }
    }

    XFree(xvi);

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

    if (xvi_visual && composite_manager != None && snapshot_path.empty()) {
        XSetWindowAttributes attrs;
        attrs.event_mask = StructureNotifyMask;
        XChangeWindowAttributes(dsp, composite_manager, CWEventMask, &attrs);

        real_transparency = 1;
        depth = 32;
        printf("real transparency on... depth: %d\n", depth);
        colormap = XCreateColormap(dsp, root_win, xvi_visual, AllocNone);
        visual = xvi_visual;
    } else {
        // no composite manager or snapshot mode => fake transparency
        real_transparency = 0;
        depth = DefaultDepth(dsp, screen);
        printf("real transparency off.... depth: %d\n", depth);
        colormap = DefaultColormap(dsp, screen);
        visual = DefaultVisual(dsp, screen);
    }
}
