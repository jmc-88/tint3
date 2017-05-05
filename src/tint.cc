/**************************************************************************
*
* Tint3 panel
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

#include <X11/Xatom.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "config.hh"
#include "launcher/launcher.hh"
#include "launcher/xsettings-client.hh"
#include "panel.hh"
#include "server.hh"
#include "systray/systraybar.hh"
#include "taskbar/task.hh"
#include "taskbar/taskbar.hh"
#include "tooltip/tooltip.hh"
#include "util/common.hh"
#include "util/fs.hh"
#include "util/imlib2.hh"
#include "util/log.hh"
#include "util/timer.hh"
#include "util/window.hh"
#include "util/xdg.hh"
#include "version.hh"

namespace {

void PrintVersion() {
#ifdef _TINT3_DEBUG
  std::cout << "tint3 debug binary (built at " << GIT_BRANCH << "/"
            << GIT_COMMIT_HASH << ")\n";
#else  // _TINT3_DEBUG
  std::cout << "tint3 (v" << TINT3_RELEASE_VERSION << ")\n";
#endif  // _TINT3_DEBUG
}

void PrintUsage() { util::log::Error() << "Usage: tint3 [-c] <config_file>\n"; }

}  // namespace

// Drag and Drop state variables
Window dnd_source_window;
Window dnd_target_window;
int dnd_version;
Atom dnd_selection;
Atom dnd_atom;
int dnd_sent_request;
std::string dnd_launcher_exec;

void Init(int argc, char* argv[], std::string* config_path) {
  config_path->clear();

  // FIXME: remove this global data shit
  // set global data
  DefaultSystray();
#ifdef ENABLE_BATTERY
  DefaultBattery();
#endif  // ENABLE_BATTERY
  DefaultClock();
  DefaultLauncher();
  DefaultTaskbar();
  DefaultTooltip();
  DefaultPanel();

  // read options
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      PrintUsage();
      std::exit(0);
    }

    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      PrintVersion();
      std::exit(0);
    }

    if (!strcmp(argv[i], "-c")) {
      i++;

      if (i < argc) {
        config_path->assign(argv[i]);
      }
    }
  }

  // Set signal handler
  signal_pending = 0;
  auto signal_handler = [](int signal_number) -> void {
    signal_pending = signal_number;
  };

  SignalAction(SIGHUP, signal_handler);
  SignalAction(SIGINT, signal_handler);
  SignalAction(SIGTERM, signal_handler);
  SignalAction(SIGUSR1, signal_handler);
  SignalAction(SIGUSR2, signal_handler);
  SignalAction(SIGCHLD, SIG_DFL, SA_NOCLDWAIT);

  // BSD does not support pselect(), therefore we have to use select and hope
  // that we do not
  // end up in a race condition there (see 'man select()' on a linux machine for
  // more information)
  // block all signals, such that no race conditions occur before pselect in our
  // main loop
  //  sigset_t block_mask;
  //  sigaddset(&block_mask, SIGINT);
  //  sigaddset(&block_mask, SIGTERM);
  //  sigaddset(&block_mask, SIGHUP);
  //  sigaddset(&block_mask, SIGUSR1);
  //  sigprocmask(SIG_BLOCK, &block_mask, 0);
}

#ifdef HAVE_SN
static int error_trap_depth = 0;

static void ErrorTrapPush(SnDisplay* display, Display* xdisplay) {
  ++error_trap_depth;
}

static void ErrorTrapPop(SnDisplay* display, Display* xdisplay) {
  if (error_trap_depth == 0) {
    util::log::Error() << "Error trap underflow!\n";
    return;
  }

  XSync(xdisplay, False); /* get all errors out of the queue */
  --error_trap_depth;
}
#endif  // HAVE_SN

void InitX11() {
  server.dsp = XOpenDisplay(nullptr);

  if (!server.dsp) {
    util::log::Error() << "Couldn't open display.\n";
    std::exit(0);
  }

  server.InitX11();
  XSetErrorHandler(ServerCatchError);

#ifdef HAVE_SN
  // Initialize startup-notification
  server.sn_dsp = sn_display_new(server.dsp, ErrorTrapPush, ErrorTrapPop);
#endif  // HAVE_SN

  imlib_context_set_display(server.dsp);
  imlib_context_set_visual(server.visual);
  imlib_context_set_colormap(server.colormap);

  /* Catch events */
  XSelectInput(server.dsp, server.root_window(),
               PropertyChangeMask | StructureNotifyMask);

  setlocale(LC_ALL, "");
  // config file use '.' as decimal separator
  setlocale(LC_NUMERIC, "POSIX");

  // load default icon
  for (auto const& dir : util::xdg::basedir::DataDirs()) {
    std::string path(util::fs::Path(dir) / "tint3" / "default_icon.png");

    if (util::fs::FileExists(path)) {
      default_icon = imlib_load_image(path.c_str());
    }
  }

  if (default_icon == nullptr) {
    util::log::Error()
        << "Couldn't load the default icon, falling back to an empty image.\n";
    default_icon = imlib_create_image(48, 48);
    if (default_icon != nullptr) {
      imlib_context_set_image(default_icon);
      imlib_context_set_color(0, 0, 0, 255);
      imlib_image_fill_rectangle(0, 0, 48, 48);
    }
  }

  if (default_icon == nullptr) {
    util::log::Error() << "Couldn't even create a default icon image, "
                          "admitting defeat! The application may misbehave.\n";
  }

  // get monitor and desktop config
  GetMonitors();
  GetDesktops();
}

