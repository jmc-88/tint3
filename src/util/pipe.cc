#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "util/log.hh"
#include "util/pipe.hh"

namespace util {

Pipe::Pipe() : alive_(true) {
  if (pipe(pipe_fd_) != 0) {
    util::log::Error() << "Failed to create pipe: " << std::strerror(errno)
                       << '\n';
    alive_ = false;
  }
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

SelfPipe::SelfPipe() : Pipe(), alive_(true) {
  if (!Pipe::IsAlive()) {
    alive_ = false;
    return;
  }

  int flags = -1;

  flags = fcntl(ReadEnd(), F_GETFL);
  if (flags == -1) {
    util::log::Error() << "Failed to retrieve flags on read end of self pipe: "
                       << std::strerror(errno) << '\n';
    alive_ = false;
    return;
  }

  if (fcntl(ReadEnd(), F_SETFL, flags | O_NONBLOCK) == -1) {
    util::log::Error() << "Failed to flag read end of self pipe "
                       << "as non blocking: " << std::strerror(errno) << '\n';
    alive_ = false;
    return;
  }

  flags = fcntl(WriteEnd(), F_GETFL);
  if (flags == -1) {
    util::log::Error() << "Failed to retrieve flags on write end of self pipe: "
                       << std::strerror(errno) << '\n';
    alive_ = false;
    return;
  }

  if (fcntl(WriteEnd(), F_SETFL, flags | O_NONBLOCK) == -1) {
    util::log::Error() << "Failed to flag write end of self pipe "
                       << "as non blocking: " << std::strerror(errno) << '\n';
    alive_ = false;
    return;
  }
}

bool SelfPipe::IsAlive() const { return alive_; }

void SelfPipe::WriteOneByte() { write(WriteEnd(), "1", 1); }

void SelfPipe::ReadPendingBytes() {
  while (true) {
    char byte;
    if (read(ReadEnd(), &byte, 1) == -1) {
      if (errno != EAGAIN) {
        util::log::Error() << "Failed reading from self pipe: "
                           << strerror(errno) << '\n';
      }
      break;
    }
  }
}

}  // namespace util
