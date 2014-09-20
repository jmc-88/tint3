/**************************************************************************
*
* Tint3 panel
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>
#include <X11/extensions/Xdamage.h>
#include <Imlib2.h>
#include <signal.h>

#ifdef HAVE_SN
#include <sys/wait.h>
#endif

#include <algorithm>

#include "version.h"
#include "server.h"
#include "window.h"
#include "config.h"
#include "task.h"
#include "taskbar.h"
#include "systraybar.h"
#include "launcher.h"
#include "panel.h"
#include "tooltip.h"
#include "timer.h"
#include "util/common.h"
#include "util/fs.h"
#include "xsettings-client.h"

// Drag and Drop state variables
Window dnd_source_window;
Window dnd_target_window;
int dnd_version;
Atom dnd_selection;
Atom dnd_atom;
int dnd_sent_request;
char* dnd_launcher_exec;

void signal_handler(int sig) {
    // signal handler is light as it should be
    signal_pending = sig;
}


void init(int argc, char* argv[]) {
    // set global data
    default_config();
    default_timeout();
    default_systray();
#ifdef ENABLE_BATTERY
    default_battery();
#endif
    default_clock();
    default_launcher();
    default_taskbar();
    default_tooltip();
    default_panel();

    // read options
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))   {
            printf("Usage: tint3 [-c] <config_file>\n");
            exit(0);
        }

        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))    {
            printf("tint3 version %s\n", VERSION_STRING);
            exit(0);
        }

        if (!strcmp(argv[i], "-c")) {
            i++;

            if (i < argc) {
                config_path = strdup(argv[i]);
            }
        }

        if (!strcmp(argv[i], "-s")) {
            i++;

            if (i < argc) {
                snapshot_path = strdup(argv[i]);
            }
        }
    }

    // Set signal handler
    signal_pending = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGUSR1, &sa, 0);
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    sigaction(SIGHUP, &sa, 0);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = SIG_DFL;
    sa_chld.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa_chld, 0);

    // BSD does not support pselect(), therefore we have to use select and hope that we do not
    // end up in a race condition there (see 'man select()' on a linux machine for more information)
    // block all signals, such that no race conditions occur before pselect in our main loop
    //  sigset_t block_mask;
    //  sigaddset(&block_mask, SIGINT);
    //  sigaddset(&block_mask, SIGTERM);
    //  sigaddset(&block_mask, SIGHUP);
    //  sigaddset(&block_mask, SIGUSR1);
    //  sigprocmask(SIG_BLOCK, &block_mask, 0);
}

#ifdef HAVE_SN
static int error_trap_depth = 0;

static void
error_trap_push(SnDisplay* display,
                Display*   xdisplay) {
    ++error_trap_depth;
}

static void
error_trap_pop(SnDisplay* display,
               Display*   xdisplay) {
    if (error_trap_depth == 0) {
        fprintf(stderr, "Error trap underflow!\n");
        return;
    }

    XSync(xdisplay, False); /* get all errors out of the queue */
    --error_trap_depth;
}