void Cleanup(Timer& timer) {
  CleanupSystray(timer);
  CleanupTooltip(timer);
  CleanupClock(timer);
  CleanupLauncher();
#ifdef ENABLE_BATTERY
  CleanupBattery(timer);
#endif
  CleanupPanel();

  imlib_context_disconnect_display();

  server.Cleanup();
}

void WindowAction(Task* tsk, MouseAction action) {
  if (!tsk) {
    return;
  }

  unsigned int desk = 0;

  switch (action) {
    case MouseAction::kClose:
      util::window::SetClose(tsk->win);
      break;

    case MouseAction::kToggle:
      util::window::SetActive(tsk->win);
      break;

    case MouseAction::kIconify:
      XIconifyWindow(server.dsp, tsk->win, server.screen);
      break;

    case MouseAction::kToggleIconify:
      if (task_active && tsk->win == task_active->win) {
        XIconifyWindow(server.dsp, tsk->win, server.screen);
      } else {
        util::window::SetActive(tsk->win);
      }
      break;

    case MouseAction::kShade:
      util::window::ToggleShade(tsk->win);
      break;

    case MouseAction::kMaximizeRestore:
      util::window::MaximizeRestore(tsk->win);
      break;

    case MouseAction::kMaximize:
      util::window::MaximizeRestore(tsk->win);
      break;

    case MouseAction::kRestore:
      util::window::MaximizeRestore(tsk->win);
      break;

    case MouseAction::kDesktopLeft:
      if (tsk->desktop == 0) {
        break;
      }

      desk = (tsk->desktop - 1);
      util::window::SetDesktop(tsk->win, desk);

      if (desk == server.desktop) {
        util::window::SetActive(tsk->win);
      }
      break;

    case MouseAction::kDesktopRight:
      if (tsk->desktop == server.num_desktops) {
        break;
      }

      desk = (tsk->desktop + 1);
      util::window::SetDesktop(tsk->win, desk);

      if (desk == server.desktop) {
        util::window::SetActive(tsk->win);
      }
      break;

    case MouseAction::kNextTask: {
      Task* tsk1 = NextTask(FindActiveTask(tsk, task_active));
      util::window::SetActive(tsk1->win);
    } break;

    case MouseAction::kPrevTask: {
      Task* tsk1 = PreviousTask(FindActiveTask(tsk, task_active));
      util::window::SetActive(tsk1->win);
    } break;

    // no-op for MouseActionEnum::kNone
    default:
      break;
  }
}

void ForwardClick(XEvent* e) {
  // forward the click to the desktop window (thanks conky)
  XUngrabPointer(server.dsp, e->xbutton.time);
  e->xbutton.window = server.root_window();
  // icewm doesn't open under the mouse.
  // and xfce doesn't open at all.
  e->xbutton.x = e->xbutton.x_root;
  e->xbutton.y = e->xbutton.y_root;
  XSendEvent(server.dsp, e->xbutton.window, False, ButtonPressMask, e);
}

void EventButtonPress(XEvent* e) {
  Panel* panel = GetPanel(e->xany.window);

  if (!panel) {
    return;
  }

  if (wm_menu && !panel->HandlesClick(&e->xbutton)) {
    ForwardClick(e);
    return;
  }

  task_drag = panel->ClickTask(e->xbutton.x, e->xbutton.y);

  if (panel_layer == PanelLayer::kBottom) {
    XLowerWindow(server.dsp, panel->main_win_);
  }
}

void EventButtonMotionNotify(XEvent* e) {
  Panel* panel = GetPanel(e->xany.window);

  if (!panel || !task_drag) {
    return;
  }

  // Find the taskbar on the event's location
  Taskbar* event_taskbar = panel->ClickTaskbar(e->xbutton.x, e->xbutton.y);

  if (event_taskbar == nullptr) {
    return;
  }

  // Find the task on the event's location
  Task* event_task = panel->ClickTask(e->xbutton.x, e->xbutton.y);

  // If the event takes place on the same taskbar as the task being dragged
  if (event_taskbar == task_drag->parent_) {
    // Swap the task_drag with the task on the event's location (if they differ)
    if (event_task != nullptr && event_task != task_drag) {
      auto& children = event_taskbar->children_;
      auto drag_iter = std::find(children.begin(), children.end(), task_drag);
      auto task_iter = std::find(children.begin(), children.end(), event_task);

      if (drag_iter != children.end() && task_iter != children.end()) {
        std::iter_swap(drag_iter, task_iter);
        event_taskbar->need_resize_ = true;
        task_dragged = true;
        panel_refresh = true;
      }
    }
  } else {  // The event is on another taskbar than the task being dragged
    if (task_drag->desktop == kAllDesktops ||
        panel_mode != PanelMode::kMultiDesktop) {
      return;
    }

    auto drag_taskbar = task_drag->parent_;
    auto drag_taskbar_iter =
        std::find(drag_taskbar->children_.begin(),
                  drag_taskbar->children_.end(), task_drag);

    if (drag_taskbar_iter != drag_taskbar->children_.end()) {
      drag_taskbar->children_.erase(drag_taskbar_iter);
    }

    if (event_taskbar->panel_x_ > drag_taskbar->panel_x_ ||
        event_taskbar->panel_y_ > drag_taskbar->panel_y_) {
      auto& children = event_taskbar->children_;
      size_t i = (taskbarname_enabled) ? 1 : 0;
      children.insert(children.begin() + i, task_drag);
    } else {
      event_taskbar->children_.push_back(task_drag);
    }

    // Move task to other desktop (but avoid the 'Window desktop changed' code
    // in 'event_property_notify')
    task_drag->parent_ = event_taskbar;
    task_drag->desktop = event_taskbar->desktop;

    util::window::SetDesktop(task_drag->win, event_taskbar->desktop);

    event_taskbar->need_resize_ = true;
    drag_taskbar->need_resize_ = true;
    task_dragged = true;
    panel_refresh = true;
  }
}

