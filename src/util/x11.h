#ifndef TINT3_UTIL_X11_H
#define TINT3_UTIL_X11_H

#include <X11/Xlib.h>
#include <sys/types.h>

#include <functional>
#include <initializer_list>
#include <map>
#include <memory>

#include "util/timer.h"

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

  bool RunLoop();
  EventLoop& RegisterHandler(int event, EventHandler handler);
  EventLoop& RegisterHandler(std::initializer_list<int> event_list,
                             EventHandler handler);
  EventLoop& RegisterDefaultHandler(EventHandler handler);

 private:
  Server const* const server_;
  int x11_file_descriptor_;
  Timer& timer_;
  EventHandler default_handler_;
  std::map<int, EventHandler> handler_map_;
};

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

#endif  // TINT3_UTIL_X11_H
