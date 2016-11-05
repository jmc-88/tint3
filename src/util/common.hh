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

#include "util/area.hh"

namespace util {

class GObjectUnrefDeleter {
 public:
  void operator()(gpointer data) const;
};

template <typename T>
using GObjectPtr = std::unique_ptr<T, GObjectUnrefDeleter>;

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

extern int const kAllDesktops;

bool SignalAction(int signal_number, void signal_handler(int), int flags = 0);

// fork and execute a shell script
void TintShellExec(std::string const& command);

// color conversion
bool GetColor(const std::string& hex, double* rgb);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustAsb(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float satur, float bright);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Drawable d, int x, int y, int w, int h);

#endif  // TINT3_UTIL_COMMON_HH
