/**************************************************************************
* Tint3 : systraybar
*
* Copyright (C) 2009 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
* based on 'docker-1.5' from Ben Jansens.
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
#include <glib.h>
#include <Imlib2.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>

#include "systraybar.h"
#include "server.h"
#include "panel.h"
#include "util/log.h"

GSList* icons;

/* defined in the systray spec */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

// selection window
Window net_sel_win = None;

// freedesktop specification doesn't allow multi systray
Systraybar systray;
int refresh_systray;
int systray_enabled;
int systray_max_icon_size;

// background pixmap if we render ourselves the icons
static Pixmap render_background;


void DefaultSystray() {
    render_background = 0;
    systray.alpha = 100;
    systray.sort = 3;
    systray.size_mode = SIZE_BY_CONTENT;
}

void CleanupSystray() {
    StopNet();
    systray_enabled = 0;
    systray_max_icon_size = 0;
    systray.on_screen = 0;
    systray.FreeArea();

    if (render_background) {
        XFreePixmap(server.dsp, render_background);
        render_background = 0;
    }
}

void InitSystray() {
    StartNet();

    if (!systray_enabled) {
        return;
    }

    if (!server.visual32 && (systray.alpha != 100 || systray.brightness != 0
                             || systray.saturation != 0)) {
        printf("No 32 bit visual for your X implementation. 'systray_asb = 100 0 0' will be forced\n");
        systray.alpha = 100;
        systray.brightness = systray.saturation = 0;
    }
}


void InitSystrayPanel(void* p) {
    systray.parent = static_cast<Area*>(p);
    systray.panel = static_cast<Panel*>(p);

    if (systray.bg == 0) {
        systray.bg = backgrounds.front();
    }

    if (systray.VisibleIcons() == 0) {
        systray.Hide();
    } else {
        systray.Show();
    }

    refresh_systray = 0;
}


void Systraybar::DrawForeground(cairo_t* /* c */) {
    if (server.real_transparency || systray.alpha != 100 || systray.brightness != 0
        || systray.saturation != 0) {
        if (render_background) {
            XFreePixmap(server.dsp, render_background);
        }

        render_background = XCreatePixmap(server.dsp, server.root_win,
                                          systray.width, systray.height, server.depth);
        XCopyArea(server.dsp, systray.pix, render_background, server.gc, 0, 0,
                  systray.width, systray.height, 0, 0);
    }

    refresh_systray = 1;
}


bool Systraybar::Resize() {
    if (panel_horizontal) {
        icon_size = height;
    } else {
        icon_size = width;
    }

    icon_size = icon_size - (2 * bg->border.width) - (2 * paddingy);

    if (systray_max_icon_size > 0 && icon_size > systray_max_icon_size) {
        icon_size = systray_max_icon_size;
    }

    size_t count = systray.VisibleIcons();

    if (panel_horizontal) {
        int height_ = height - 2 * bg->border.width - 2 * paddingy;
        // here icons_per_column always higher than 0
        icons_per_column = (height_ + paddingx) / (icon_size + paddingx);
        marging = height_ - (icons_per_column - 1) * (icon_size + paddingx) - icon_size;
        icons_per_row = count / icons_per_column + (count % icons_per_column != 0);
        systray.width = (2 * systray.bg->border.width) + (2 * systray.paddingxlr) +
                        (icon_size * icons_per_row) + ((icons_per_row - 1) * systray.paddingx);
    } else {
        int width_ = width - 2 * bg->border.width - 2 * paddingy;
        // here icons_per_row always higher than 0
        icons_per_row = (width_ + paddingx) / (icon_size + paddingx);
        marging = width_ - (icons_per_row - 1) * (icon_size + paddingx) - icon_size;
        icons_per_column = count / icons_per_row + (count % icons_per_row != 0);
        systray.height = (2 * systray.bg->border.width) + (2 * systray.paddingxlr) +
                         (icon_size * icons_per_column) + ((icons_per_column - 1) * systray.paddingx);
    }

    return true;
}


