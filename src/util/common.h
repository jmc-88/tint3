/**************************************************************************
* Common declarations
*
**************************************************************************/

#ifndef COMMON_H
#define COMMON_H

#define WM_CLASS_TINT "panel"

#include <Imlib2.h>
#include <glib-object.h>
#include <signal.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "util/area.h"

namespace util {

class GObjectUnrefDeleter {
 public:
  void operator()(gpointer data) const;
};

template <typename T>
using GObjectPtr = std::unique_ptr<T, GObjectUnrefDeleter>;

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

class StringBuilder {
  std::ostringstream ss_;

 public:
  template <typename T>
  StringBuilder& operator<<(T const& value) {
    ss_ << value;
    return (*this);
  }

  operator std::string() const { return ss_.str(); }
};

bool SignalAction(int signal_number, void signal_handler(int), int flags = 0);

std::string& StringTrim(std::string& str);

// Wrapper around strtof()
float StringToFloat(std::string const& str, char** endptr = nullptr);

template <typename T>
std::string StringRepresentation(T const& value) {
  return StringBuilder() << value;
}

std::vector<std::string> SplitString(std::string const& str, char sep);

// execute a command by calling fork
void TintExec(std::string const& command);

// color conversion
bool GetColor(const std::string& hex, double* rgb);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustAsb(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float satur, float bright);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Drawable d, int x, int y, int w, int h);

bool RegexMatch(std::string const& pattern, std::string const& string);

#endif
