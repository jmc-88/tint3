#include <cstdlib>
#include <cstring>
#include <functional>

#include "util/common.h"
#include "util/environment.h"
#include "util/fs.h"
#include "util/xdg.h"

namespace {

std::function<std::string(std::string)> DefaultValue(std::string value) {
  return [value](std::string other)
      -> std::string { return (!other.empty()) ? other : value; };
}

std::string ValidatePath(std::string path) {
  if (util::fs::DirectoryExists(path)) {
    return path;
  }

  return std::string();
}

std::function<std::string(std::string)> GetDefaultDirectory(
    char const* relative_path) {
  std::string resolved_directory = util::fs::HomeDirectory() / relative_path;

  if (util::fs::DirectoryExists(resolved_directory)) {
    return DefaultValue(resolved_directory);
  }

  // if we cannot resolve the specified home-relative directory, just return
  // an identity functor as a fallback
  return [](std::string path) { return path; };
}

}  // namespace

namespace util {
namespace xdg {
namespace basedir {

util::fs::Path DataHome() {
  static auto default_ = GetDefaultDirectory("/.local/share");
  return ValidatePath(default_(environment::Get("XDG_DATA_HOME")));
}

util::fs::Path ConfigHome() {
  static auto default_ = GetDefaultDirectory("/.config");
  return ValidatePath(default_(environment::Get("XDG_CONFIG_HOME")));
}

util::fs::Path CacheHome() {
  static auto default_ = GetDefaultDirectory("/.cache");
  return ValidatePath(default_(environment::Get("XDG_CACHE_HOME")));
}

util::fs::Path RuntimeDir() {
  // FIXME: the value of $XDG_CACHE_HOME is what glib defaults to if
  // $XDG_RUNTIME_DIR is empty, but the specification also mandates that the
  // runtime directory be tied to the user's session lifetime (i.e., created
  // at user login time, and fully removed at user logout time), so there
  // might be a better choice for this default value
  static auto default_ = DefaultValue(CacheHome());
  return ValidatePath(default_(environment::Get("XDG_RUNTIME_DIR")));
}

std::vector<std::string> DataDirs() {
  static auto default_ = DefaultValue("/usr/local/share:/usr/share");
  return SplitString(default_(environment::Get("XDG_DATA_DIRS")), ':');
}

std::vector<std::string> ConfigDirs() {
  static auto default_ = DefaultValue("/usr/local/etc/xdg:/etc/xdg");
  return SplitString(default_(environment::Get("XDG_CONFIG_DIRS")), ':');
}

}  // namespace basedir
}  // namespace xdg
}  // namespace util
