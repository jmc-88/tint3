#ifndef TINT3_UTIL_X11_HH
#define TINT3_UTIL_X11_HH

#include <X11/Xlib.h>
#include <sys/types.h>

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>

#include "util/pipe.hh"
#include "util/timer.hh"

extern int signal_pending;
extern bool pending_children;

class Server;

namespace util {
namespace x11 {

class ScopedErrorHandler {
 public:
  explicit ScopedErrorHandler(XErrorHandler new_handler);
  ~ScopedErrorHandler();

 private:
  XErrorHandler old_handler_;
};

class XFreeDeleter {
 public:
  void operator()(void* data) const;
};

template <typename T>
class ClientData : public std::unique_ptr<T, XFreeDeleter> {
 public:
  explicit ClientData(void* data)
      : std::unique_ptr<T, XFreeDeleter>(static_cast<T*>(data)) {}
};

class EventLoop {
 public:
  using EventHandler = std::function<void(XEvent&)>;

  EventLoop(Server const* const server, Timer& timer);

  bool IsAlive() const;
  bool RunLoop();
  void WakeUp();
  EventLoop& RegisterHandler(int event, EventHandler handler);
  EventLoop& RegisterHandler(std::initializer_list<int> event_list,
                             EventHandler handler);

 private:
  bool alive_;
  Server const* const server_;
  int x11_file_descriptor_;
  util::SelfPipe self_pipe_;
  Timer& timer_;
  std::unordered_map<int, EventHandler> handler_map_;

  void ReapChildPIDs() const;
};

bool GetWMName(Display* display, Window window, std::string* output);

pid_t GetWindowPID(Window window);
int SetWindowPID(Window window);
Window CreateSimpleWindow(Window parent, int x, int y, unsigned int width,
                          unsigned int height, unsigned int border_width,
                          unsigned long border, unsigned long background);
Window CreateWindow(Window parent, int x, int y, unsigned int width,
                    unsigned int height, unsigned int border_width, int depth,
                    unsigned int window_class, Visual* visual,
                    unsigned long valuemask, XSetWindowAttributes* attributes);

}  // namespace x11
}  // namespace util

#endif  // TINT3_UTIL_X11_HH
