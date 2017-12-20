#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "util/log.hh"
#include "util/pipe.hh"

namespace util {

Pipe::Pipe(Pipe::Options opts) : alive_{true} {
  if (pipe(pipe_fd_) != 0) {
    util::log::Error() << "Failed to create pipe: " << std::strerror(errno)
                       << '\n';
    alive_ = false;
    return;
  }

  if (opts == Options::kNonBlocking) {
    if (!SetFlag(ReadEnd(), O_NONBLOCK)) {
      alive_ = false;
      return;
    }
    if (!SetFlag(WriteEnd(), O_NONBLOCK)) {
      alive_ = false;
      return;
    }
  }
}

Pipe::Pipe(Pipe&& other) {
  alive_ = other.alive_;
  other.alive_ = false;

  pipe_fd_[0] = other.pipe_fd_[0];
  other.pipe_fd_[0] = -1;

  pipe_fd_[1] = other.pipe_fd_[1];
  other.pipe_fd_[1] = -1;
}

Pipe::~Pipe() {
  if (alive_) {
    close(pipe_fd_[0]);
    close(pipe_fd_[1]);
  }
}

bool Pipe::IsAlive() const { return alive_; }

int Pipe::ReadEnd() const { return pipe_fd_[0]; }

int Pipe::WriteEnd() const { return pipe_fd_[1]; }

bool Pipe::SetFlag(int fd, int flag) const {
  int flags = fcntl(ReadEnd(), F_GETFL);
  if (flags == -1) {
    util::log::Error() << "Failed to retrieve flags on read end of self pipe: "
                       << std::strerror(errno) << '\n';
    return false;
  }
  if (fcntl(ReadEnd(), F_SETFL, flags | O_NONBLOCK) == -1) {
    util::log::Error() << "Failed to flag read end of self pipe "
                       << "as non blocking: " << std::strerror(errno) << '\n';
    return false;
  }
  return true;
}

SelfPipe::SelfPipe() : Pipe{Pipe::Options::kNonBlocking} {}

void SelfPipe::WriteOneByte() { write(WriteEnd(), "1", 1); }

void SelfPipe::ReadPendingBytes() {
  while (true) {
    char byte;
    int ret = read(ReadEnd(), &byte, 1);
    if (ret <= 0) {
      if (ret == -1 && errno != EAGAIN) {
        util::log::Error() << "Failed reading from self pipe: "
                           << strerror(errno) << '\n';
      }
      break;
    }
  }
}

}  // namespace util
