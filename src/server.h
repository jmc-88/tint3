#ifndef TINT3_SERVER_H
#define TINT3_SERVER_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#include <map>
#include <string>
#include <vector>

#ifdef HAVE_SN
#include <glib.h>
#include <libsn/sn.h>
#endif

#include "util/x11.h"

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
  bool real_transparency;
  // current desktop
  int desktop;
  int screen;
  int depth;
  int nb_desktop;
  // number of monitor (without monitor included into another one)
  int nb_monitor;
  std::vector<Monitor> monitor;
  bool got_root_win;
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
#endif  // HAVE_SN

  void Cleanup();
  int GetCurrentDesktop();
  int GetNumberOfDesktops();
  void InitGC(Window win);
  void InitAtoms();
  void InitVisual();
};

extern Server server;

void SendEvent32(Window win, Atom at, long data1, long data2, long data3);
int GetProperty32(Window win, Atom at, Atom type);
int ServerCatchError(Display* d, XErrorEvent* ev);

// detect root background
void GetRootPixmap();

// detect monitors and desktops
void GetMonitors();
void GetDesktops();

template <typename T>
util::x11::ClientData<T> ServerGetProperty(Window win, Atom at, Atom type,
                                           int* num_results) {
  if (!win) {
    return util::x11::ClientData<T>(nullptr);
  }

  Atom type_ret;
  int format_ret = 0;
  unsigned long nitems_ret = 0;
  unsigned long bafter_ret = 0;
  unsigned char* prop_value = nullptr;
  int result = XGetWindowProperty(server.dsp, win, at, 0, 0x7fffffff, False,
                                  type, &type_ret, &format_ret, &nitems_ret,
                                  &bafter_ret, &prop_value);

  // Send back resultcount
  if (num_results != nullptr) {
    (*num_results) = static_cast<int>(nitems_ret);
  }

  if (result == Success && prop_value != nullptr) {
    return util::x11::ClientData<T>(prop_value);
  }

  return util::x11::ClientData<T>(nullptr);
}

template <typename T>
T GetProperty32(Window win, Atom at, Atom type) {
  int num_results;
  auto data = ServerGetProperty<unsigned long>(win, at, type, &num_results);

  if (data != nullptr) {
    return static_cast<T>(*data);
  }

  return T();
}

#endif  // TINT3_SERVER_H
