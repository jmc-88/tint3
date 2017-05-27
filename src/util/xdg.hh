#ifndef TINT3_UTIL_XDG_HH
#define TINT3_UTIL_XDG_HH

#include <string>
#include <vector>

#include "util/fs.hh"

namespace util {
namespace xdg {
namespace basedir {

util::fs::Path ConfigHome();
util::fs::Path DataHome();
std::vector<std::string> ConfigDirs();
std::vector<std::string> DataDirs();

}  // namespace basedir
}  // namespace xdg
}  // namespace util

#endif  // TINT3_UTIL_XDG_HH