void EventButtonRelease(XEvent* e) {
  Panel* panel = GetPanel(e->xany.window);

  if (!panel) {
    return;
  }

  if (wm_menu && !panel->HandlesClick(&e->xbutton)) {
    ForwardClick(e);

    if (panel_layer == PanelLayer::kBottom) {
      XLowerWindow(server.dsp, panel->main_win_);
    }

    task_drag = nullptr;
    return;
  }

  static std::unordered_map<unsigned int, MouseAction> mouse_actions{
      {2, mouse_middle},      {3, mouse_right},     {4, mouse_scroll_up},
      {5, mouse_scroll_down}, {6, mouse_tilt_left}, {7, mouse_tilt_right},
  };

  MouseAction action = MouseAction::kNone;
  auto it = mouse_actions.find(e->xbutton.button);

  if (it != mouse_actions.end()) {
    action = it->second;
  } else {
    action = MouseAction::kToggleIconify;
  }

  if (panel->ClickClock(e->xbutton.x, e->xbutton.y)) {
    ClockAction(e->xbutton.button);

    if (panel_layer == PanelLayer::kBottom) {
      XLowerWindow(server.dsp, panel->main_win_);
    }

    task_drag = nullptr;
    return;
  }

  if (panel->ClickLauncher(e->xbutton.x, e->xbutton.y)) {
    LauncherIcon* icon = panel->ClickLauncherIcon(e->xbutton.x, e->xbutton.y);

    if (icon) {
      LauncherAction(icon, e);
    }

    task_drag = nullptr;
    return;
  }

  Taskbar* tskbar = panel->ClickTaskbar(e->xbutton.x, e->xbutton.y);

  if (!tskbar) {
    // TODO: check better solution to keep window below
    if (panel_layer == PanelLayer::kBottom) {
      XLowerWindow(server.dsp, panel->main_win_);
    }

    task_drag = nullptr;
    return;
  }

  // drag and drop task
  if (task_dragged) {
    task_drag = nullptr;
    task_dragged = false;
    return;
  }

  // switch desktop
  if (panel_mode == PanelMode::kMultiDesktop) {
    if (tskbar->desktop != server.desktop && action != MouseAction::kClose &&
        action != MouseAction::kDesktopLeft &&
        action != MouseAction::kDesktopRight) {
      SetDesktop(tskbar->desktop);
    }
  }

  // action on task
  WindowAction(panel->ClickTask(e->xbutton.x, e->xbutton.y), action);

  // to keep window below
  if (panel_layer == PanelLayer::kBottom) {
    XLowerWindow(server.dsp, panel->main_win_);
  }
}

