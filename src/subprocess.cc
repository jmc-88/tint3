#include "subprocess.hh"

#include <cerrno>
#include <cstring>
#include <utility>

#include "util/log.hh"

void Subprocess::apply_options() {
  // Base case for the recursive template expansion in the header.
  // This one actually does nothing, it's only here to stop the recursion.
}

void Subprocess::set_option(child_callback&& option) {
  child_callback_ = std::move(option.value);
}

void Subprocess::set_option(session_leader&& option) {
  session_leader_ = std::move(option.value);
}

void Subprocess::set_option(shell&& option) {
  shell_ = std::move(option.value);
}

int Subprocess::start() const {
  if (command_.empty()) {
    util::log::Error() << "Refusing to launch empty command\n";
    return -1;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    util::log::Error() << "fork: " << std::strerror(errno) << '\n';
    return -1;
  }
  if (child_pid == 0) {
    if (child_callback_) {
      child_callback_();
    }

    // Allow child to exist after parent destruction
    if (session_leader_) {
      setsid();
    }

    if (shell_) {
      // "/bin/sh" should be guaranteed to be a POSIX-compliant shell
      // accepting the "-c" flag:
      //   http://pubs.opengroup.org/onlinepubs/9699919799/utilities/sh.html
      execlp("/bin/sh", "sh", "-c", command_.c_str(), nullptr);

      // In case execlp() fails and the process image is not replaced
      util::log::Error() << "execlp(\"" << command_
                         << "\"): " << std::strerror(errno) << '\n';
      _exit(1);
    } else {
      execl(command_.c_str(), command_.c_str(), nullptr);

      // In case execl() fails and the process image is not replaced
      util::log::Error() << "execl(\"" << command_
                         << "\"): " << std::strerror(errno) << '\n';
      _exit(1);
    }
  }
  return child_pid;
}

pid_t ShellExec(std::string const& command) {
  // Delegate to the template version of this function with a no-op callback
  return ShellExec(command, [] {});
}
