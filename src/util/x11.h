#ifndef X11_H
#define X11_H

#include <X11/Xlib.h>

#include <initializer_list>
#include <functional>
#include <map>
#include <memory>

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

template<typename T>
class ClientData : public std::unique_ptr<T, XFreeDeleter> {
  public:
    explicit ClientData(void* data)
        : std::unique_ptr<T, XFreeDeleter>(static_cast<T * >(data)) {
    }
};

class EventLoop {
  public:
    using EventHandler = std::function<void(XEvent&)>;

    EventLoop(Server const* const server);

    bool RunLoop();
    EventLoop& RegisterHandler(int event,
                               EventHandler handler);
    EventLoop& RegisterHandler(std::initializer_list<int> event_list,
                               EventHandler handler);
    EventLoop& RegisterDefaultHandler(EventHandler handler);

  private:
    Server const* const server_;
    int x11_file_descriptor_;
    EventHandler default_handler_;
    std::map<int, EventHandler> handler_map_;
};

}  // namespace x11
}  // namespace util


#endif // X11_H
