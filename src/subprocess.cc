#include "subprocess.hh"

#include <cerrno>
#include <cstring>
#include <utility>

#include "util/log.hh"

void Subprocess::apply_options() {
  // Base case for the recursive template expansion in the header.
  // This one actually does nothing, it's only here to stop the recursion.
}

void Subprocess::set_option(capture&& option) {
  capture_ = std::move(option.value);
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

int Subprocess::start() {
  if (command_.empty()) {
    util::log::Error() << "Refusing to launch empty command\n";
    return -1;
  }

  std::function<void()> child_callback_ptr = child_callback_;
  if (capture_) {
    stdout_.reset(new util::Pipe{util::Pipe::Options::kNonBlocking});
    stderr_.reset(new util::Pipe{util::Pipe::Options::kNonBlocking});
    child_callback_ptr = [&] {
      if (dup2(stdout_->WriteEnd(), STDOUT_FILENO) == -1 ||
          dup2(stderr_->WriteEnd(), STDERR_FILENO) == -1) {
        util::log::Error() << "dup2: " << strerror(errno) << '\n';
        _exit(1);
      }
      if (child_callback_) {
        child_callback_();
      }
    };
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    util::log::Error() << "fork: " << std::strerror(errno) << '\n';
    return -1;
  }
  if (child_pid == 0) {
    if (child_callback_ptr) {
      child_callback_ptr();
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

bool Subprocess::communicate(std::ostream* out_ss, std::ostream* err_ss) const {
  static constexpr ssize_t kBufferSize = 1024;

  auto read_fully = [&](int fd, std::ostream& ss) -> bool {
    while (true) {
      char buffer[kBufferSize];
      int ret = read(fd, buffer, (kBufferSize - 1) * sizeof(char));
      if (ret <= 0) {
        if (ret == -1 && errno != EAGAIN) {
          util::log::Error() << "Failed reading from pipe " << fd << ": "
                             << strerror(errno) << '\n';
          return false;
        }
        return true;
      }
      buffer[ret] = '\0';
      ss << buffer;
    }
  };

  bool read_stdout = true;
  if (out_ss != nullptr) {
    read_stdout = read_fully(stdout_->ReadEnd(), *out_ss);
  }

  bool read_stderr = true;
  if (err_ss != nullptr) {
    read_stderr = read_fully(stderr_->ReadEnd(), *err_ss);
  }

  return read_stdout && read_stderr;
}
