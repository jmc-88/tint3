#include <cstdlib>
#include <cstring>
#include <functional>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

#include "util/common.hh"
#include "util/environment.hh"
#include "util/fs.hh"
#include "util/log.hh"
#include "util/xdg.hh"

namespace {

using transform_fn = std::function<util::fs::Path(std::string)>;

transform_fn DefaultValue(util::fs::Path value) {
  return [value](absl::string_view other) {
    return !other.empty() ? other : value;
  };
}

transform_fn GetDefaultDirectory(absl::string_view relative_path) {
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

util::fs::Path DataHome() {
  static auto default_ = GetDefaultDirectory("/.local/share");
  return default_(environment::Get("XDG_DATA_HOME"));
}

std::vector<std::string> DataDirs() {
  static auto default_ = DefaultValue("/usr/local/share:/usr/share");
  auto path_components = default_(environment::Get("XDG_DATA_DIRS"));
  return absl::StrSplit(std::string(path_components), ":");
}

std::vector<std::string> ConfigDirs() {
  static auto default_ = DefaultValue("/usr/local/etc/xdg:/etc/xdg");
  auto path_components = default_(environment::Get("XDG_CONFIG_DIRS"));
  return absl::StrSplit(std::string(path_components), ":");
}

}  // namespace basedir
}  // namespace xdg
}  // namespace util
