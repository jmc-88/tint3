#include "util/fs_test_utils.hh"

bool FakeFileSystemInterface::stat(std::string const& path, struct stat* buf) {
  // We pretend that any path passed to this function is a regular file, and
  // that it exists.
  buf->st_mode = S_IFREG;
  return true;
}
