#ifndef TINT3_UTIL_XDG_H
#define TINT3_UTIL_XDG_H

#include <string>
#include <vector>

#include "util/fs.h"

namespace util {
namespace xdg {
namespace basedir {

util::fs::Path DataHome();
util::fs::Path ConfigHome();
util::fs::Path CacheHome();
util::fs::Path RuntimeDir();
std::vector<std::string> DataDirs();
std::vector<std::string> ConfigDirs();

}  // namespace basedir
}  // namespace xdg
}  // namespace util

#endif  // TINT3_UTIL_XDG_H