void Systraybar::OnChangeLayout() {
    // here, systray.posx/posy are defined by rendering engine. so we can calculate position of tray icon.
    int posx, posy;
    int start = panel->bg->border.width + panel->paddingy +
                systray.bg->border.width + systray.paddingy + marging / 2;

    if (panel_horizontal) {
        posy = start;
        posx = systray.posx + systray.bg->border.width +
               systray.paddingxlr;
    } else {
        posx = start;
        posy = systray.posy + systray.bg->border.width +
               systray.paddingxlr;
    }

    int i = 0;

    for (auto& traywin : systray.list_icons) {
        ++i;

        if (traywin->hide) {
            continue;
        }

        traywin->y = posy;
        traywin->x = posx;
        //printf("systray %d : %d,%d\n", i, posx, posy);
        traywin->width = icon_size;
        traywin->height = icon_size;

        if (panel_horizontal) {
            if (i % icons_per_column) {
                posy += (icon_size + paddingx);
            } else {
                posy = start;
                posx += (icon_size + systray.paddingx);
            }
        } else {
            if (i % icons_per_row) {
                posx += (icon_size + systray.paddingx);
            } else {
                posx = start;
                posy += (icon_size + systray.paddingx);
            }
        }

        // position and size the icon window
        XMoveResizeWindow(server.dsp, traywin->id, traywin->x, traywin->y,
                          icon_size, icon_size);
        XResizeWindow(server.dsp, traywin->tray_id, icon_size,
                      icon_size);
    }
}


size_t Systraybar::VisibleIcons() {
    size_t count = 0;

    for (auto& traywin : list_icons) {
        if (!traywin->hide) {
            ++count;
        }
    }

    return count;
}


// ***********************************************
// systray protocol

void StartNet() {
    if (net_sel_win) {
        // protocol already started
        if (!systray_enabled) {
            StopNet();
        }

        return;
    } else if (!systray_enabled) {
        return;
    }

    Window win = XGetSelectionOwner(server.dsp,
                                    server.atoms_["_NET_SYSTEM_TRAY_SCREEN"]);

    // freedesktop systray specification
    if (win != None) {
        // search pid
        Atom _NET_WM_PID, actual_type;
        int actual_format;
        unsigned long nitems;
        unsigned long bytes_after;
        unsigned char* prop = 0;
        int pid;

        _NET_WM_PID = XInternAtom(server.dsp, "_NET_WM_PID", True);
        int ret = XGetWindowProperty(server.dsp, win, _NET_WM_PID, 0, 1024, False,
                                     AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop);

        util::log::Error() << "tint3: another systray is running";

        if (ret == Success && prop) {
            pid = prop[1] * 256;
            pid += prop[0];
            util::log::Error() << " (pid " << pid << ')';
        }

        util::log::Error() << '\n';
        return;
    }

    // init systray protocol
    net_sel_win = XCreateSimpleWindow(server.dsp, server.root_win, -1, -1, 1, 1, 0,
                                      0, 0);

    // v0.3 trayer specification. tint3 always horizontal.
    // Vertical panel will draw the systray horizontal.
    long orient = 0;
    XChangeProperty(server.dsp, net_sel_win,
                    server.atoms_["_NET_SYSTEM_TRAY_ORIENTATION"], XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*) &orient, 1);
    VisualID vid;

    if (server.visual32 && (systray.alpha != 100 || systray.brightness != 0
                            || systray.saturation != 0)) {
        vid = XVisualIDFromVisual(server.visual32);
    } else {
        vid = XVisualIDFromVisual(server.visual);
    }

    XChangeProperty(server.dsp, net_sel_win, XInternAtom(server.dsp,
                    "_NET_SYSTEM_TRAY_VISUAL", False), XA_VISUALID, 32, PropModeReplace,
                    (unsigned char*)&vid, 1);

    XSetSelectionOwner(server.dsp, server.atoms_["_NET_SYSTEM_TRAY_SCREEN"],
                       net_sel_win,
                       CurrentTime);

    if (XGetSelectionOwner(server.dsp,
                           server.atoms_["_NET_SYSTEM_TRAY_SCREEN"]) != net_sel_win) {
        StopNet();
        util::log::Error() << "tint3: can't get systray manager\n";
        return;
    }

    XClientMessageEvent ev;
    ev.type = ClientMessage;
    ev.window = server.root_win;
    ev.message_type = server.atoms_["MANAGER"];
    ev.format = 32;
    ev.data.l[0] = CurrentTime;
    ev.data.l[1] = server.atoms_["_NET_SYSTEM_TRAY_SCREEN"];
    ev.data.l[2] = net_sel_win;
    ev.data.l[3] = 0;
    ev.data.l[4] = 0;
    XSendEvent(server.dsp, server.root_win, False, StructureNotifyMask,
               (XEvent*)&ev);
}