static void sigchld_handler(int /* signal */) {
    // Wait for all dead processes
    pid_t pid;

    while ((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {
        auto it = server.pids.find(pid);

        if (it != server.pids.end()) {
            fprintf(stderr, "Unknown child %d terminated!\n", pid);
        } else {
            sn_launcher_context_complete(it->second);
            sn_launcher_context_unref(it->second);
            server.pids.erase(it);
        }
    }
}
#endif // HAVE_SN

void init_X11() {
    server.dsp = XOpenDisplay(nullptr);

    if (!server.dsp) {
        fprintf(stderr, "tint3 exit : could not open display.\n");
        exit(0);
    }

    server_init_atoms();
    server.screen = DefaultScreen(server.dsp);
    server.root_win = RootWindow(server.dsp, server.screen);
    server.desktop = server_get_current_desktop();
    server_init_visual();
    XSetErrorHandler((XErrorHandler) server_catch_error);

#ifdef HAVE_SN
    // Initialize startup-notification
    server.sn_dsp = sn_display_new(server.dsp, error_trap_push, error_trap_pop);

    // Setup a handler for child termination
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = sigchld_handler;

    if (sigaction(SIGCHLD, &act, 0)) {
        perror("sigaction");
    }

#endif // HAVE_SN

    imlib_context_set_display(server.dsp);
    imlib_context_set_visual(server.visual);
    imlib_context_set_colormap(server.colormap);

    /* Catch events */
    XSelectInput(
        server.dsp,
        server.root_win,
        PropertyChangeMask | StructureNotifyMask);

    setlocale(LC_ALL, "");
    // config file use '.' as decimal separator
    setlocale(LC_NUMERIC, "POSIX");

    // load default icon
    const gchar* const* data_dirs = g_get_system_data_dirs();

    for (int i = 0; data_dirs[i] != nullptr; i++)  {
        auto path = fs::BuildPath({ data_dirs[i], "tint3", "default_icon.png" });

        if (fs::FileExists(path)) {
            default_icon = imlib_load_image(path.c_str());
        }
    }

    // get monitor and desktop config
    get_monitors();
    get_desktops();
}


void cleanup() {
    cleanup_systray();
    cleanup_tooltip();
    cleanup_clock();
    cleanup_launcher();
#ifdef ENABLE_BATTERY
    cleanup_battery();
#endif
    cleanup_panel();

    if (default_icon) {
        imlib_context_set_image(default_icon);
        imlib_free_image();
    }

    imlib_context_disconnect_display();

    cleanup_server();
    cleanup_timeout();

    if (server.dsp) {
        XCloseDisplay(server.dsp);
    }
}


void get_snapshot(const char* path) {
    Panel* panel = &panel1[0];

    if (panel->width > server.monitor[0].width) {
        panel->width = server.monitor[0].width;
    }

    panel->temp_pmap = XCreatePixmap(server.dsp, server.root_win, panel->width,
                                     panel->height, server.depth);
    panel->render();

    Imlib_Image img = nullptr;
    imlib_context_set_drawable(panel->temp_pmap);
    img = imlib_create_image_from_drawable(0, 0, 0, panel->width,
                                           panel->height, 0);

    imlib_context_set_image(img);

    if (!panel_horizontal) {
        // rotate 90° vertical panel
        imlib_image_flip_horizontal();
        imlib_image_flip_diagonal();
    }

    imlib_save_image(path);
    imlib_free_image();
}


void window_action(Task* tsk, int action) {
    if (!tsk) {
        return;
    }

    int desk;

    switch (action) {
        case CLOSE:
            set_close(tsk->win);
            break;

        case TOGGLE:
            set_active(tsk->win);
            break;

        case ICONIFY:
            XIconifyWindow(server.dsp, tsk->win, server.screen);
            break;

        case TOGGLE_ICONIFY:
            if (task_active && tsk->win == task_active->win) {
                XIconifyWindow(server.dsp, tsk->win, server.screen);
            } else {
                set_active(tsk->win);
            }

            break;

        case SHADE:
            window_toggle_shade(tsk->win);
            break;

        case MAXIMIZE_RESTORE:
            window_maximize_restore(tsk->win);
            break;

        case MAXIMIZE:
            window_maximize_restore(tsk->win);
            break;

        case RESTORE:
            window_maximize_restore(tsk->win);
            break;

        case DESKTOP_LEFT:
            if (tsk->desktop == 0) {
                break;
            }

            desk = tsk->desktop - 1;
            windows_set_desktop(tsk->win, desk);

            if (desk == server.desktop) {
                set_active(tsk->win);
            }

            break;

        case DESKTOP_RIGHT:
            if (tsk->desktop == server.nb_desktop) {
                break;
            }

            desk = tsk->desktop + 1;
            windows_set_desktop(tsk->win, desk);

            if (desk == server.desktop) {
                set_active(tsk->win);
            }

            break;

        case NEXT_TASK: {
                Task* tsk1;
                tsk1 = next_task(find_active_task(tsk, task_active));
                set_active(tsk1->win);
            }
            break;

        case PREV_TASK: {
                Task* tsk1;
                tsk1 = prev_task(find_active_task(tsk, task_active));
                set_active(tsk1->win);
            }
    }
}


bool tint3_handles_click(Panel* panel, XButtonEvent* e) {
    Task* task = click_task(panel, e->x, e->y);

    if (task) {
        return ((e->button == 1)
                || (e->button == 2 && mouse_middle != 0)
                || (e->button == 3 && mouse_right != 0)
                || (e->button == 4 && mouse_scroll_up != 0)
                || (e->button == 5 && mouse_scroll_down != 0));
    }

    LauncherIcon* icon = click_launcher_icon(panel, e->x, e->y);

    if (icon) {
        return (e->button == 1);
    }

    // no launcher/task clicked --> check if taskbar clicked
    Taskbar* tskbar = click_taskbar(panel, e->x, e->y);

    if (tskbar && e->button == 1 && panel_mode == MULTI_DESKTOP) {
        return 1;
    }

    if (click_clock(panel, e->x, e->y)) {
        bool clock_lclick = (e->button == 1 && !clock_lclick_command.empty());
        bool clock_rclick = (e->button == 3 && !clock_rclick_command.empty());
        return (clock_lclick || clock_rclick);
    }

    return false;
}


void forward_click(XEvent* e) {
    // forward the click to the desktop window (thanks conky)
    XUngrabPointer(server.dsp, e->xbutton.time);
    e->xbutton.window = server.root_win;
    // icewm doesn't open under the mouse.
    // and xfce doesn't open at all.
    e->xbutton.x = e->xbutton.x_root;
    e->xbutton.y = e->xbutton.y_root;
    //printf("**** %d, %d\n", e->xbutton.x, e->xbutton.y);
    //XSetInputFocus(server.dsp, e->xbutton.window, RevertToParent, e->xbutton.time);
    XSendEvent(server.dsp, e->xbutton.window, False, ButtonPressMask, e);
}


void event_button_press(XEvent* e) {
    Panel* panel = get_panel(e->xany.window);

    if (!panel) {
        return;
    }


    if (wm_menu && !tint3_handles_click(panel, &e->xbutton)) {
        forward_click(e);
        return;
    }

    task_drag = click_task(panel, e->xbutton.x, e->xbutton.y);

    if (panel_layer == BOTTOM_LAYER) {
        XLowerWindow(server.dsp, panel->main_win);
    }
}

void event_button_motion_notify(XEvent* e) {
    Panel* panel = get_panel(e->xany.window);

    if (!panel || !task_drag) {
        return;
    }

    // Find the taskbar on the event's location
    Taskbar* event_taskbar = click_taskbar(panel, e->xbutton.x, e->xbutton.y);

    if (event_taskbar == nullptr) {
        return;
    }

    // Find the task on the event's location
    Task* event_task = click_task(panel, e->xbutton.x, e->xbutton.y);

    // If the event takes place on the same taskbar as the task being dragged
    if (event_taskbar == (Taskbar*)task_drag->parent) {
        // Swap the task_drag with the task on the event's location (if they differ)
        if (event_task && event_task != task_drag) {
            auto& children = event_taskbar->children;
            auto drag_iter = std::find(children.begin(), children.end(), task_drag);
            auto task_iter = std::find(children.begin(), children.end(), event_task);

            if (drag_iter != children.end() && task_iter != children.end()) {
                std::iter_swap(drag_iter, task_iter);
                event_taskbar->resize = 1;
                panel_refresh = 1;
                task_dragged = 1;
            }
        }
    } else { // The event is on another taskbar than the task being dragged
        if (task_drag->desktop == ALLDESKTOP || panel_mode != MULTI_DESKTOP) {
            return;
        }

        auto drag_taskbar = reinterpret_cast<Taskbar*>(task_drag->parent);
        auto drag_taskbar_iter = std::find(
                                     drag_taskbar->children.begin(),
                                     drag_taskbar->children.end(),
                                     task_drag);

        if (drag_taskbar_iter != drag_taskbar->children.end()) {
            drag_taskbar->children.erase(drag_taskbar_iter);
        }

        if (event_taskbar->posx > drag_taskbar->posx
            || event_taskbar->posy > drag_taskbar->posy) {
            auto& children = event_taskbar->children;
            size_t i = (taskbarname_enabled) ? 1 : 0;
            children.insert(children.begin() + i, task_drag);
        } else {
            event_taskbar->children.push_back(task_drag);
        }

        // Move task to other desktop (but avoid the 'Window desktop changed' code in 'event_property_notify')
        task_drag->parent = event_taskbar;
        task_drag->desktop = event_taskbar->desktop;

        windows_set_desktop(task_drag->win, event_taskbar->desktop);

        event_taskbar->resize = 1;
        drag_taskbar->resize = 1;
        task_dragged = 1;
        panel_refresh = 1;
    }
}

void event_button_release(XEvent* e) {
    Panel* panel = get_panel(e->xany.window);

    if (!panel) {
        return;
    }

    if (wm_menu && !tint3_handles_click(panel, &e->xbutton)) {
        forward_click(e);

        if (panel_layer == BOTTOM_LAYER) {
            XLowerWindow(server.dsp, panel->main_win);
        }

        task_drag = 0;
        return;
    }

    int action = TOGGLE_ICONIFY;

    switch (e->xbutton.button) {
        case 2:
            action = mouse_middle;
            break;

        case 3:
            action = mouse_right;
            break;

        case 4:
            action = mouse_scroll_up;
            break;

        case 5:
            action = mouse_scroll_down;
            break;

        case 6:
            action = mouse_tilt_left;
            break;

        case 7:
            action = mouse_tilt_right;
            break;
    }

    if (click_clock(panel, e->xbutton.x, e->xbutton.y)) {
        clock_action(e->xbutton.button);

        if (panel_layer == BOTTOM_LAYER) {
            XLowerWindow(server.dsp, panel->main_win);
        }

        task_drag = 0;
        return;
    }

    if (click_launcher(panel, e->xbutton.x, e->xbutton.y)) {
        LauncherIcon* icon = click_launcher_icon(panel, e->xbutton.x, e->xbutton.y);

        if (icon) {
            launcher_action(icon, e);
        }

        task_drag = 0;
        return;
    }

    Taskbar* tskbar;

    if (!(tskbar = click_taskbar(panel, e->xbutton.x, e->xbutton.y))) {
        // TODO: check better solution to keep window below
        if (panel_layer == BOTTOM_LAYER) {
            XLowerWindow(server.dsp, panel->main_win);
        }

        task_drag = 0;
        return;
    }

    // drag and drop task
    if (task_dragged) {
        task_drag = 0;
        task_dragged = 0;
        return;
    }

    // switch desktop
    if (panel_mode == MULTI_DESKTOP) {
        if (tskbar->desktop != server.desktop && action != CLOSE
            && action != DESKTOP_LEFT && action != DESKTOP_RIGHT) {
            set_desktop(tskbar->desktop);
        }
    }

    // action on task
    window_action(click_task(panel, e->xbutton.x, e->xbutton.y), action);

    // to keep window below
    if (panel_layer == BOTTOM_LAYER) {
        XLowerWindow(server.dsp, panel->main_win);
    }
}


void event_property_notify(XEvent* e) {
    int i;
    Task* tsk;
    Window win = e->xproperty.window;
    Atom at = e->xproperty.atom;

    if (xsettings_client) {
        xsettings_client_process_event(xsettings_client, e);
    }

    if (win == server.root_win) {
        if (!server.got_root_win) {
            XSelectInput(server.dsp, server.root_win,
                         PropertyChangeMask | StructureNotifyMask);
            server.got_root_win = 1;
        }
        // Change name of desktops
        else if (at == server.atom._NET_DESKTOP_NAMES) {
            if (!taskbarname_enabled) {
                return;
            }

            auto desktop_names = server_get_desktop_names();
            auto it = desktop_names.begin();

            for (i = 0; i < nb_panel; ++i) {
                for (int j = 0; j < panel1[i].nb_desktop; ++j) {
                    std::string name;

                    if (it != desktop_names.end()) {
                        name = (*it++);
                    } else {
                        name = StringRepresentation(j + 1);
                    }

                    Taskbar* tskbar = &panel1[i].taskbar[j];

                    if (tskbar->bar_name.name() != name) {
                        tskbar->bar_name.set_name(name);
                        tskbar->bar_name.resize = 1;
                    }
                }
            }

            panel_refresh = 1;
        }
        // Change number of desktops
        else if (at == server.atom._NET_NUMBER_OF_DESKTOPS) {
            if (!taskbar_enabled) {
                return;
            }

            server.nb_desktop = server_get_number_of_desktop();

            if (server.nb_desktop <= server.desktop) {
                server.desktop = server.nb_desktop - 1;
            }

            cleanup_taskbar();
            init_taskbar();

            for (i = 0 ; i < nb_panel ; i++) {
                init_taskbar_panel(&panel1[i]);
                set_panel_items_order(&panel1[i]);
                visible_taskbar(&panel1[i]);
                panel1[i].resize = 1;
            }

            task_refresh_tasklist();
            active_task();
            panel_refresh = 1;
        }
        // Change desktop
        else if (at == server.atom._NET_CURRENT_DESKTOP) {
            if (!taskbar_enabled) {
                return;
            }

            int old_desktop = server.desktop;
            server.desktop = server_get_current_desktop();

            for (i = 0 ; i < nb_panel ; i++) {
                Panel* panel = &panel1[i];
                panel->taskbar[old_desktop].set_state(TASKBAR_NORMAL);
                panel->taskbar[server.desktop].set_state(TASKBAR_ACTIVE);
                // check ALLDESKTOP task => resize taskbar
                Taskbar* tskbar;

                if (server.nb_desktop > old_desktop) {
                    tskbar = &panel->taskbar[old_desktop];
                    auto it = tskbar->children.begin();

                    if (taskbarname_enabled) {
                        ++it;
                    }

                    for (; it != tskbar->children.end(); ++it) {
                        auto tsk = static_cast<Task*>(*it);

                        if (tsk->desktop == ALLDESKTOP) {
                            tsk->on_screen = 0;
                            tskbar->resize = 1;
                            panel_refresh = 1;
                        }
                    }
                }

                tskbar = &panel->taskbar[server.desktop];
                auto it = tskbar->children.begin();

                if (taskbarname_enabled) {
                    ++it;
                }

                for (; it != tskbar->children.end(); ++it) {
                    auto tsk = static_cast<Task*>(*it);

                    if (tsk->desktop == ALLDESKTOP) {
                        tsk->on_screen = 1;
                        tskbar->resize = 1;
                    }
                }
            }
        }
        // Window list
        else if (at == server.atom._NET_CLIENT_LIST) {
            task_refresh_tasklist();
            panel_refresh = 1;
        }
        // Change active
        else if (at == server.atom._NET_ACTIVE_WINDOW) {
            active_task();
            panel_refresh = 1;
        } else if (at == server.atom._XROOTPMAP_ID || at == server.atom._XROOTMAP_ID) {
            // change Wallpaper
            for (i = 0 ; i < nb_panel ; i++) {
                set_panel_background(&panel1[i]);
            }

            panel_refresh = 1;
        }
    } else {
        tsk = task_get_task(win);

        if (!tsk) {
            if (at != server.atom._NET_WM_STATE) {
                return;
            } else {
                // xfce4 sends _NET_WM_STATE after minimized to tray, so we need to check if window is mapped
                // if it is mapped and not set as skip_taskbar, we must add it to our task list
                XWindowAttributes wa;
                XGetWindowAttributes(server.dsp, win, &wa);

                if (wa.map_state == IsViewable && !window_is_skip_taskbar(win)) {
                    if ((tsk = add_task(win))) {
                        panel_refresh = 1;
                    } else {
                        return;
                    }
                } else {
                    return;
                }
            }
        }

        //printf("atom root_win = %s, %s\n", XGetAtomName(server.dsp, at), tsk->title);

        // Window title changed
        if (at == server.atom._NET_WM_VISIBLE_NAME || at == server.atom._NET_WM_NAME
            || at == server.atom.WM_NAME) {
            if (get_title(tsk)) {
                if (g_tooltip.mapped && (g_tooltip.area == (Area*)tsk)) {
                    tooltip_copy_text((Area*)tsk);
                    tooltip_update();
                }

                panel_refresh = 1;
            }
        }
        // Demand attention
        else if (at == server.atom._NET_WM_STATE) {
            if (window_is_urgent(win)) {
                add_urgent(tsk);
            }

            if (window_is_skip_taskbar(win)) {
                remove_task(tsk);
                panel_refresh = 1;
            }
        } else if (at == server.atom.WM_STATE) {
            // Iconic state
            int state = (task_active
                         && tsk->win == task_active->win ? TASK_ACTIVE : TASK_NORMAL);

            if (window_is_iconified(win)) {
                state = TASK_ICONIFIED;
            }

            set_task_state(tsk, state);
            panel_refresh = 1;
        }
        // Window icon changed
        else if (at == server.atom._NET_WM_ICON) {
            get_icon(tsk);
            panel_refresh = 1;
        }
        // Window desktop changed
        else if (at == server.atom._NET_WM_DESKTOP) {
            int desktop = window_get_desktop(win);

            //printf("  Window desktop changed %d, %d\n", tsk->desktop, desktop);
            // bug in windowmaker : send unecessary 'desktop changed' when focus changed
            if (desktop != tsk->desktop) {
                remove_task(tsk);
                tsk = add_task(win);
                active_task();
                panel_refresh = 1;
            }
        } else if (at == server.atom.WM_HINTS) {
            XWMHints* wmhints = XGetWMHints(server.dsp, win);

            if (wmhints && wmhints->flags & XUrgencyHint) {
                add_urgent(tsk);
            }

            XFree(wmhints);
        }

        if (!server.got_root_win) {
            server.root_win = RootWindow(server.dsp, server.screen);
        }
    }
}


void event_expose(XEvent* e) {
    Panel* panel;
    panel = get_panel(e->xany.window);

    if (!panel) {
        return;
    }

    // TODO : one panel_refresh per panel ?
    panel_refresh = 1;
}


void event_configure_notify(Window win) {
    // change in root window (xrandr)
    if (win == server.root_win) {
        signal_pending = SIGUSR1;
        return;
    }

    // 'win' is a trayer icon
    TrayWindow* traywin;
    GSList* l;

    for (l = systray.list_icons; l ; l = l->next) {
        traywin = (TrayWindow*)l->data;

        if (traywin->tray_id == win) {
            //printf("move tray %d\n", traywin->x);
            XMoveResizeWindow(server.dsp, traywin->id, traywin->x, traywin->y,
                              traywin->width, traywin->height);
            XResizeWindow(server.dsp, traywin->tray_id, traywin->width, traywin->height);
            panel_refresh = 1;
            return;
        }
    }

    // 'win' move in another monitor
    if (nb_panel == 1) {
        return;
    }

    Task* tsk = task_get_task(win);

    if (!tsk) {
        return;
    }

    Panel* p = tsk->panel;

    if (p->monitor != window_get_monitor(win)) {
        remove_task(tsk);
        tsk = add_task(win);

        if (win == window_get_active()) {
            set_task_state(tsk, TASK_ACTIVE);
            task_active = tsk;
        }

        panel_refresh = 1;
    }
}

char const* GetAtomName(Display* disp, Atom a) {
    if (a == None) {
        return "None";
    }

    return XGetAtomName(disp, a);
}

typedef struct Property {
    unsigned char* data;
    int format, nitems;
    Atom type;
} Property;

//This fetches all the data from a property
struct Property read_property(Display* disp, Window w, Atom property) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* ret = 0;

    int read_bytes = 1024;

    //Keep trying to read the property until there are no
    //bytes unread.
    do {
        if (ret != 0) {
            XFree(ret);
        }

        XGetWindowProperty(disp, w, property, 0, read_bytes, False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after,
                           &ret);
        read_bytes *= 2;
    } while (bytes_after != 0);

    fprintf(stderr, "DnD %s:%d: Property:\n", __FILE__, __LINE__);
    fprintf(stderr, "DnD %s:%d: Actual type: %s\n", __FILE__, __LINE__,
            GetAtomName(disp, actual_type));
    fprintf(stderr, "DnD %s:%d: Actual format: %d\n", __FILE__, __LINE__,
            actual_format);
    fprintf(stderr, "DnD %s:%d: Number of items: %lu\n", __FILE__, __LINE__,
            nitems);

    Property p;
    p.data = ret;
    p.format = actual_format;
    p.nitems = nitems;
    p.type = actual_type;

    return p;
}

// This function takes a list of targets which can be converted to (atom_list, nitems)
// and a list of acceptable targets with prioritees (datatypes). It returns the highest
// entry in datatypes which is also in atom_list: ie it finds the best match.
Atom pick_target_from_list(Display* disp, Atom* atom_list, int nitems) {
    Atom to_be_requested = None;
    int i;

    for (i = 0; i < nitems; i++) {
        char const* atom_name = GetAtomName(disp, atom_list[i]);
        fprintf(stderr, "DnD %s:%d: Type %d = %s\n", __FILE__, __LINE__, i, atom_name);

        //See if this data type is allowed and of higher priority (closer to zero)
        //than the present one.
        if (strcmp(atom_name, "STRING") == 0) {
            to_be_requested = atom_list[i];
        }
    }

    return to_be_requested;
}

// Finds the best target given up to three atoms provided (any can be None).
// Useful for part of the Xdnd protocol.
Atom pick_target_from_atoms(Display* disp, Atom t1, Atom t2, Atom t3) {
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

    return pick_target_from_list(disp, atoms, n);
}

// Finds the best target given a local copy of a property.
Atom pick_target_from_targets(Display* disp, Property p) {
    //The list of targets is a list of atoms, so it should have type XA_ATOM
    //but it may have the type TARGETS instead.

    if ((p.type != XA_ATOM && p.type != server.atom.TARGETS) || p.format != 32) {
        //This would be really broken. Targets have to be an atom list
        //and applications should support this. Nevertheless, some
        //seem broken (MATLAB 7, for instance), so ask for STRING
        //next instead as the lowest common denominator
        return XA_STRING;
    } else {
        Atom* atom_list = (Atom*)p.data;

        return pick_target_from_list(disp, atom_list, p.nitems);
    }
}

void dnd_enter(XClientMessageEvent* e) {
    dnd_atom = None;
    int more_than_3 = e->data.l[1] & 1;
    dnd_source_window = e->data.l[0];
    dnd_version = (e->data.l[1] >> 24);

    fprintf(stderr, "DnD %s:%d: DnDEnter\n", __FILE__, __LINE__);
    fprintf(stderr, "DnD %s:%d: DnDEnter. Supports > 3 types = %s\n", __FILE__,
            __LINE__, more_than_3 ? "yes" : "no");
    fprintf(stderr, "DnD %s:%d: Protocol version = %d\n", __FILE__, __LINE__,
            dnd_version);
    fprintf(stderr, "DnD %s:%d: Type 1 = %s\n", __FILE__, __LINE__,
            GetAtomName(server.dsp, e->data.l[2]));
    fprintf(stderr, "DnD %s:%d: Type 2 = %s\n", __FILE__, __LINE__,
            GetAtomName(server.dsp, e->data.l[3]));
    fprintf(stderr, "DnD %s:%d: Type 3 = %s\n", __FILE__, __LINE__,
            GetAtomName(server.dsp, e->data.l[4]));

    //Query which conversions are available and pick the best

    if (more_than_3) {
        //Fetch the list of possible conversions
        //Notice the similarity to TARGETS with paste.
        Property p = read_property(server.dsp, dnd_source_window,
                                   server.atom.XdndTypeList);
        dnd_atom = pick_target_from_targets(server.dsp, p);
        XFree(p.data);
    } else {
        //Use the available list
        dnd_atom = pick_target_from_atoms(server.dsp, e->data.l[2], e->data.l[3],
                                          e->data.l[4]);
    }

    fprintf(stderr, "DnD %s:%d: Requested type = %s\n", __FILE__, __LINE__,
            GetAtomName(server.dsp, dnd_atom));
}

void dnd_position(XClientMessageEvent* e) {
    dnd_target_window = e->window;
    int accept = 0;
    Panel* panel = get_panel(e->window);
    int x, y, mapX, mapY;
    Window child;
    x = (e->data.l[2] >> 16) & 0xFFFF;
    y = e->data.l[2] & 0xFFFF;
    XTranslateCoordinates(server.dsp, server.root_win, e->window, x, y, &mapX,
                          &mapY, &child);
    Task* task = click_task(panel, mapX, mapY);

    if (task) {
        if (task->desktop != server.desktop) {
            set_desktop(task->desktop);
        }

        window_action(task, TOGGLE);
    } else {
        LauncherIcon* icon = click_launcher_icon(panel, mapX, mapY);

        if (icon) {
            accept = 1;
            dnd_launcher_exec = icon->cmd;
        } else {
            dnd_launcher_exec = 0;
        }
    }

    // send XdndStatus event to get more XdndPosition events
    XClientMessageEvent se;
    se.type = ClientMessage;
    se.window = e->data.l[0];
    se.message_type = server.atom.XdndStatus;
    se.format = 32;
    se.data.l[0] = e->window;  // XID of the target window
    se.data.l[1] = accept ? 1 :
                   0;          // bit 0: accept drop    bit 1: send XdndPosition events if inside rectangle
    se.data.l[2] =
        0;          // Rectangle x,y for which no more XdndPosition events
    se.data.l[3] = (1 << 16) |
                   1;  // Rectangle w,h for which no more XdndPosition events

    if (accept) {
        se.data.l[4] = dnd_version >= 2 ? e->data.l[4] : server.atom.XdndActionCopy;
    } else {
        se.data.l[4] = None;       // None = drop will not be accepted
    }

    XSendEvent(server.dsp, e->data.l[0], False, NoEventMask, (XEvent*)&se);
}

void dnd_drop(XClientMessageEvent* e) {
    if (dnd_target_window && dnd_launcher_exec) {
        if (dnd_version >= 1) {
            XConvertSelection(server.dsp, server.atom.XdndSelection, XA_STRING,
                              dnd_selection, dnd_target_window, e->data.l[2]);
        } else {
            XConvertSelection(server.dsp, server.atom.XdndSelection, XA_STRING,
                              dnd_selection, dnd_target_window, CurrentTime);
        }
    } else {
        //The source is sending anyway, despite instructions to the contrary.
        //So reply that we're not interested.
        XClientMessageEvent m;
        memset(&m, sizeof(m), 0);
        m.type = ClientMessage;
        m.display = e->display;
        m.window = e->data.l[0];
        m.message_type = server.atom.XdndFinished;
        m.format = 32;
        m.data.l[0] = dnd_target_window;
        m.data.l[1] = 0;
        m.data.l[2] = None; //Failed.
        XSendEvent(server.dsp, e->data.l[0], False, NoEventMask, (XEvent*)&m);
    }
}

int main(int argc, char* argv[]) {
    int hidden_dnd = 0;
    GSList* it;

start:
    init(argc, argv);
    init_X11();

    int i;

    if (!config_path.empty()) {
        i = config_read_file(config_path.c_str());
    } else {
        i = config_read();
    }

    if (!i) {
        fprintf(stderr, "usage: tint3 [-c] <config_file>\n");
        cleanup();
        exit(1);
    }

    init_panel();

    if (!snapshot_path.empty()) {
        get_snapshot(snapshot_path.c_str());
        cleanup();
        exit(0);
    }

    int damage_event, damage_error;
    XDamageQueryExtension(server.dsp, &damage_event, &damage_error);
    int x11_fd = ConnectionNumber(server.dsp);
    XSync(server.dsp, False);

    // XDND initialization
    dnd_source_window = 0;
    dnd_target_window = 0;
    dnd_version = 0;
    dnd_selection = XInternAtom(server.dsp, "PRIMARY", 0);
    dnd_atom = None;
    dnd_sent_request = 0;
    dnd_launcher_exec = 0;

    //  sigset_t empty_mask;
    //  sigemptyset(&empty_mask);

    while (true) {
        Panel* panel = nullptr;

        if (panel_refresh) {
            panel_refresh = 0;

            for (i = 0 ; i < nb_panel ; i++) {
                panel = &panel1[i];

                if (panel->is_hidden) {
                    XCopyArea(server.dsp, panel->hidden_pixmap, panel->main_win, server.gc, 0, 0,
                              panel->hidden_width, panel->hidden_height, 0, 0);
                    XSetWindowBackgroundPixmap(server.dsp, panel->main_win, panel->hidden_pixmap);
                } else {
                    if (panel->temp_pmap) {
                        XFreePixmap(server.dsp, panel->temp_pmap);
                    }

                    panel->temp_pmap = XCreatePixmap(server.dsp, server.root_win, panel->width,
                                                     panel->height, server.depth);
                    panel->render();
                    XCopyArea(server.dsp, panel->temp_pmap, panel->main_win, server.gc, 0, 0,
                              panel->width, panel->height, 0, 0);
                }
            }

            XFlush(server.dsp);

            panel = systray.panel;

            if (refresh_systray && panel && !panel->is_hidden) {
                refresh_systray = 0;
                // tint3 doen't draw systray icons. it just redraw background.
                XSetWindowBackgroundPixmap(server.dsp, panel->main_win, panel->temp_pmap);
                // force icon's refresh
                refresh_systray_icon();
            }
        }

        // thanks to AngryLlama for the timer
        // Create a File Description Set containing x11_fd
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(x11_fd, &fdset);
        update_next_timeout();

        struct timeval* timeout = nullptr;

        if (next_timeout.tv_sec >= 0 && next_timeout.tv_usec >= 0) {
            timeout = &next_timeout;
        }

        // Wait for X Event or a Timer
        if (select(x11_fd + 1, &fdset, 0, 0, timeout) > 0) {
            while (XPending(server.dsp)) {
                XEvent e;
                XNextEvent(server.dsp, &e);
#if HAVE_SN
                sn_display_process_event(server.sn_dsp, &e);
#endif // HAVE_SN

                panel = get_panel(e.xany.window);

                if (panel && panel_autohide) {
                    if (e.type == EnterNotify) {
                        autohide_trigger_show(panel);
                    } else if (e.type == LeaveNotify) {
                        autohide_trigger_hide(panel);
                    }

                    if (panel->is_hidden) {
                        if (e.type == ClientMessage
                            && e.xclient.message_type == server.atom.XdndPosition) {
                            hidden_dnd = 1;
                            autohide_show(panel);
                        } else {
                            continue;    // discard further processing of this event because the panel is not visible yet
                        }
                    } else if (hidden_dnd && e.type == ClientMessage
                               && e.xclient.message_type == server.atom.XdndLeave) {
                        hidden_dnd = 0;
                        autohide_hide(panel);
                    }
                }

                XClientMessageEvent* ev;

                switch (e.type) {
                    case ButtonPress:
                        tooltip_hide(0);
                        event_button_press(&e);
                        break;

                    case ButtonRelease:
                        event_button_release(&e);
                        break;

                    case MotionNotify: {
                            unsigned int button_mask = Button1Mask | Button2Mask | Button3Mask | Button4Mask
                                                       | Button5Mask;

                            if (e.xmotion.state & button_mask) {
                                event_button_motion_notify(&e);
                            }

                            Panel* panel = get_panel(e.xmotion.window);
                            Area* area = click_area(panel, e.xmotion.x, e.xmotion.y);

                            if (area->_get_tooltip_text) {
                                tooltip_trigger_show(area, panel, &e);
                            } else {
                                tooltip_trigger_hide();
                            }

                            break;
                        }

                    case LeaveNotify:
                        tooltip_trigger_hide();
                        break;

                    case Expose:
                        event_expose(&e);
                        break;

                    case PropertyNotify:
                        event_property_notify(&e);
                        break;

                    case ConfigureNotify:
                        event_configure_notify(e.xconfigure.window);
                        break;

                    case ReparentNotify:
                        if (!systray_enabled) {
                            break;
                        }

                        panel = systray.panel;

                        if (e.xany.window == panel->main_win) { // reparented to us
                            break;
                        }

                        // FIXME: 'reparent to us' badly detected => disabled
                        break;

                    case UnmapNotify:
                    case DestroyNotify:
                        if (e.xany.window == server.composite_manager) {
                            // Stop real_transparency
                            signal_pending = SIGUSR1;
                            break;
                        }

                        if (e.xany.window == g_tooltip.window || !systray_enabled) {
                            break;
                        }

                        for (it = systray.list_icons; it; it = g_slist_next(it)) {
                            if (((TrayWindow*)it->data)->tray_id == e.xany.window) {
                                remove_icon((TrayWindow*)it->data);
                                break;
                            }
                        }

                        break;

                    case ClientMessage:
                        ev = &e.xclient;

                        if (ev->data.l[1] == server.atom._NET_WM_CM_S0) {
                            if (ev->data.l[2] == None)
                                // Stop real_transparency
                            {
                                signal_pending = SIGUSR1;
                            } else
                                // Start real_transparency
                            {
                                signal_pending = SIGUSR1;
                            }
                        }

                        if (systray_enabled
                            && e.xclient.message_type == server.atom._NET_SYSTEM_TRAY_OPCODE
                            && e.xclient.format == 32 && e.xclient.window == net_sel_win) {
                            net_message(&e.xclient);
                        } else if (e.xclient.message_type == server.atom.XdndEnter) {
                            dnd_enter(&e.xclient);
                        } else if (e.xclient.message_type == server.atom.XdndPosition) {
                            dnd_position(&e.xclient);
                        } else if (e.xclient.message_type == server.atom.XdndDrop) {
                            dnd_drop(&e.xclient);
                        }

                        break;

                    case SelectionNotify: {
                            Atom target = e.xselection.target;

                            fprintf(stderr, "DnD %s:%d: A selection notify has arrived!\n", __FILE__,
                                    __LINE__);
                            fprintf(stderr, "DnD %s:%d: Requestor = %lu\n", __FILE__, __LINE__,
                                    e.xselectionrequest.requestor);
                            fprintf(stderr, "DnD %s:%d: Selection atom = %s\n", __FILE__, __LINE__,
                                    GetAtomName(server.dsp, e.xselection.selection));
                            fprintf(stderr, "DnD %s:%d: Target atom    = %s\n", __FILE__, __LINE__,
                                    GetAtomName(server.dsp, target));
                            fprintf(stderr, "DnD %s:%d: Property atom  = %s\n", __FILE__, __LINE__,
                                    GetAtomName(server.dsp, e.xselection.property));

                            if (e.xselection.property != None && dnd_launcher_exec) {
                                Property prop = read_property(server.dsp, dnd_target_window, dnd_selection);

                                //If we're being given a list of targets (possible conversions)
                                if (target == server.atom.TARGETS && !dnd_sent_request) {
                                    dnd_sent_request = 1;
                                    dnd_atom = pick_target_from_targets(server.dsp, prop);

                                    if (dnd_atom == None) {
                                        fprintf(stderr, "No matching datatypes.\n");
                                    } else {
                                        //Request the data type we are able to select
                                        fprintf(stderr, "Now requsting type %s", GetAtomName(server.dsp, dnd_atom));
                                        XConvertSelection(server.dsp, dnd_selection, dnd_atom, dnd_selection,
                                                          dnd_target_window, CurrentTime);
                                    }
                                } else if (target == dnd_atom) {
                                    //Dump the binary data
                                    fprintf(stderr, "DnD %s:%d: Data begins:\n", __FILE__, __LINE__);
                                    fprintf(stderr, "--------\n");
                                    int i;

                                    for (i = 0; i < prop.nitems * prop.format / 8; i++) {
                                        fprintf(stderr, "%c", ((char*)prop.data)[i]);
                                    }

                                    fprintf(stderr, "--------\n");

                                    int cmd_length = 0;
                                    cmd_length += 1; // (
                                    cmd_length += strlen(dnd_launcher_exec) + 1; // exec + space
                                    cmd_length += 1; // open double quotes

                                    for (i = 0; i < prop.nitems * prop.format / 8; i++) {
                                        char c = ((char*)prop.data)[i];

                                        if (c == '\n') {
                                            if (i < prop.nitems * prop.format / 8 - 1) {
                                                cmd_length += 3; // close double quotes, space, open double quotes
                                            }
                                        } else if (c == '\r') {
                                        } else {
                                            cmd_length += 1; // 1 character

                                            if (c == '`' || c == '$' || c == '\\') {
                                                cmd_length += 1; // escape with one backslash
                                            }
                                        }
                                    }

                                    cmd_length += 1; // close double quotes
                                    cmd_length += 2; // &)
                                    cmd_length += 1; // terminator

                                    char* cmd = (char*) malloc(cmd_length);
                                    cmd[0] = '\0';
                                    strcat(cmd, "(");
                                    strcat(cmd, dnd_launcher_exec);
                                    strcat(cmd, " \"");

                                    for (i = 0; i < prop.nitems * prop.format / 8; i++) {
                                        char c = ((char*)prop.data)[i];

                                        if (c == '\n') {
                                            if (i < prop.nitems * prop.format / 8 - 1) {
                                                strcat(cmd, "\" \"");
                                            }
                                        } else if (c == '\r') {
                                        } else {
                                            if (c == '`' || c == '$' || c == '\\') {
                                                strcat(cmd, "\\");
                                            }

                                            char sc[2];
                                            sc[0] = c;
                                            sc[1] = '\0';
                                            strcat(cmd, sc);
                                        }
                                    }

                                    strcat(cmd, "\"");
                                    strcat(cmd, "&)");
                                    fprintf(stderr, "DnD %s:%d: Running command: %s\n", __FILE__, __LINE__, cmd);
                                    tint_exec(cmd);
                                    free(cmd);

                                    // Reply OK.
                                    XClientMessageEvent m;
                                    memset(&m, sizeof(m), 0);
                                    m.type = ClientMessage;
                                    m.display = server.dsp;
                                    m.window = dnd_source_window;
                                    m.message_type = server.atom.XdndFinished;
                                    m.format = 32;
                                    m.data.l[0] = dnd_target_window;
                                    m.data.l[1] = 1;
                                    m.data.l[2] = server.atom.XdndActionCopy; //We only ever copy.
                                    XSendEvent(server.dsp, dnd_source_window, False, NoEventMask, (XEvent*)&m);
                                    XSync(server.dsp, False);
                                }

                                XFree(prop.data);
                            }

                            break;
                        }

                    default:
                        if (e.type == XDamageNotify + damage_event) {
                            // union needed to avoid strict-aliasing warnings by gcc
                            union {
                                XEvent e;
                                XDamageNotifyEvent de;
                            } event_union = {.e = e};
                            TrayWindow* traywin;
                            GSList* l;
                            XDamageNotifyEvent* de = &event_union.de;

                            for (l = systray.list_icons; l ; l = l->next) {
                                traywin = (TrayWindow*)l->data;

                                if (traywin->id == de->drawable) {
                                    systray_render_icon(traywin);
                                    break;
                                }
                            }
                        }
                }
            }
        }

        callback_timeout_expired();

        if (signal_pending) {
            cleanup();

            if (signal_pending == SIGUSR1) {
                // restart tint3
                // SIGUSR1 used when : user's signal, composite manager stop/start or xrandr
                FD_CLR(x11_fd, &fdset);  // not sure if needed
                goto start;
            } else {
                // SIGINT, SIGTERM, SIGHUP
                return 0;
            }
        }
    }
}


