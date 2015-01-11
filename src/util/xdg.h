#ifndef TINT3_XDG_H
#define TINT3_XDG_H

#include <string>
#include <vector>

namespace util {
namespace xdg {
namespace basedir {

std::string DataHome();
std::string ConfigHome();
std::string CacheHome();
std::string RuntimeDir();
std::vector<std::string> DataDirs();
std::vector<std::string> ConfigDirs();

}  // namespace basedir
}  // namespace xdg
}  // namespace util

#endif  // TINT3_XDG_H
