#ifndef TINT3_UTIL_FS_TEST_UTILS_HH
#define TINT3_UTIL_FS_TEST_UTILS_HH

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "util/fs.hh"

class FakeFileSystemInterface : public util::fs::SystemInterface {
 public:
  bool stat(std::string const& path, struct stat* buf);
};

#endif  // TINT3_UTIL_FS_TEST_UTILS_HH
