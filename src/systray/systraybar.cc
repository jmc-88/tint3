/**************************************************************************
* Tint3 : systraybar
*
* Copyright (C) 2009 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <Imlib2.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "panel.hh"
#include "server.hh"
#include "systray/systraybar.hh"
#include "systray/tray_window.hh"
#include "util/log.hh"
#include "util/x11.hh"

/* defined in the systray spec */
#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

// selection window
Window net_sel_win = None;

// freedesktop specification doesn't allow multi systray
Systraybar systray;
bool systray_enabled;
int systray_max_icon_size;

// background pixmap if we render ourselves the icons
static Pixmap render_background;

void DefaultSystray() {
  render_background = 0;
  systray.alpha = 100;
  systray.sort = 3;
  systray.size_mode_ = SizeMode::kByContent;
}

void CleanupSystray(Timer& timer) {
  systray.StopNet(timer);

  if (systray_enabled) {
    systray.Clear(timer);
  }

  systray_enabled = false;
  systray_max_icon_size = 0;
  systray.on_screen_ = false;
  systray.FreeArea();

  if (render_background) {
    XFreePixmap(server.dsp, render_background);
    render_background = 0;
  }
}

void InitSystray(Timer& timer) {
  systray.StartNet(timer);

  if (!systray_enabled) {
    return;
  }

  if (!server.visual32 && (systray.alpha != 100 || systray.brightness != 0 ||
                           systray.saturation != 0)) {
    util::log::Error() << "No 32 bit visual for your X implementation. "
                          "'systray_asb = 100 0 0' will be forced.\n";
    systray.alpha = 100;
    systray.brightness = systray.saturation = 0;
  }

  systray.list_icons.clear();
}

bool Systraybar::should_refresh() const { return should_refresh_; }

void Systraybar::set_should_refresh(bool should_refresh) {
  should_refresh_ = should_refresh;
}

void Systraybar::SetParentPanel(Panel* panel) {
  parent_ = panel;
  panel_ = panel;

  if (VisibleIcons() == 0) {
    Hide();
  } else {
    Show();
  }

  set_should_refresh(false);
}

void Systraybar::DrawForeground(cairo_t* /* c */) {
  if (server.real_transparency || systray.alpha != 100 ||
      systray.brightness != 0 || systray.saturation != 0) {
    if (render_background) {
      XFreePixmap(server.dsp, render_background);
    }

    render_background =
        XCreatePixmap(server.dsp, server.root_window(), systray.width_,
                      systray.height_, server.depth);
    XCopyArea(server.dsp, systray.pix_, render_background, server.gc, 0, 0,
              systray.width_, systray.height_, 0, 0);
  }

  should_refresh_ = true;
}

bool Systraybar::Resize() {
  if (panel_horizontal) {
    icon_size = height_;
  } else {
    icon_size = width_;
  }

  icon_size = icon_size - (2 * bg_.border().width()) - (2 * padding_y_);

  if (systray_max_icon_size > 0 && icon_size > systray_max_icon_size) {
    icon_size = systray_max_icon_size;
  }

  size_t count = systray.VisibleIcons();

  if (panel_horizontal) {
    int height = height_ - 2 * bg_.border().width() - 2 * padding_y_;
    // here icons_per_column always higher than 0
    icons_per_column = (height + padding_x_) / (icon_size + padding_x_);
    margin_ =
        height - (icons_per_column - 1) * (icon_size + padding_x_) - icon_size;
    icons_per_row = count / icons_per_column + (count % icons_per_column != 0);
    systray.width_ = (2 * systray.bg_.border().width()) +
                     (2 * systray.padding_x_lr_) + (icon_size * icons_per_row) +
                     ((icons_per_row - 1) * systray.padding_x_);
  } else {
    int width = width_ - 2 * bg_.border().width() - 2 * padding_y_;
    // here icons_per_row always higher than 0
    icons_per_row = (width + padding_x_) / (icon_size + padding_x_);
    margin_ =
        width - (icons_per_row - 1) * (icon_size + padding_x_) - icon_size;
    icons_per_column = count / icons_per_row + (count % icons_per_row != 0);
    systray.height_ = (2 * systray.bg_.border().width()) +
                      (2 * systray.padding_x_lr_) +
                      (icon_size * icons_per_column) +
                      ((icons_per_column - 1) * systray.padding_x_);
  }

  return true;
}

