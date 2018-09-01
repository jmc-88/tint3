#ifndef TINT3_UTIL_COMMON_HH
#define TINT3_UTIL_COMMON_HH

#define WM_CLASS_TINT "panel"

#include <Imlib2.h>
#include <glib-object.h>
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

#include "util/log.hh"

// forward declaration
class Server;

namespace util {

struct GObjectUnrefDeleter {
  void operator()(gpointer data) const;
};

template <typename T>
using GObjectPtr = std::unique_ptr<T, GObjectUnrefDeleter>;

// ScopedCallback is a class that receives a callable and invokes it when it
// goes out of scope.
template <typename T>
struct ScopedCallback {
  explicit ScopedCallback(T callback) : callback_{callback} {}
  ~ScopedCallback() { callback_(); }
  T callback_;
};

// MakeScopedCallback is a helper method to create a ScopedCallback of the right
// type from the argument it's passed.
template <typename T>
ScopedCallback<T> MakeScopedCallback(T callback) {
  return ScopedCallback<T>{callback};
}

template <typename It_>
class iterator_range {
 public:
  using iterator = It_;
  using const_iterator = It_;

  iterator_range() = delete;
  iterator_range(iterator_range const&) = default;
  iterator_range(iterator_range&) = default;

  iterator_range(It_ begin, It_ end) : begin_{begin}, end_{end} {}
  It_ begin() const { return begin_; }
  It_ end() const { return end_; }

  iterator_range& operator=(iterator_range other) {
    std::swap(begin_, other.begin_);
    std::swap(end_, other.end_);
    return (*this);
  }

 private:
  It_ begin_;
  It_ end_;
};

template <typename It_>
iterator_range<It_> make_iterator_range(It_ begin, It_ end) {
  return iterator_range<It_>{begin, end};
}

template <typename T>
iterator_range<typename T::iterator> range_skip_n(T& container, size_t offset) {
  return make_iterator_range(container.begin() + offset, container.end());
}

namespace string {

template <typename T>
std::string Representation(T const& value) {
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

bool ToNumber(absl::string_view str, int* ptr);
bool ToNumber(absl::string_view str, long* ptr);
bool ToNumber(absl::string_view str, float* ptr);

bool RegexMatch(std::string const& pattern, std::string const& string);

}  // namespace string
}  // namespace util

// mouse actions
enum class MouseAction {
  kNone,
  kClose,
  kToggle,
  kIconify,
  kShade,
  kToggleIconify,
  kMaximizeRestore,
  kMaximize,
  kRestore,
  kDesktopLeft,
  kDesktopRight,
  kNextTask,
  kPrevTask
};

extern const unsigned int kAllDesktops;

bool SignalAction(int signal_number, void signal_handler(int), int flags = 0);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustASB(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float saturation_adjustment, float brightness_adjustment);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Server* server, Drawable d, int x, int y, int w, int h);

std::ostream& operator<<(std::ostream& os, std::nullptr_t);

#endif  // TINT3_UTIL_COMMON_HH
