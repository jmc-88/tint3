/**************************************************************************
* server :
* -
*
* Check COPYING file for Copyright
*
**************************************************************************/

#ifndef SERVER_H
#define SERVER_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#include <map>
#include <string>
#include <vector>

#ifdef HAVE_SN
#include <libsn/sn.h>
#include <glib.h>
#endif

struct Monitor {
    int x;
    int y;
    int width;
    int height;
    std::vector<std::string> names;
};

class Server {
  public:
    std::map<std::string, Atom> atoms_;

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
#ifdef HAVE_SN
    SnDisplay* sn_dsp;
    std::map<pid_t, SnLauncherContext*> pids;
    Atom atom;
#endif // HAVE_SN

    void Cleanup();
    int GetCurrentDesktop();
    int GetDesktop();
    int GetDesktopFromWindow(Window win);
    void InitAtoms();
    void InitVisual();
};


extern Server server;

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
