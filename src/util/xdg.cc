#include <cstdlib>
#include <cstring>
#include <functional>

#include "util/common.hh"
#include "util/environment.hh"
#include "util/fs.hh"
#include "util/xdg.hh"

namespace {

std::function<std::string(std::string)> DefaultValue(std::string value) {
  return [value](std::string other) -> std::string {
    return (!other.empty()) ? other : value;
  };
}

std::function<std::string(std::string)> GetDefaultDirectory(
    char const* relative_path) {
  return DefaultValue(util::fs::HomeDirectory() / relative_path);
}

}  // namespace

namespace util {
namespace xdg {
namespace basedir {

util::fs::Path ConfigHome() {
  static auto default_ = GetDefaultDirectory("/.config");
  return default_(environment::Get("XDG_CONFIG_HOME"));
}

std::vector<std::string> DataDirs() {
  static auto default_ = DefaultValue("/usr/local/share:/usr/share");
  return util::string::Split(default_(environment::Get("XDG_DATA_DIRS")), ':');
}

std::vector<std::string> ConfigDirs() {
  static auto default_ = DefaultValue("/usr/local/etc/xdg:/etc/xdg");
  return util::string::Split(default_(environment::Get("XDG_CONFIG_DIRS")),
                             ':');
}

}  // namespace basedir
}  // namespace xdg
}  // namespace util