void Systraybar::OnChangeLayout() {
  // here, systray.posx/posy are defined by rendering engine. so we can
  // calculate position of tray icon.
  int posx, posy;
  int start = panel_->bg_.border().width() + panel_->padding_y_ +
              systray.bg_.border().width() + systray.padding_y_ + margin_ / 2;

  if (panel_horizontal) {
    posy = start;
    posx =
        systray.panel_x_ + systray.bg_.border().width() + systray.padding_x_lr_;
  } else {
    posx = start;
    posy =
        systray.panel_y_ + systray.bg_.border().width() + systray.padding_x_lr_;
  }

  int i = 0;

  for (auto& traywin : systray.list_icons) {
    ++i;

    if (traywin->hide) {
      continue;
    }

    traywin->y = posy;
    traywin->x = posx;
    traywin->width = icon_size;
    traywin->height = icon_size;

    if (panel_horizontal) {
      if (i % icons_per_column) {
        posy += (icon_size + padding_x_);
      } else {
        posy = start;
        posx += (icon_size + systray.padding_x_);
      }
    } else {
      if (i % icons_per_row) {
        posx += (icon_size + systray.padding_x_);
      } else {
        posx = start;
        posy += (icon_size + systray.padding_x_);
      }
    }

    // position and size the icon window
    XMoveResizeWindow(server.dsp, traywin->id, traywin->x, traywin->y,
                      icon_size, icon_size);
    XResizeWindow(server.dsp, traywin->tray_id, icon_size, icon_size);
  }
}

