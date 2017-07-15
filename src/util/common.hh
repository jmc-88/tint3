#ifndef TINT3_UTIL_COMMON_HH
#define TINT3_UTIL_COMMON_HH

#define WM_CLASS_TINT "panel"

#include <Imlib2.h>
#include <glib-object.h>
#include <signal.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace util {

class GObjectUnrefDeleter {
 public:
  void operator()(gpointer data) const;
};

template <typename T>
using GObjectPtr = std::unique_ptr<T, GObjectUnrefDeleter>;

// ScopedDeleter is a class that receives a callable and invokes it when it
// goes out of scope.
template <typename T>
class ScopedDeleter {
 public:
  ScopedDeleter(T deleter) : deleter_(deleter) {}
  ~ScopedDeleter() { deleter_(); }

 private:
  T deleter_;
};

// MakeScopedDeleter is a helper method to create a ScopedDeleter of the right
// type from the argument it's passed.
template <typename T>
ScopedDeleter<T> MakeScopedDeleter(T deleter) {
  return deleter;
}

namespace string {

class Builder {
  std::ostringstream ss_;

 public:
  template <typename T>
  Builder& operator<<(T const& value) {
    ss_ << value;
    return (*this);
  }

  operator std::string() const { return ss_.str(); }
};

template <typename T>
std::string Representation(T const& value) {
  return Builder() << value;
}

std::string& Trim(std::string& str);
std::vector<std::string> Split(std::string const& str, char sep);
bool StartsWith(std::string const& str, std::string const& other);
bool RegexMatch(std::string const& pattern, std::string const& string);

void ToLowerCase(std::string* str);

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

extern unsigned int const kAllDesktops;

bool SignalAction(int signal_number, void signal_handler(int), int flags = 0);

// fork and execute a shell script
void TintShellExec(std::string const& command);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustASB(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float saturation_adjustment, float brightness_adjustment);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Drawable d, int x, int y, int w, int h);

#endif  // TINT3_UTIL_COMMON_HH
