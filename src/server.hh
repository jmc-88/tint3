#ifndef TINT3_SERVER_HH
#define TINT3_SERVER_HH

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#include <string>
#include <unordered_map>
#include <vector>

#ifdef HAVE_SN
#include <libsn/sn.h>
#endif

#include "util/x11.hh"

struct Monitor {
  int x;
  int y;
  unsigned int width;
  unsigned int height;
  std::vector<std::string> names;
};

class Server {
 public:
  Display* dsp;
  Window composite_manager;
  bool real_transparency;
  unsigned int screen;
  int depth;
  // number of monitor (without monitor included into another one)
  unsigned int num_monitors;
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
  std::unordered_map<pid_t, SnLauncherContext*> pids;
#endif  // HAVE_SN

  void Cleanup();
  void UpdateCurrentDesktop();
  std::vector<std::string> GetDesktopNames() const;
  void UpdateNumberOfDesktops();
  int GetNumberOfDesktops();
  void GetRootPixmap();
  void InitGC(Window win);
  void InitAtoms();
  void InitDesktops();
  void InitVisual();
  void InitX11();

  unsigned int desktop() const;
  unsigned int num_desktops() const;

  Window root_window() const;
  void UpdateRootWindow();

  Atom atom(std::string const& name) const;

  template <typename T>
  util::x11::ClientData<T> GetProperty(Window win, Atom at, Atom type,
                                       int* num_results) {
    if (!win) {
      return util::x11::ClientData<T>(nullptr);
    }

    Atom type_ret;
    int format_ret = 0;
    unsigned long nitems_ret = 0;
    unsigned long bafter_ret = 0;
    unsigned char* prop_value = nullptr;
    int result =
        XGetWindowProperty(dsp, win, at, 0, 0x7fffffff, False, type, &type_ret,
                           &format_ret, &nitems_ret, &bafter_ret, &prop_value);

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
    auto data = GetProperty<unsigned long>(win, at, type, &num_results);
    return (data != nullptr) ? static_cast<T>(*data) : T();
  }

 private:
  Window root_window_;
  std::unordered_map<std::string, Atom> atoms_;
  unsigned int desktop_;
  unsigned int num_desktops_;
};

extern Server server;

void SendEvent32(Window win, Atom at, long data1, long data2, long data3);
int GetProperty32(Window win, Atom at, Atom type);
int ServerCatchError(Display* d, XErrorEvent* ev);

// detect monitors and desktops
void GetMonitors();

template <typename T>
util::x11::ClientData<T> ServerGetProperty(Window win, Atom at, Atom type,
                                           int* num_results) {
  return server.GetProperty<T>(win, at, type, num_results);
}

template <typename T>
T GetProperty32(Window win, Atom at, Atom type) {
  return server.GetProperty32<T>(win, at, type);
}

#endif  // TINT3_SERVER_HH