void EventPropertyNotify(XEvent* e, Timer& timer) {
  Task* tsk;
  Window win = e->xproperty.window;
  Atom at = e->xproperty.atom;

  if (xsettings_client) {
    XSettingsClientProcessEvent(xsettings_client, e);
  }

  if (win == server.root_window()) {
    if (!server.got_root_win) {
      XSelectInput(server.dsp, server.root_window(),
                   PropertyChangeMask | StructureNotifyMask);
      server.got_root_win = true;
    }
    // Change name of desktops
    else if (at == server.atoms_["_NET_DESKTOP_NAMES"]) {
      if (!taskbarname_enabled) {
        return;
      }

      auto desktop_names = ServerGetDesktopNames();
      auto it = desktop_names.begin();

      for (Panel& panel : panels) {
        for (unsigned int i = 0; i < panel.num_desktops_; ++i) {
          std::string name;

          if (it != desktop_names.end()) {
            name = (*it++);
          } else {
            name = util::string::Representation(i + 1);
          }

          Taskbar& tskbar = panel.taskbar_[i];

          if (tskbar.bar_name.name() != name) {
            tskbar.bar_name.set_name(name);
            tskbar.bar_name.need_resize_ = true;
          }
        }
      }

      panel_refresh = true;
    }
    // Change number of desktops
    else if (at == server.atoms_["_NET_NUMBER_OF_DESKTOPS"]) {
      if (!taskbar_enabled) {
        return;
      }

      server.num_desktops = server.GetNumberOfDesktops();

      if (server.desktop >= server.num_desktops) {
        server.desktop = server.num_desktops - 1;
      }

      CleanupTaskbar();
      InitTaskbar();

      for (Panel& panel : panels) {
        Taskbar::InitPanel(&panel);
        panel.SetItemsOrder();
        panel.UpdateTaskbarVisibility();
        panel.need_resize_ = true;
      }

      TaskRefreshTasklist(timer);
      ActiveTask();
      panel_refresh = true;
    }
    // Change desktop
    else if (at == server.atoms_["_NET_CURRENT_DESKTOP"]) {
      if (!taskbar_enabled) {
        return;
      }

      unsigned int old_desktop = server.desktop;
      server.desktop = server.GetCurrentDesktop();

      util::log::Debug() << "Current desktop changed from " << old_desktop
                         << " to " << server.desktop << '\n';

      for (Panel& panel : panels) {
        panel.taskbar_[old_desktop].SetState(kTaskbarNormal);
        panel.taskbar_[server.desktop].SetState(kTaskbarActive);
        // check ALLDESKTOP task => resize taskbar

        if (server.num_desktops > old_desktop) {
          Taskbar& tskbar = panel.taskbar_[old_desktop];
          auto it = tskbar.children_.begin();

          if (taskbarname_enabled) {
            ++it;
          }

          for (; it != tskbar.children_.end(); ++it) {
            auto tsk = static_cast<Task*>(*it);

            if (tsk->desktop == kAllDesktops) {
              tsk->on_screen_ = false;
              tskbar.need_resize_ = true;
              panel_refresh = true;
            }
          }
        }

        Taskbar& tskbar = panel.taskbar_[server.desktop];
        auto it = tskbar.children_.begin();

        if (taskbarname_enabled) {
          ++it;
        }

        for (; it != tskbar.children_.end(); ++it) {
          auto tsk = static_cast<Task*>(*it);

          if (tsk->desktop == kAllDesktops) {
            tsk->on_screen_ = true;
            tskbar.need_resize_ = true;
          }
        }
      }
    }
    // Window list
    else if (at == server.atoms_["_NET_CLIENT_LIST"]) {
      TaskRefreshTasklist(timer);
      panel_refresh = true;
    }
    // Change active
    else if (at == server.atoms_["_NET_ACTIVE_WINDOW"]) {
      ActiveTask();
      panel_refresh = true;
    } else if (at == server.atoms_["_XROOTPMAP_ID"] ||
               at == server.atoms_["_XROOTMAP_ID"]) {
      // change Wallpaper
      for (Panel& panel : panels) {
        panel.SetBackground();
      }
      panel_refresh = true;
    }
  } else {
    tsk = TaskGetTask(win);

    if (!tsk) {
      if (at != server.atoms_["_NET_WM_STATE"]) {
        return;
      } else {
        // xfce4 sends _NET_WM_STATE after minimized to tray, so we need to
        // check if window is mapped
        // if it is mapped and not set as skip_taskbar, we must add it to our
        // task list
        XWindowAttributes wa;
        XGetWindowAttributes(server.dsp, win, &wa);

        if (wa.map_state != IsViewable || util::window::IsSkipTaskbar(win)) {
          return;
        }

        if (!(tsk = AddTask(win, timer))) {
          return;
        }

        panel_refresh = true;
      }
    }

    // Window title changed
    if (at == server.atoms_["_NET_WM_VISIBLE_NAME"] ||
        at == server.atoms_["_NET_WM_NAME"] || at == server.atoms_["WM_NAME"]) {
      if (tsk->UpdateTitle()) {
        if (g_tooltip.IsBoundTo(tsk)) {
          g_tooltip.BindTo(tsk);
          g_tooltip.Update(timer);
        }

        panel_refresh = true;
      }
    }
    // Demand attention
    else if (at == server.atoms_["_NET_WM_STATE"]) {
      if (util::window::IsUrgent(win)) {
        tsk->AddUrgent();
      }

      if (util::window::IsSkipTaskbar(win)) {
        RemoveTask(tsk);
        panel_refresh = true;
      }
    } else if (at == server.atoms_["WM_STATE"]) {
      // Iconic state
      int state = (task_active != nullptr && tsk->win == task_active->win)
                      ? kTaskActive
                      : kTaskNormal;

      if (util::window::IsIconified(win)) {
        state = kTaskIconified;
      }

      tsk->SetState(state);
      panel_refresh = true;
    }
    // Window icon changed
    else if (at == server.atoms_["_NET_WM_ICON"]) {
      GetIcon(tsk);
      panel_refresh = true;
    }
    // Window desktop changed
    else if (at == server.atoms_["_NET_WM_DESKTOP"]) {
      unsigned int desktop = util::window::GetDesktop(win);

      util::log::Debug() << "Window desktop changed from " << tsk->desktop
                         << " to " << desktop << '\n';

      // bug in windowmaker : send unecessary 'desktop changed' when focus
      // changed
      if (desktop != tsk->desktop) {
        RemoveTask(tsk);
        AddTask(win, timer);
        ActiveTask();
        panel_refresh = true;
      }
    } else if (at == server.atoms_["WM_HINTS"]) {
      util::x11::ClientData<XWMHints> wmhints(XGetWMHints(server.dsp, win));

      if (wmhints != nullptr && wmhints->flags & XUrgencyHint) {
        tsk->AddUrgent();
      }
    }

    if (!server.got_root_win) {
      server.UpdateRootWindow();
    }
  }
}

