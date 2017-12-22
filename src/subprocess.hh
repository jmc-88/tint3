#ifndef TINT3_SUBPROCESS_HH
#define TINT3_SUBPROCESS_HH

#include <functional>
#include <string>

#include <unistd.h>

struct session_leader {
  session_leader(bool session_leader) : value{session_leader} {}
  bool value = true;
};

struct shell {
  shell(bool shell) : value{shell} {}
  bool value = true;
};

struct child_callback {
  child_callback(std::function<void()> child_callback)
      : value{child_callback} {}
  std::function<void()> value;
};

class Subprocess {
 public:
  Subprocess(Subprocess const& other) = delete;
  Subprocess(Subprocess&& other) = default;

  void set_option(child_callback&& option);
  void set_option(session_leader&& option);
  void set_option(shell&& option);

  pid_t start() const;

 private:
  template <typename... Args>
  friend Subprocess make_subprocess(std::string command, Args&&... args);

  template <typename... Args>
  explicit Subprocess(std::string command, Args&&... args) : command_{command} {
    apply_options(args...);
  }

  void apply_options();

  template <typename T, typename... Args>
  void apply_options(T first_option, Args... rest) {
    set_option(std::forward<T>(first_option));
    apply_options(rest...);
  }

  std::string command_;
  std::function<void()> child_callback_;
  bool shell_ = false;
  bool session_leader_ = false;
};

template <typename... Args>
Subprocess make_subprocess(std::string command, Args&&... args) {
  return Subprocess{command, std::forward<Args>(args)...};
}

// ShellExec executes a command through /bin/sh, invoking the provided callback
// in the child process.
template <typename... Args>
pid_t ShellExec(std::string const& command, Args&&... args) {
  auto sp = make_subprocess(command, session_leader{true}, shell{true},
                            std::forward<Args>(args)...);
  return sp.start();
}

#endif  // TINT3_SUBPROCESS_HH