size_t Systraybar::VisibleIcons() const {
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

namespace {

Window GetSystemTrayOwner() {
  return XGetSelectionOwner(server.dsp,
                            server.atoms_["_NET_SYSTEM_TRAY_SCREEN"]);
}

}  // namespace

void Systraybar::StartNet(Timer& timer) {
  if (!systray_enabled) {
    return;
  }

  if (net_sel_win) {
    // protocol already started
    if (!systray_enabled) {
      StopNet(timer);
    }
    return;
  }

  Window window = GetSystemTrayOwner();

  // freedesktop systray specification
  if (window != None) {
    pid_t pid = util::x11::GetWindowPID(window);

    util::log::Error() << "tint3: another systray is running";
    if (pid != -1) {
      util::log::Error() << " (pid " << pid << ')';
    }
    util::log::Error() << '\n';
    return;
  }

  // init systray protocol
  net_sel_win = util::x11::CreateSimpleWindow(server.root_window(), -1, -1, 1,
                                              1, 0, 0, 0);

  // v0.3 trayer specification. tint3 always horizontal.
  // Vertical panel will draw the systray horizontal.
  unsigned char orient = 0;
  XChangeProperty(server.dsp, net_sel_win,
                  server.atoms_["_NET_SYSTEM_TRAY_ORIENTATION"], XA_CARDINAL,
                  32, PropModeReplace, &orient, 1);

  VisualID vid;

  if (server.visual32 && (systray.alpha != 100 || systray.brightness != 0 ||
                          systray.saturation != 0)) {
    vid = XVisualIDFromVisual(server.visual32);
  } else {
    vid = XVisualIDFromVisual(server.visual);
  }

  XChangeProperty(server.dsp, net_sel_win,
                  server.atoms_["_NET_SYSTEM_TRAY_VISUAL"], XA_VISUALID, 32,
                  PropModeReplace, (unsigned char*)&vid, 1);

  XSetSelectionOwner(server.dsp, server.atoms_["_NET_SYSTEM_TRAY_SCREEN"],
                     net_sel_win, CurrentTime);

  Window owner =
      XGetSelectionOwner(server.dsp, server.atoms_["_NET_SYSTEM_TRAY_SCREEN"]);

  if (owner != net_sel_win) {
    util::log::Error() << "Can't get systray manager.\n";
    StopNet(timer);
    return;
  }

  XClientMessageEvent ev;
  ev.type = ClientMessage;
  ev.window = server.root_window();
  ev.message_type = server.atoms_["MANAGER"];
  ev.format = 32;
  ev.data.l[0] = CurrentTime;
  ev.data.l[1] = server.atoms_["_NET_SYSTEM_TRAY_SCREEN"];
  ev.data.l[2] = net_sel_win;
  ev.data.l[3] = 0;
  ev.data.l[4] = 0;
  XSendEvent(server.dsp, server.root_window(), False, StructureNotifyMask,
             (XEvent*)&ev);
}

void Systraybar::StopNet(Timer& timer) {
  // remove_icon change systray.list_icons
  for (auto& icon : systray.list_icons) {
    RemoveIconInternal(icon, timer);
  }

  list_icons.clear();

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

bool CompareTrayWindows(TrayWindow const* traywin_a,
                        TrayWindow const* traywin_b) {
  std::string name_a;
  if (!util::x11::GetWMName(server.dsp, traywin_a->tray_id, &name_a)) {
    return true;
  }

  std::string name_b;
  if (!util::x11::GetWMName(server.dsp, traywin_a->tray_id, &name_b)) {
    return true;
  }

  util::string::ToLowerCase(&name_a);
  util::string::ToLowerCase(&name_b);
  return (systray.sort == /*descending=*/-1) ? (name_a > name_b)
                                             : (name_a < name_b);
}

bool Systraybar::AddIcon(Window id) {
  error = false;
  XWindowAttributes attr;

  if (XGetWindowAttributes(server.dsp, id, &attr) == False) {
    return false;
  }

  unsigned long mask = 0;
  XSetWindowAttributes set_attr;
  Visual* visual = server.visual;

  if (attr.depth != server.depth || alpha != 100 || brightness != 0 ||
      saturation != 0) {
    visual = attr.visual;
    set_attr.colormap = attr.colormap;
    set_attr.background_pixel = 0;
    set_attr.border_pixel = 0;
    mask = CWColormap | CWBackPixel | CWBorderPixel;
  } else {
    set_attr.background_pixmap = ParentRelative;
    mask = CWBackPixmap;
  }

  Window parent_window =
      util::x11::CreateWindow(panel_->main_win_, 0, 0, 30, 30, 0, attr.depth,
                              InputOutput, visual, mask, &set_attr);

  {
    error = false;
    util::x11::ScopedErrorHandler error_handler(WindowErrorHandler);
    XReparentWindow(server.dsp, id, parent_window, 0, 0);
    // watch for the icon trying to resize itself / closing again!
    XSelectInput(server.dsp, id, StructureNotifyMask);
    XSync(server.dsp, False);
  }

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

    int ret = XGetWindowProperty(server.dsp, id, server.atoms_["_XEMBED_INFO"],
                                 0, 2, False, server.atoms_["_XEMBED_INFO"],
                                 &acttype, &actfmt, &nbitem, &bytes, &data);

    if (ret == Success) {
      if (data) {
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

  auto traywin = new TrayWindow(parent_window, id);
  traywin->hide = false;
  traywin->depth = attr.depth;
  traywin->damage = 0;

  if (!on_screen_) {
    Show();
  }

  if (sort == 3) {
    list_icons.push_front(traywin);
  } else if (sort == 2) {
    list_icons.push_back(traywin);
  } else {
    auto it = std::lower_bound(list_icons.begin(), list_icons.end(), traywin,
                               CompareTrayWindows);
    list_icons.insert(it, traywin);
  }

  if (server.real_transparency || alpha != 100 || brightness != 0 ||
      saturation != 0) {
    traywin->damage =
        XDamageCreate(server.dsp, traywin->id, XDamageReportRawRectangles);
    XCompositeRedirectWindow(server.dsp, traywin->id, CompositeRedirectManual);
  }

  // show the window
  if (!traywin->hide) {
    XMapWindow(server.dsp, traywin->tray_id);
  }

  if (!traywin->hide && !panel_->hidden()) {
    XMapRaised(server.dsp, traywin->id);
  }

  // changed in systray
  need_resize_ = true;
  panel_refresh = true;
  return true;
}

void Systraybar::RemoveIconInternal(TrayWindow* traywin, Timer& timer) {
  XSelectInput(server.dsp, traywin->tray_id, NoEventMask);

  if (traywin->damage) {
    XDamageDestroy(server.dsp, traywin->damage);
  }

  // reparent to root
  {
    error = false;
    util::x11::ScopedErrorHandler error_handler(WindowErrorHandler);

    if (!traywin->hide) {
      XUnmapWindow(server.dsp, traywin->tray_id);
    }

    XReparentWindow(server.dsp, traywin->tray_id, server.root_window(), 0, 0);
    XDestroyWindow(server.dsp, traywin->id);
    XSync(server.dsp, False);
  }

  if (traywin->render_timeout) {
    timer.ClearInterval(traywin->render_timeout);
  }

  delete traywin;
}

void Systraybar::RemoveIcon(TrayWindow* traywin, Timer& timer) {
  RemoveIconInternal(traywin, timer);

  // remove from our list
  list_icons.erase(std::remove(list_icons.begin(), list_icons.end(), traywin),
                   list_icons.end());

  // check empty systray
  if (VisibleIcons() == 0) {
    Hide();
  }

  // changed in systray
  need_resize_ = true;
  panel_refresh = true;
}

void Systraybar::Clear(Timer& timer) {
  for (auto& traywin : list_icons) {
    RemoveIconInternal(traywin, timer);
  }

  list_icons.clear();

  // check empty systray
  if (VisibleIcons() == 0) {
    Hide();
  }

  // changed in systray
  need_resize_ = true;
  panel_refresh = true;
}

#ifdef _TINT3_DEBUG

std::string Systraybar::GetFriendlyName() const { return "Systraybar"; }

#endif  // _TINT3_DEBUG

void Systraybar::NetMessage(XClientMessageEvent* e) {
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

namespace {

void SystrayRenderIconNow(TrayWindow* traywin, Timer& timer) {
  // we end up in this function only in real transparency mode or if
  // systray_task_asb != 100 0 0
  // we made also sure, that we always have a 32 bit visual, i.e. we can safely
  // create 32 bit pixmaps here
  traywin->render_timeout.Clear();

  if (traywin->width == 0 || traywin->height == 0) {
    // reschedule rendering since the geometry information has not yet been
    // processed (can happen on slow cpu)
    SystrayRenderIcon(traywin, timer);
    return;
  }

  // good systray icons support 32 bit depth, but some icons are still 24 bit.
  // We create a heuristic mask for these icons, i.e. we get the rgb value in
  // the top left corner, and
  // mask out all pixel with the same rgb value
  Panel* panel = systray.panel_;

  // Very ugly hack, but somehow imlib2 is not able to get the image from the
  // traywindow itself,
  // so we first render the tray window onto a pixmap, and then we tell imlib2
  // to use this pixmap as
  // drawable. If someone knows why it does not work with the traywindow itself,
  // please tell me ;)
  Pixmap tmp_pmap = XCreatePixmap(server.dsp, server.root_window(),
                                  traywin->width, traywin->height, 32);
  XRenderPictFormat* f = nullptr;

  if (traywin->depth == 24) {
    f = XRenderFindStandardFormat(server.dsp, PictStandardRGB24);
  } else if (traywin->depth == 32) {
    f = XRenderFindStandardFormat(server.dsp, PictStandardARGB32);
  } else {
    util::log::Debug() << "Strange tray icon found with depth: "
                       << traywin->depth << '\n';
    return;
  }

  Picture pict_image =
      XRenderCreatePicture(server.dsp, traywin->tray_id, f, 0, 0);
  Picture pict_drawable = XRenderCreatePicture(
      server.dsp, tmp_pmap,
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

  if (image == nullptr) {
    return;
  }

  imlib_context_set_image(image);
  // if (traywin->depth == 24)
  // imlib_save_image("/home/thil77/test.jpg");
  imlib_image_set_has_alpha(1);
  DATA32* data = imlib_image_get_data();

  if (traywin->depth == 24) {
    CreateHeuristicMask(data, traywin->width, traywin->height);
  }

  if (systray.alpha != 100 || systray.brightness != 0 ||
      systray.saturation != 0) {
    AdjustASB(data, traywin->width, traywin->height, systray.alpha,
              (float)systray.saturation / 100, (float)systray.brightness / 100);
  }

  imlib_image_put_back_data(data);
  XCopyArea(server.dsp, render_background, systray.pix_, server.gc,
            traywin->x - systray.panel_x_, traywin->y - systray.panel_y_,
            traywin->width, traywin->height, traywin->x - systray.panel_x_,
            traywin->y - systray.panel_y_);
  RenderImage(systray.pix_, traywin->x - systray.panel_x_,
              traywin->y - systray.panel_y_, traywin->width, traywin->height);
  XCopyArea(server.dsp, systray.pix_, panel->main_win_, server.gc,
            traywin->x - systray.panel_x_, traywin->y - systray.panel_y_,
            traywin->width, traywin->height, traywin->x, traywin->y);
  imlib_free_image_and_decache();

  XFreePixmap(server.dsp, tmp_pmap);
  imlib_context_set_visual(server.visual);
  imlib_context_set_colormap(server.colormap);

  if (traywin->damage) {
    XDamageSubtract(server.dsp, traywin->damage, None, None);
  }

  XFlush(server.dsp);
}

}  // namespace

void SystrayRenderIcon(TrayWindow* traywin, Timer& timer) {
  if (server.real_transparency || systray.alpha != 100 ||
      systray.brightness != 0 || systray.saturation != 0) {
    // wine tray icons update whenever mouse is over them, so we limit the
    // updates to 50 ms
    if (!traywin->render_timeout) {
      traywin->render_timeout = timer.SetTimeout(
          std::chrono::milliseconds(50), [traywin, &timer]() -> bool {
            SystrayRenderIconNow(traywin, timer);
            return true;
          });
    }
  } else {
    // comment by andreas: I'm still not sure, what exactly we need to do
    // here... Somehow trayicons which do not
    // offer the same depth as tint3 does, need to draw a background pixmap, but
    // this cannot be done with
    // XCopyArea... So we actually need XRenderComposite???
    //          Pixmap pix = XCreatePixmap(server.dsp, server.root_window(),
    //          traywin->width, traywin->height, server.depth);
    //          XCopyArea(server.dsp, panel->temp_pmap, pix, server.gc,
    //          traywin->x, traywin->y, traywin->width, traywin->height, 0, 0);
    //          XSetWindowBackgroundPixmap(server.dsp, traywin->id, pix);
    XClearArea(server.dsp, traywin->tray_id, 0, 0, traywin->width,
               traywin->height, True);
  }
}

void RefreshSystrayIcon(Timer& timer) {
  for (auto& traywin : systray.list_icons) {
    if (!traywin->hide) {
      SystrayRenderIcon(traywin, timer);
    }
  }
}
