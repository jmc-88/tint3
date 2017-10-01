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

#include "util/log.hh"

// forward declaration
class Server;

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

template <typename It_>
class iterator_range {
 public:
  iterator_range() = delete;
  iterator_range(iterator_range const&) = default;
  iterator_range(iterator_range&) = default;

  iterator_range(It_ begin, It_ end) : begin_{begin}, end_{end} {}

  iterator_range& operator=(iterator_range other) {
    std::swap(begin_, other.begin_);
    std::swap(end_, other.end_);
    return (*this);
  }

  It_ begin() const { return begin_; }

  It_ end() const { return end_; }

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

// ShellExec executes a command through /bin/sh, invoking the provided callback
// in the child process.
template <typename Callback>
pid_t ShellExec(std::string const& command, Callback callback) {
  if (command.empty()) {
    util::log::Error() << "Refusing to launch empty command\n";
    return -1;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    util::log::Error() << "fork: " << std::strerror(errno) << '\n';
    return -1;
  }
  if (child_pid == 0) {
    callback();

    // change for the fork the signal mask
    //          sigset_t sigset;
    //          sigprocmask(SIG_SETMASK, &sigset, 0);
    //          sigprocmask(SIG_UNBLOCK, &sigset, 0);

    // Allow child to exist after parent destruction
    setsid();

    // "/bin/sh" should be guaranteed to be a POSIX-compliant shell
    // accepting the "-c" flag:
    //   http://pubs.opengroup.org/onlinepubs/9699919799/utilities/sh.html
    execlp("/bin/sh", "sh", "-c", command.c_str(), nullptr);

    // In case execlp() fails and the process image is not replaced
    util::log::Error() << "execlp(\"" << command
                       << "\"): " << std::strerror(errno) << '\n';
    _exit(1);
  }
  return child_pid;
}

// ShellExec executes a command through /bin/sh.
pid_t ShellExec(std::string const& command);

namespace string {

class Builder {
  std::ostringstream ss_;

 public:
  template <typename T>
  Builder& operator<<(T const& value) {
    ss_ << value;
    return (*this);
  }

  Builder& operator<<(std::nullptr_t const& /*value*/);

  operator std::string() const;
};

template <typename T>
std::string Representation(T const& value) {
  return Builder() << value;
}

bool ToNumber(std::string const& str, int* ptr);
bool ToNumber(std::string const& str, long* ptr);
bool ToNumber(std::string const& str, float* ptr);

std::string& Trim(std::string* str);
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

extern const unsigned int kAllDesktops;

bool SignalAction(int signal_number, void signal_handler(int), int flags = 0);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustASB(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float saturation_adjustment, float brightness_adjustment);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Server* server, Drawable d, int x, int y, int w, int h);

#endif  // TINT3_UTIL_COMMON_HH