void StopNet() {
    // remove_icon change systray.list_icons
    while (!systray.list_icons.empty()) {
        systray.RemoveIcon(systray.list_icons.back());
    }

    if (net_sel_win != None) {
        XDestroyWindow(server.dsp, net_sel_win);
        net_sel_win = None;
    }
}


// TODO: this error handling is horrible - let's move to XCB as soon as
// possible..
bool error = false;

int WindowErrorHandler(Display* d, XErrorEvent* e) {
    error = true;

    // TODO: fix this - how large should the buffer be?
    char error_text[512];

    if (XGetErrorText(d, e->error_code, error_text, 512) == 0) {
        util::log::Error() << "WindowErrorHandler: " << error_text << '\n';
    }

    return 0;
}


bool CompareTrayWindows(gconstpointer a, gconstpointer b) {
    const TrayWindow* traywin_a = (TrayWindow*)a;
    const TrayWindow* traywin_b = (TrayWindow*)b;
    XTextProperty name_a, name_b;

    if (XGetWMName(server.dsp, traywin_a->tray_id, &name_a) == 0) {
        return true;
    }

    if (XGetWMName(server.dsp, traywin_b->tray_id, &name_b) == 0) {
        XFree(name_a.value);
        return false;
    }

    gint retval = g_ascii_strncasecmp((char*)name_a.value, (char*)name_b.value,
                                      -1) * systray.sort;
    XFree(name_a.value);
    XFree(name_b.value);
    return (retval <= 0);
}


bool Systraybar::AddIcon(Window id) {
    int hide = 0;

    error = false;
    XWindowAttributes attr;

    if (XGetWindowAttributes(server.dsp, id, &attr) == False) {
        return false;
    }

    unsigned long mask = 0;
    XSetWindowAttributes set_attr;
    Visual* visual = server.visual;

    //printf("icon with depth: %d, width %d, height %d\n", attr.depth, attr.width, attr.height);
    //printf("icon with depth: %d\n", attr.depth);
    if (attr.depth != server.depth || alpha != 100 || brightness != 0
        || saturation != 0) {
        visual = attr.visual;
        set_attr.colormap = attr.colormap;
        set_attr.background_pixel = 0;
        set_attr.border_pixel = 0;
        mask = CWColormap | CWBackPixel | CWBorderPixel;
    } else {
        set_attr.background_pixmap = ParentRelative;
        mask = CWBackPixmap;
    }

    Window parent_window = XCreateWindow(server.dsp, panel->main_win,
                                         0, 0, 30, 30, 0,
                                         attr.depth, InputOutput, visual, mask, &set_attr);
    auto old_handler = XSetErrorHandler(WindowErrorHandler);
    XReparentWindow(server.dsp, id, parent_window, 0, 0);
    // watch for the icon trying to resize itself / closing again!
    XSelectInput(server.dsp, id, StructureNotifyMask);
    XSync(server.dsp, False);
    XSetErrorHandler(old_handler);

    if (error) {
        util::log::Error() << "tint3: not icon_swallow\n";
        XDestroyWindow(server.dsp, parent_window);
        return false;
    }

    {
        Atom acttype;
        int actfmt;
        unsigned long nbitem, bytes;
        unsigned char* data = 0;

        int ret = XGetWindowProperty(server.dsp, id, server.atoms_["_XEMBED_INFO"], 0,
                                     2,
                                     False, server.atoms_["_XEMBED_INFO"], &acttype, &actfmt,
                                     &nbitem, &bytes, &data);

        if (ret == Success) {
            if (data) {
                if (nbitem == 2) {
                    //hide = ((data[1] & XEMBED_MAPPED) == 0);
                    //printf("hide %d\n", hide);
                }

                XFree(data);
            }
        } else {
            util::log::Error() << "tint3: xembed error\n";
            XDestroyWindow(server.dsp, parent_window);
            return false;
        }
    }

    {
        XEvent e;
        e.xclient.type = ClientMessage;
        e.xclient.serial = 0;
        e.xclient.send_event = True;
        e.xclient.message_type = server.atoms_["_XEMBED"];
        e.xclient.window = id;
        e.xclient.format = 32;
        e.xclient.data.l[0] = CurrentTime;
        e.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
        e.xclient.data.l[2] = 0;
        e.xclient.data.l[3] = parent_window;
        e.xclient.data.l[4] = 0;
        XSendEvent(server.dsp, id, False, 0xFFFFFF, &e);
    }

    auto traywin = new TrayWindow();
    traywin->id = parent_window;
    traywin->tray_id = id;
    traywin->hide = hide;
    traywin->depth = attr.depth;
    traywin->damage = 0;

    if (on_screen == 0) {
        Show();
    }

    if (sort == 3) {
        list_icons.push_front(traywin);
    } else if (sort == 2) {
        list_icons.push_back(traywin);
    } else {
        auto it = std::lower_bound(list_icons.begin(), list_icons.end(),
                                   traywin, CompareTrayWindows);
        list_icons.insert(it, traywin);
    }

    if (server.real_transparency || alpha != 100 || brightness != 0
        || saturation != 0) {
        traywin->damage = XDamageCreate(server.dsp, traywin->id,
                                        XDamageReportRawRectangles);
        XCompositeRedirectWindow(server.dsp, traywin->id, CompositeRedirectManual);
    }

    // show the window
    if (!traywin->hide) {
        XMapWindow(server.dsp, traywin->tray_id);
    }

    if (!traywin->hide && !panel->is_hidden) {
        XMapRaised(server.dsp, traywin->id);
    }

    // changed in systray
    need_resize = true;
    panel_refresh = 1;
    return true;
}