void EventExpose(XEvent* e) {
  Panel* panel = GetPanel(e->xany.window);

  if (!panel) {
    return;
  }

  // TODO : one panel_refresh per panel ?
  panel_refresh = true;
}

void EventConfigureNotify(Window win, Timer& timer) {
  // change in root window (xrandr)
  if (win == server.root_window()) {
    signal_pending = SIGUSR1;
    return;
  }

  // 'win' is a tray icon
  for (auto& traywin : systray.list_icons) {
    if (traywin->tray_id == win) {
      XMoveResizeWindow(server.dsp, traywin->id, traywin->x, traywin->y,
                        traywin->width, traywin->height);
      XResizeWindow(server.dsp, traywin->tray_id, traywin->width,
                    traywin->height);
      panel_refresh = true;
      return;
    }
  }

  // 'win' move in another monitor
  if (panels.size() == 1) {
    return;
  }

  Task* tsk = TaskGetTask(win);

  if (!tsk) {
    return;
  }

  Panel* p = tsk->panel_;

  if (p->monitor_ != util::window::GetMonitor(win)) {
    RemoveTask(tsk);
    tsk = AddTask(win, timer);

    if (win == util::window::GetActive()) {
      tsk->SetState(kTaskActive);
      task_active = tsk;
    }

    panel_refresh = true;
  }
}

char const* GetAtomName(Display* disp, Atom a) {
  if (a == None) {
    return "None";
  }

  return XGetAtomName(disp, a);
}

struct Property {
  unsigned char* data;
  int format, nitems;
  Atom type;
};

// This fetches all the data from a property
Property ReadProperty(Display* disp, Window w, Atom property) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char* ret = nullptr;

  int read_bytes = 1024;

  // Keep trying to read the property until there are no
  // bytes unread.
  do {
    if (ret != nullptr) {
      XFree(ret);
    }

    XGetWindowProperty(disp, w, property, 0, read_bytes, False, AnyPropertyType,
                       &actual_type, &actual_format, &nitems, &bytes_after,
                       &ret);
    read_bytes *= 2;
  } while (bytes_after != 0);

  util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__ << ": Property:\n"
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Actual type: " << GetAtomName(disp, actual_type)
                     << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Actual format: " << actual_format << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Number of items: " << nitems << '\n';

  Property p;
  p.data = ret;
  p.format = actual_format;
  p.nitems = nitems;
  p.type = actual_type;

  return p;
}

// This function takes a list of targets which can be converted to (atom_list,
// nitems)
// and a list of acceptable targets with prioritees (datatypes). It returns the
// highest
// entry in datatypes which is also in atom_list: ie it finds the best match.
Atom PickTargetFromList(Display* disp, Atom* atom_list, int nitems) {
  Atom to_be_requested = None;

  for (int i = 0; i < nitems; ++i) {
    char const* atom_name = GetAtomName(disp, atom_list[i]);

    util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__ << ": Type "
                       << i << " = " << atom_name << '\n';

    // See if this data type is allowed and of higher priority (closer to zero)
    // than the present one.
    if (strcmp(atom_name, "STRING") == 0) {
      to_be_requested = atom_list[i];
    }
  }

  return to_be_requested;
}

// Finds the best target given up to three atoms provided (any can be None).
// Useful for part of the Xdnd protocol.
Atom PickTargetFromAtoms(Display* disp, Atom t1, Atom t2, Atom t3) {
  Atom atoms[3];
  int n = 0;

  if (t1 != None) {
    atoms[n++] = t1;
  }

  if (t2 != None) {
    atoms[n++] = t2;
  }

  if (t3 != None) {
    atoms[n++] = t3;
  }

  return PickTargetFromList(disp, atoms, n);
}

// Finds the best target given a local copy of a property.
Atom PickTargetFromTargets(Display* disp, Property p) {
  // The list of targets is a list of atoms, so it should have type XA_ATOM
  // but it may have the type TARGETS instead.

  if ((p.type != XA_ATOM && p.type != server.atoms_["TARGETS"]) ||
      p.format != 32) {
    // This would be really broken. Targets have to be an atom list
    // and applications should support this. Nevertheless, some
    // seem broken (MATLAB 7, for instance), so ask for STRING
    // next instead as the lowest common denominator
    return XA_STRING;
  } else {
    Atom* atom_list = (Atom*)p.data;

    return PickTargetFromList(disp, atom_list, p.nitems);
  }
}

