#ifndef TINT3_UTIL_FS_TEST_UTILS_HH
#define TINT3_UTIL_FS_TEST_UTILS_HH

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <string>
#include <unordered_map>

#include "util/fs.hh"

namespace std {

template <typename First, typename Second>
struct hash<std::pair<First, Second>> {
  size_t operator()(std::pair<First, Second> const& p) const {
    size_t lhs = std::hash<First>()(p.first);
    size_t rhs = std::hash<Second>()(p.second);
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
  };
};

}  // namespace std

struct FakeFileSystemInterface : public util::fs::SystemInterface {
  bool stat(std::string const& path, struct stat* buf);
  std::unordered_map<std::string, std::list<struct stat>> stat_responses;

  bool symlink(std::string const& target, std::string const& linkpath);
  std::unordered_map<std::pair<std::string, std::string>, std::list<bool>>
      symlink_responses;

  bool unlink(std::string const& path);
  std::unordered_map<std::string, std::list<bool>> unlink_responses;
};

#endif  // TINT3_UTIL_FS_TEST_UTILS_HH