void Systraybar::RemoveIcon(TrayWindow* traywin) {
    // remove from our list
    list_icons.erase(std::remove(list_icons.begin(), list_icons.end(), traywin),
                     list_icons.end());
    //printf("remove_icon id %lx, %d\n", traywin->id);

    XSelectInput(server.dsp, traywin->tray_id, NoEventMask);

    if (traywin->damage) {
        XDamageDestroy(server.dsp, traywin->damage);
    }

    // reparent to root
    error = false;
    auto old = XSetErrorHandler(WindowErrorHandler);

    if (!traywin->hide) {
        XUnmapWindow(server.dsp, traywin->tray_id);
    }

    XReparentWindow(server.dsp, traywin->tray_id, server.root_win, 0, 0);
    XDestroyWindow(server.dsp, traywin->id);
    XSync(server.dsp, False);
    XSetErrorHandler(old);

    if (traywin->render_timeout) {
        StopTimeout(traywin->render_timeout);
    }

    delete traywin;

    // check empty systray
    if (VisibleIcons() == 0) {
        Hide();
    }

    // changed in systray
    need_resize = true;
    panel_refresh = 1;
}


void NetMessage(XClientMessageEvent* e) {
    unsigned long opcode;
    Window id;

    opcode = e->data.l[1];

    switch (opcode) {
        case SYSTEM_TRAY_REQUEST_DOCK:
            id = e->data.l[2];

            if (id) {
                systray.AddIcon(id);
            }

            break;

        case SYSTEM_TRAY_BEGIN_MESSAGE:
        case SYSTEM_TRAY_CANCEL_MESSAGE:
            // we don't show baloons messages.
            break;

        default:
            if (opcode == server.atoms_["_NET_SYSTEM_TRAY_MESSAGE_DATA"]) {
                util::log::Debug() << "message from dockapp: " << e->data.b << '\n';
            } else {
                util::log::Error() << "SYSTEM_TRAY: unknown message type\n";
            }

            break;
    }
}