void DragAndDropEnter(XClientMessageEvent* e) {
  dnd_atom = None;
  int more_than_3 = e->data.l[1] & 1;
  dnd_source_window = e->data.l[0];
  dnd_version = (e->data.l[1] >> 24);

  util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__ << ": DnDEnter\n"
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": DnDEnter. Supports > 3 types = "
                     << (more_than_3 ? "yes" : "no") << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Protocol version = " << dnd_version << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Type 1 = " << GetAtomName(server.dsp, e->data.l[2])
                     << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Type 2 = " << GetAtomName(server.dsp, e->data.l[3])
                     << '\n'
                     << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Type 3 = " << GetAtomName(server.dsp, e->data.l[4])
                     << '\n';

  // Query which conversions are available and pick the best

  if (more_than_3) {
    // Fetch the list of possible conversions
    // Notice the similarity to TARGETS with paste.
    Property p = ReadProperty(server.dsp, dnd_source_window,
                              server.atoms_["XdndTypeList"]);
    dnd_atom = PickTargetFromTargets(server.dsp, p);
    XFree(p.data);
  } else {
    // Use the available list
    dnd_atom = PickTargetFromAtoms(server.dsp, e->data.l[2], e->data.l[3],
                                   e->data.l[4]);
  }

  util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__
                     << ": Requested type = "
                     << GetAtomName(server.dsp, dnd_atom) << '\n';
}

void DragAndDropPosition(XClientMessageEvent* e) {
  bool accept = false;

  dnd_target_window = e->window;

  int x = (e->data.l[2] >> 16) & 0xFFFF;
  int y = e->data.l[2] & 0xFFFF;
  int map_x, map_y;
  Window child;
  XTranslateCoordinates(server.dsp, server.root_window(), e->window, x, y,
                        &map_x, &map_y, &child);

  Panel* panel = GetPanel(e->window);
  Task* task = panel->ClickTask(map_x, map_y);

  if (task) {
    if (task->desktop != server.desktop) {
      SetDesktop(task->desktop);
    }

    WindowAction(task, MouseAction::kToggle);
  } else {
    LauncherIcon* icon = panel->ClickLauncherIcon(map_x, map_y);

    if (icon) {
      accept = true;
      dnd_launcher_exec = icon->cmd_;
    } else {
      dnd_launcher_exec.clear();
    }
  }

  // send XdndStatus event to get more XdndPosition events
  XClientMessageEvent se;
  se.type = ClientMessage;
  se.window = e->data.l[0];
  se.message_type = server.atoms_["XdndStatus"];
  se.format = 32;
  se.data.l[0] = e->window;  // XID of the target window
  // bit 0: accept drop, bit 1: send XdndPosition events if inside rectangle
  se.data.l[1] = accept ? 1 : 0;
  // Rectangle x,y for which no more XdndPosition events
  se.data.l[2] = 0;
  // Rectangle w,h for which no more XdndPosition events
  se.data.l[3] = (1 << 16) | 1;

  if (accept) {
    se.data.l[4] =
        (dnd_version >= 2) ? e->data.l[4] : server.atoms_["XdndActionCopy"];
  } else {
    se.data.l[4] = None;  // None = drop will not be accepted
  }

  XSendEvent(server.dsp, e->data.l[0], False, NoEventMask, (XEvent*)&se);
}

void DragAndDropDrop(XClientMessageEvent* e) {
  if (dnd_target_window && !dnd_launcher_exec.empty()) {
    if (dnd_version >= 1) {
      XConvertSelection(server.dsp, server.atoms_["XdndSelection"], XA_STRING,
                        dnd_selection, dnd_target_window, e->data.l[2]);
    } else {
      XConvertSelection(server.dsp, server.atoms_["XdndSelection"], XA_STRING,
                        dnd_selection, dnd_target_window, CurrentTime);
    }
  } else {
    // The source is sending anyway, despite instructions to the contrary.
    // So reply that we're not interested.
    XClientMessageEvent m;
    std::memset(&m, 0, sizeof(m));
    m.type = ClientMessage;
    m.display = e->display;
    m.window = e->data.l[0];
    m.message_type = server.atoms_["XdndFinished"];
    m.format = 32;
    m.data.l[0] = dnd_target_window;
    m.data.l[1] = 0;
    m.data.l[2] = None;  // Failed.
    XSendEvent(server.dsp, e->data.l[0], False, NoEventMask, (XEvent*)&m);
  }
}

