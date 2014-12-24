#ifndef X11_H
#define X11_H

#include <X11/Xlib.h>
#include <memory>


namespace {

class XFreeDeleter {
  public:
    void operator()(void* data) {
        XFree(data);
    }
};

}  // namespace


namespace util {
namespace x11 {

class ScopedErrorHandler {
  public:
    explicit ScopedErrorHandler(XErrorHandler new_handler);
    ~ScopedErrorHandler();

  private:
    XErrorHandler old_handler_;
};

template<typename T>
class ClientData : public std::unique_ptr<T, ::XFreeDeleter> {
  public:
    explicit ClientData(void* data)
        : std::unique_ptr<T, ::XFreeDeleter>(static_cast<T * >(data)) {
    }
};

}  // namespace x11
}  // namespace util


#endif // X11_H