void systray_render_icon_now(void* t) {
    // we end up in this function only in real transparency mode or if systray_task_asb != 100 0 0
    // we made also sure, that we always have a 32 bit visual, i.e. we can safely create 32 bit pixmaps here
    TrayWindow* traywin = static_cast<TrayWindow*>(t);
    traywin->render_timeout = 0;

    if (traywin->width == 0 || traywin->height == 0) {
        // reschedule rendering since the geometry information has not yet been processed (can happen on slow cpu)
        SystrayRenderIcon(traywin);
        return;
    }

    // good systray icons support 32 bit depth, but some icons are still 24 bit.
    // We create a heuristic mask for these icons, i.e. we get the rgb value in the top left corner, and
    // mask out all pixel with the same rgb value
    Panel* panel = systray.panel;

    // Very ugly hack, but somehow imlib2 is not able to get the image from the traywindow itself,
    // so we first render the tray window onto a pixmap, and then we tell imlib2 to use this pixmap as
    // drawable. If someone knows why it does not work with the traywindow itself, please tell me ;)
    Pixmap tmp_pmap = XCreatePixmap(server.dsp, server.root_win, traywin->width,
                                    traywin->height, 32);
    XRenderPictFormat* f;

    if (traywin->depth == 24) {
        f = XRenderFindStandardFormat(server.dsp, PictStandardRGB24);
    } else if (traywin->depth == 32) {
        f = XRenderFindStandardFormat(server.dsp, PictStandardARGB32);
    } else {
        printf("Strange tray icon found with depth: %d\n", traywin->depth);
        return;
    }

    Picture pict_image;
    //if (server.real_transparency)
    //pict_image = XRenderCreatePicture(server.dsp, traywin->id, f, 0, 0);
    // reverted Rev 407 because here it's breaking alls icon with systray + xcompmgr
    pict_image = XRenderCreatePicture(server.dsp, traywin->tray_id, f, 0, 0);
    Picture pict_drawable = XRenderCreatePicture(server.dsp, tmp_pmap,
                            XRenderFindVisualFormat(server.dsp, server.visual32), 0, 0);
    XRenderComposite(server.dsp, PictOpSrc, pict_image, None, pict_drawable, 0, 0,
                     0, 0, 0, 0, traywin->width, traywin->height);
    XRenderFreePicture(server.dsp, pict_image);
    XRenderFreePicture(server.dsp, pict_drawable);
    // end of the ugly hack and we can continue as before

    imlib_context_set_visual(server.visual32);
    imlib_context_set_colormap(server.colormap32);
    imlib_context_set_drawable(tmp_pmap);
    Imlib_Image image = imlib_create_image_from_drawable(0, 0, 0, traywin->width,
                        traywin->height, 1);

    if (image == 0) {
        return;
    }

    imlib_context_set_image(image);
    //if (traywin->depth == 24)
    //imlib_save_image("/home/thil77/test.jpg");
    imlib_image_set_has_alpha(1);
    DATA32* data = imlib_image_get_data();

    if (traywin->depth == 24) {
        CreateHeuristicMask(data, traywin->width, traywin->height);
    }

    if (systray.alpha != 100 || systray.brightness != 0
        || systray.saturation != 0) {
        AdjustAsb(data, traywin->width, traywin->height, systray.alpha,
                  (float)systray.saturation / 100, (float)systray.brightness / 100);
    }

    imlib_image_put_back_data(data);
    XCopyArea(server.dsp, render_background, systray.pix, server.gc,
              traywin->x - systray.posx, traywin->y - systray.posy, traywin->width,
              traywin->height, traywin->x - systray.posx,
              traywin->y - systray.posy);
    RenderImage(systray.pix, traywin->x - systray.posx,
                traywin->y - systray.posy, traywin->width, traywin->height);
    XCopyArea(server.dsp, systray.pix, panel->main_win, server.gc,
              traywin->x - systray.posx, traywin->y - systray.posy, traywin->width,
              traywin->height, traywin->x, traywin->y);
    imlib_free_image_and_decache();
    XFreePixmap(server.dsp, tmp_pmap);
    imlib_context_set_visual(server.visual);
    imlib_context_set_colormap(server.colormap);

    if (traywin->damage) {
        XDamageSubtract(server.dsp, traywin->damage, None, None);
    }

    XFlush(server.dsp);
}


void SystrayRenderIcon(TrayWindow* traywin) {
    if (server.real_transparency || systray.alpha != 100 || systray.brightness != 0
        || systray.saturation != 0) {
        // wine tray icons update whenever mouse is over them, so we limit the updates to 50 ms
        if (traywin->render_timeout == 0) {
            traywin->render_timeout = AddTimeout(50, 0, systray_render_icon_now, traywin);
        }
    } else {
        // comment by andreas: I'm still not sure, what exactly we need to do here... Somehow trayicons which do not
        // offer the same depth as tint3 does, need to draw a background pixmap, but this cannot be done with
        // XCopyArea... So we actually need XRenderComposite???
        //          Pixmap pix = XCreatePixmap(server.dsp, server.root_win, traywin->width, traywin->height, server.depth);
        //          XCopyArea(server.dsp, panel->temp_pmap, pix, server.gc, traywin->x, traywin->y, traywin->width, traywin->height, 0, 0);
        //          XSetWindowBackgroundPixmap(server.dsp, traywin->id, pix);
        XClearArea(server.dsp, traywin->tray_id, 0, 0, traywin->width, traywin->height,
                   True);
    }
}


void RefreshSystrayIcon() {
    for (auto& traywin : systray.list_icons) {
        if (traywin->hide) {
            continue;
        }

        SystrayRenderIcon(traywin);
    }
}
