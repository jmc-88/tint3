#include "util/fs_test_utils.hh"

bool FakeFileSystemInterface::stat(std::string const& path, struct stat* buf) {
  auto response = stat_responses.find(path);
  if (response == stat_responses.end()) return false;
  if (response->second.empty()) return false;

  *buf = response->second.front();
  response->second.pop_front();
  return true;
}

bool FakeFileSystemInterface::symlink(std::string const& target,
                                      std::string const& linkpath) {
  auto response = symlink_responses.find({target, linkpath});
  if (response == symlink_responses.end()) return false;
  if (response->second.empty()) return false;

  bool result = response->second.front();
  response->second.pop_front();
  return result;
}

bool FakeFileSystemInterface::unlink(std::string const& path) {
  auto response = unlink_responses.find(path);
  if (response == unlink_responses.end()) return false;
  if (response->second.empty()) return false;

  bool result = response->second.front();
  response->second.pop_front();
  return result;
}
