#ifndef TINT3_SERVER_HH
#define TINT3_SERVER_HH

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "startup_notification.hh"
#include "util/x11.hh"

struct Monitor {
  unsigned int number;
  int x;
  int y;
  unsigned int width;
  unsigned int height;
  std::vector<std::string> names;
};

class Server {
 public:
  Display* dsp = nullptr;
  Window composite_manager = None;
  bool real_transparency = false;
  unsigned int screen = 0;
  int depth = 32;
  // number of monitors (without monitors included into another one)
  unsigned int num_monitors = 0;
  std::vector<Monitor> monitor;
  Visual* visual = nullptr;
  // root background
  Pixmap root_pmap = None;
  GC gc = None;
  util::x11::Colormap colormap;

  SnDisplay* sn_dsp = nullptr;
#ifdef HAVE_SN
  std::unordered_map<pid_t, StartupNotification> pids;
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
  Window root_window_ = None;
  std::unordered_map<std::string, Atom> atoms_;
  unsigned int desktop_ = 0;
  unsigned int num_desktops_ = 0;
};

extern Server server;

void SendEvent32(Window win, Atom at, long data1, long data2, long data3);
int GetProperty32(Window win, Atom at, Atom type);

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
