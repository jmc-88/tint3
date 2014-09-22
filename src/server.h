/**************************************************************************
* server :
* -
*
* Check COPYING file for Copyright
*
**************************************************************************/

#ifndef SERVER_H
#define SERVER_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>

#include <vector>
#include <map>

#ifdef HAVE_SN
#include <libsn/sn.h>
#include <glib.h>
#endif


struct Global_atom {
    Atom _XROOTPMAP_ID;
    Atom _XROOTMAP_ID;
    Atom _NET_CURRENT_DESKTOP;
    Atom _NET_NUMBER_OF_DESKTOPS;
    Atom _NET_DESKTOP_NAMES;
    Atom _NET_DESKTOP_GEOMETRY;
    Atom _NET_DESKTOP_VIEWPORT;
    Atom _NET_ACTIVE_WINDOW;
    Atom _NET_WM_WINDOW_TYPE;
    Atom _NET_WM_STATE_SKIP_PAGER;
    Atom _NET_WM_STATE_SKIP_TASKBAR;
    Atom _NET_WM_STATE_STICKY;
    Atom _NET_WM_STATE_DEMANDS_ATTENTION;
    Atom _NET_WM_WINDOW_TYPE_DOCK;
    Atom _NET_WM_WINDOW_TYPE_DESKTOP;
    Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
    Atom _NET_WM_WINDOW_TYPE_MENU;
    Atom _NET_WM_WINDOW_TYPE_SPLASH;
    Atom _NET_WM_WINDOW_TYPE_DIALOG;
    Atom _NET_WM_WINDOW_TYPE_NORMAL;
    Atom _NET_WM_DESKTOP;
    Atom WM_STATE;
    Atom _NET_WM_STATE;
    Atom _NET_WM_STATE_MAXIMIZED_VERT;
    Atom _NET_WM_STATE_MAXIMIZED_HORZ;
    Atom _NET_WM_STATE_SHADED;
    Atom _NET_WM_STATE_HIDDEN;
    Atom _NET_WM_STATE_BELOW;
    Atom _NET_WM_STATE_ABOVE;
    Atom _NET_WM_STATE_MODAL;
    Atom _NET_CLIENT_LIST;
    Atom _NET_WM_NAME;
    Atom _NET_WM_VISIBLE_NAME;
    Atom _NET_WM_STRUT;
    Atom _NET_WM_ICON;
    Atom _NET_WM_ICON_GEOMETRY;
    Atom _NET_CLOSE_WINDOW;
    Atom UTF8_STRING;
    Atom _NET_SUPPORTING_WM_CHECK;
    Atom _NET_WM_CM_S0;
    Atom _NET_WM_STRUT_PARTIAL;
    Atom WM_NAME;
    Atom __SWM_VROOT;
    Atom _MOTIF_WM_HINTS;
    Atom WM_HINTS;
    Atom _NET_SYSTEM_TRAY_SCREEN;
    Atom _NET_SYSTEM_TRAY_OPCODE;
    Atom MANAGER;
    Atom _NET_SYSTEM_TRAY_MESSAGE_DATA;
    Atom _NET_SYSTEM_TRAY_ORIENTATION;
    Atom _XEMBED;
    Atom _XEMBED_INFO;
    Atom _XSETTINGS_SCREEN;
    Atom _XSETTINGS_SETTINGS;
    Atom XdndAware;
    Atom XdndEnter;
    Atom XdndPosition;
    Atom XdndStatus;
    Atom XdndDrop;
    Atom XdndLeave;
    Atom XdndSelection;
    Atom XdndTypeList;
    Atom XdndActionCopy;
    Atom XdndFinished;
    Atom TARGETS;
};

struct Monitor {
    int x;
    int y;
    int width;
    int height;
    char** names;
};

class Server_global {
  public:
    Display* dsp;
    Window root_win;
    Window composite_manager;
    int real_transparency;
    // current desktop
    int desktop;
    int screen;
    int depth;
    int nb_desktop;
    // number of monitor (without monitor included into another one)
    int nb_monitor;
    std::vector<Monitor> monitor;
    int got_root_win;
    Visual* visual;
    Visual* visual32;
    // root background
    Pixmap root_pmap;
    GC gc;
    Colormap colormap;
    Colormap colormap32;
    Global_atom atom;
#ifdef HAVE_SN
    SnDisplay* sn_dsp;
    std::map<pid_t, SnLauncherContext*> pids;
#endif // HAVE_SN

    void Cleanup();
    int GetCurrentDesktop();
    int GetDesktop();
    int GetDesktopFromWindow(Window win);
    void InitAtoms();
    void InitVisual();
};


extern Server_global server;

void SendEvent32(Window win, Atom at, long data1, long data2, long data3);
int  GetProperty32(Window win, Atom at, Atom type);
void* ServerGetProperty(Window win, Atom at, Atom type, int* num_results);
int ServerCatchError(Display* d, XErrorEvent* ev);

// detect root background
void GetRootPixmap();

// detect monitors and desktops
void GetMonitors();
void GetDesktops();


#endif