int main(int argc, char* argv[]) {
start:
  std::string config_path;
  Init(argc, argv, &config_path);
  InitX11();

  Timer timer;

  config::Reader config_reader{&server};
  bool config_read = false;

  if (!config_path.empty()) {
    config_read = config_reader.LoadFromFile(config_path.c_str());
  } else {
    config_read = config_reader.LoadFromDefaults();
  }

  if (!config_read) {
    util::log::Error() << "usage: tint3 [-c] <config_file>\n";
    Cleanup(timer);
    std::exit(1);
  }

  InitPanel(timer);

#ifdef _TINT3_DEBUG

  unsigned int panel_index = 0;
  for (Panel& panel : panels) {
    util::log::Debug() << "Panel " << (++panel_index) << ":\n";
    panel.PrintTree();
  }

#endif  // _TINT3_DEBUG

  int xdamage_event, xdamage_error;
  if (!XDamageQueryExtension(server.dsp, &xdamage_event, &xdamage_error)) {
    util::log::Error() << "Couldn't initialize XDAMAGE.\n";
  }

  int xfixes_event, xfixes_error;
  if (!XFixesQueryExtension(server.dsp, &xfixes_event, &xfixes_error)) {
    util::log::Error() << "Couldn't initialize XFIXES.\n";
  }

  XSync(server.dsp, False);

  // XDND initialization
  dnd_source_window = 0;
  dnd_target_window = 0;
  dnd_version = 0;
  dnd_selection = XInternAtom(server.dsp, "PRIMARY", 0);
  dnd_atom = None;
  dnd_sent_request = 0;
  dnd_launcher_exec.clear();

  util::x11::EventLoop event_loop(&server, timer);

  if (!event_loop.IsAlive()) {
    Cleanup(timer);
    std::exit(1);
  }

  // Setup a handler for child termination
  pending_children = false;
  SignalAction(SIGCHLD, [](int) -> void { pending_children = true; });

  // Pointer to the Area that was last activated by a mouse effect.
  Area* previous_mouse_over_area = nullptr;

  for (auto& panel : panels) {
    XFixesSelectSelectionInput(server.dsp, panel.main_win_,
                               server.atoms_["_NET_WM_CM_SCREEN"],
                               XFixesSetSelectionOwnerNotifyMask |
                                   XFixesSelectionWindowDestroyNotifyMask |
                                   XFixesSelectionClientCloseNotifyMask);
  }

  event_loop.RegisterHandler(
      xfixes_event + XFixesSelectionNotify, [&](XEvent& e) -> void {
        XFixesSelectionNotifyEvent* ev =
            reinterpret_cast<XFixesSelectionNotifyEvent*>(&e);

        Panel* panel = GetPanel(ev->window);
        if (!panel) {
          return;
        }

        // Compositing was enabled or disabled, reinitialize the panel
        signal_pending = SIGUSR1;
      });

  event_loop.RegisterHandler(ButtonPress, [&](XEvent& e) -> void {
    TooltipHide(timer);
    EventButtonPress(&e);

    Panel* panel = GetPanel(e.xmotion.window);
    if (!panel) {
      return;
    }

    previous_mouse_over_area =
        panel->InnermostAreaUnderPoint(e.xmotion.x, e.xmotion.y)
            ->MouseOver(previous_mouse_over_area, /*button_pressed=*/true);
  });

  event_loop.RegisterHandler(ButtonRelease, [&](XEvent& e) -> void {
    EventButtonRelease(&e);

    Panel* panel = GetPanel(e.xmotion.window);
    if (!panel) {
      return;
    }

    previous_mouse_over_area =
        panel->InnermostAreaUnderPoint(e.xmotion.x, e.xmotion.y)
            ->MouseOver(previous_mouse_over_area, /*button_pressed=*/false);
  });

  event_loop.RegisterHandler(MotionNotify, [&](XEvent& e) -> void {
    static constexpr unsigned int button_mask =
        Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;
    const bool button_pressed = (e.xmotion.state & button_mask) != 0;

    if (button_pressed) {
      EventButtonMotionNotify(&e);
    }

    Panel* panel = GetPanel(e.xmotion.window);
    Area* area = panel->InnermostAreaUnderPoint(e.xmotion.x, e.xmotion.y);
    std::string tooltip = area->GetTooltipText();

    if (!tooltip.empty()) {
      TooltipTriggerShow(area, panel, &e, timer);
    } else {
      TooltipTriggerHide(timer);
    }

    previous_mouse_over_area =
        area->MouseOver(previous_mouse_over_area, button_pressed);
  });

  event_loop.RegisterHandler(LeaveNotify, [&](XEvent& e) -> void {
    TooltipTriggerHide(timer);

    if (previous_mouse_over_area != nullptr) {
      previous_mouse_over_area->MouseLeave();
      previous_mouse_over_area = nullptr;
    }
  });

  event_loop.RegisterHandler(Expose,
                             [](XEvent& e) -> void { EventExpose(&e); });

  event_loop.RegisterHandler(PropertyNotify, [&timer](XEvent& e) -> void {
    EventPropertyNotify(&e, timer);
  });

  event_loop.RegisterHandler(ConfigureNotify, [&timer](XEvent& e) -> void {
    EventConfigureNotify(e.xconfigure.window, timer);
  });

  event_loop.RegisterHandler(ReparentNotify, [&](XEvent& e) -> void {
    if (!systray_enabled) {
      return;
    }

    // reparented to us
    if (e.xany.window == systray.panel_->main_win_) {
      return;
    }

    // FIXME: 'reparent to us' badly detected => disabled
  });

  event_loop.RegisterHandler(
      {UnmapNotify, DestroyNotify},
      [&](XEvent& e) -> void {
        if (e.xany.window == g_tooltip.window || !systray_enabled) {
          return;
        }

        for (auto& traywin : systray.list_icons) {
          if (traywin->tray_id == e.xany.window) {
            systray.RemoveIcon(traywin, timer);
            return;
          }
        }
      });

  event_loop.RegisterHandler(ClientMessage, [&](XEvent& e) -> void {
    if (systray_enabled &&
        e.xclient.message_type == server.atoms_["_NET_SYSTEM_TRAY_OPCODE"] &&
        e.xclient.format == 32 && e.xclient.window == net_sel_win) {
      systray.NetMessage(&e.xclient);
    } else if (e.xclient.message_type == server.atoms_["XdndEnter"]) {
      DragAndDropEnter(&e.xclient);
    } else if (e.xclient.message_type == server.atoms_["XdndPosition"]) {
      DragAndDropPosition(&e.xclient);
    } else if (e.xclient.message_type == server.atoms_["XdndDrop"]) {
      DragAndDropDrop(&e.xclient);
    }
  });

  event_loop.RegisterHandler(SelectionNotify, [&](XEvent& e) -> void {
    Atom target = e.xselection.target;

    util::log::Debug()
        << "DnD " << __FILE__ << ':' << __LINE__
        << ": A selection notify has arrived!\n"
        << "DnD " << __FILE__ << ':' << __LINE__
        << ": Requestor = " << e.xselectionrequest.requestor << '\n'
        << "DnD " << __FILE__ << ':' << __LINE__ << ": Selection atom = "
        << GetAtomName(server.dsp, e.xselection.selection) << '\n'
        << "DnD " << __FILE__ << ':' << __LINE__
        << ": Target atom    = " << GetAtomName(server.dsp, target) << '\n'
        << "DnD " << __FILE__ << ':' << __LINE__ << ": Property atom  = "
        << GetAtomName(server.dsp, e.xselection.property) << '\n';

    if (e.xselection.property != None && !dnd_launcher_exec.empty()) {
      Property prop =
          ReadProperty(server.dsp, dnd_target_window, dnd_selection);

      // If we're being given a list of targets (possible conversions)
      if (target == server.atoms_["TARGETS"] && !dnd_sent_request) {
        dnd_sent_request = 1;
        dnd_atom = PickTargetFromTargets(server.dsp, prop);

        if (dnd_atom == None) {
          util::log::Debug() << "No matching datatypes.\n";
        } else {
          // Request the data type we are able to select
          util::log::Debug() << "Now requesting type "
                             << GetAtomName(server.dsp, dnd_atom) << '\n';
          XConvertSelection(server.dsp, dnd_selection, dnd_atom, dnd_selection,
                            dnd_target_window, CurrentTime);
        }
      } else if (target == dnd_atom) {
        // Dump the binary data
        util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__
                           << ": Data begins:\n";
        util::log::Debug() << "--------\n";

        for (int i = 0; i < prop.nitems * prop.format / 8; i++) {
          util::log::Debug() << ((char*)prop.data)[i];
        }

        util::log::Debug() << "--------\n";

        util::string::Builder cmd;
        cmd << '(' << dnd_launcher_exec << " \"";

        for (int i = 0; i < prop.nitems * prop.format / 8; i++) {
          char c = ((char*)prop.data)[i];

          if (c == '\n') {
            if (i < prop.nitems * prop.format / 8 - 1) {
              cmd << "\" \"";
            }
          } else {
            if (c == '`' || c == '$' || c == '\\') {
              cmd << '\\';
            }

            cmd << c;
          }
        }

        cmd << "\"&";
        util::log::Debug() << "DnD " << __FILE__ << ':' << __LINE__
                           << ": Running command: \"" << std::string(cmd)
                           << "\"\n";
        TintShellExec(cmd);

        // Reply OK.
        XClientMessageEvent m;
        std::memset(&m, 0, sizeof(m));
        m.type = ClientMessage;
        m.display = server.dsp;
        m.window = dnd_source_window;
        m.message_type = server.atoms_["XdndFinished"];
        m.format = 32;
        m.data.l[0] = dnd_target_window;
        m.data.l[1] = 1;
        m.data.l[2] = server.atoms_["XdndActionCopy"];  // We only ever copy.
        XSendEvent(server.dsp, dnd_source_window, False, NoEventMask,
                   (XEvent*)&m);
        XSync(server.dsp, False);
      }

      XFree(prop.data);
    }
  });

  event_loop.RegisterHandler(
      xdamage_event + XDamageNotify, [&](XEvent& e) -> void {
        XDamageNotifyEvent* ev = reinterpret_cast<XDamageNotifyEvent*>(&e);

        for (auto& traywin : systray.list_icons) {
          if (traywin->id == ev->drawable) {
            SystrayRenderIcon(traywin, timer);
            return;
          }
        }
      });

  if (event_loop.RunLoop()) {
    systray.Clear(timer);
    Cleanup(timer);

    // Reinitialize tint3.
    if (signal_pending == SIGUSR1) {
      goto start;  // brrr
    }
    // Try to replace the process with a new instance of itself.
    else if (signal_pending == SIGUSR2) {
      if (execvp(argv[0], argv) == -1) {
        util::log::Error() << "execv() failed: terminating tint3\n";
        return 1;
      }
    }
  }

  // Make sure we reparent all the tray icon windows to the server root window
  // before we exit the program and destroy our own tray window.
  // FIXME: this wouldn't need manual deletion if there was no global state.
  Cleanup(timer);
  return 0;
}
