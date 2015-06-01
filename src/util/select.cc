#include <algorithm>

#include "util/select.h"

namespace {

template <typename _First>
_First max(_First i0) {
  return i0;
}

template <typename _First, typename... _Rest>
_First max(_First i0, _Rest... iN) {
  return std::max(i0, max(iN...));
}

}  // namespace

namespace util {

FdSet::FdSet() : maxfd_(0) { Clear(); }

FdSet::~FdSet() { Clear(); }

int FdSet::maxfd() const { return maxfd_; }

fd_set* FdSet::fdset() { return &fdset_; }

FdSet& FdSet::Add(int fd) {
  FD_SET(fd, &fdset_);
  maxfd_ = std::max(maxfd_, fd);
  return (*this);
}

FdSet& FdSet::Remove(int fd) {
  FD_CLR(fd, &fdset_);
  return (*this);
}

FdSet& FdSet::Clear() {
  FD_ZERO(&fdset_);
  return (*this);
}

bool FdSet::Has(int fd) { return FD_ISSET(fd, &fdset_); }

SyncIoMux::SyncIoMux() {}

bool SyncIoMux::Select(struct timeval* timeout) {
  int maxfd =
      ::max(read_fdset_.maxfd(), write_fdset_.maxfd(), except_fdset_.maxfd());

  return select(maxfd + 1, read_fdset_.fdset(), write_fdset_.fdset(),
                except_fdset_.fdset(), timeout) != -1;
}

FdSet& SyncIoMux::read_fdset() { return read_fdset_; }

FdSet& SyncIoMux::write_fdset() { return write_fdset_; }

FdSet& SyncIoMux::except_fdset() { return except_fdset_; }

}  // namespace util
