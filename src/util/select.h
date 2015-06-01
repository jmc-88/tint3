#ifndef TINT3_UTIL_SELECT_H
#define TINT3_UTIL_SELECT_H

#if _POSIX_C_SOURCE >= 201112L
#include <sys/select.h>
#else  // _POSIX_C_SOURCE >= 201112L
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _POSIX_C_SOURCE >= 201112L

namespace util {

class FdSet {
 public:
  FdSet();
  ~FdSet();

  int maxfd() const;
  fd_set* fdset();

  FdSet& Add(int fd);
  FdSet& Remove(int fd);
  FdSet& Clear();
  bool Has(int fd);

 private:
  int maxfd_;
  fd_set fdset_;
};

class SyncIoMux {
 public:
  SyncIoMux();

  bool Select(struct timeval* timeout);

  FdSet& read_fdset();
  FdSet& write_fdset();
  FdSet& except_fdset();

 private:
  FdSet read_fdset_;
  FdSet write_fdset_;
  FdSet except_fdset_;
};

}  // namespace util

#endif  // TINT3_UTIL_SELECT_H
