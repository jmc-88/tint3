#ifndef X11_H
#define X11_H

#include <X11/Xlib.h>
#include <utility>

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
class ClientData {
  public:
    explicit ClientData(void* data)
        : data_(data) {
    }

    ClientData(ClientData&& other) {
        data_ = other.data_;
        other.data_ = nullptr;
    }

    ~ClientData() {
        if (data_ != nullptr) {
            XFree(data_);
        }
    }

    ClientData& operator=(ClientData other) {
        std::swap(data_, other.data_);
        return (*this);
    }

    operator T const() const {
        return reinterpret_cast<T>(data_);
    }

  private:
    void* data_;
};

}  // namespace x11
}  // namespace util

#endif // X11_H
