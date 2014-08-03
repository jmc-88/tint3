#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "common.h"
#include "fs.h"
#include "xdg.h"

namespace {

std::function<std::string(std::string)> DefaultValue(std::string value) {
    return [value](std::string other) -> std::string {
        return (!other.empty()) ? other : value;
    };
}

std::vector<std::string> SplitString(std::string const& str, char sep) {
    auto beg = str.begin();
    auto end = std::find(beg, str.end(), sep);
    std::vector<std::string> parts;

    while (beg != str.end()) {
        parts.push_back(std::string(beg, end));
        beg = (end != str.end()) ? (end + 1) : end;
        end = std::find(beg, str.end(), sep);
    }

    return parts;
}

std::string ValidatePath(std::string path) {
    if (fs::DirectoryExists(path) && fs::IsAbsolutePath(path)) {
        return path;
    }

    return std::string();
}

std::function<std::string(std::string)> GetDefaultDirectory(
    char const* relative_path) {
    std::ostringstream ss;
    ss << fs::HomeDirectory() << relative_path;

    std::string resolved_directory = ss.str();

    if (fs::DirectoryExists(resolved_directory)) {
        return DefaultValue(resolved_directory);
    }

    // if we cannot resolve the specified home-relative directory, just return
    // an identity functor as a fallback
    return [](std::string path) {
        return path;
    };
}

} // namespace

namespace xdg {

namespace basedir {

std::string DataHome() {
    static auto default_ = GetDefaultDirectory("/.local/share");
    return ValidatePath(default_(GetEnvironment("XDG_DATA_HOME")));
}

std::string ConfigHome() {
    static auto default_ = GetDefaultDirectory("/.config");
    return ValidatePath(default_(GetEnvironment("XDG_CONFIG_HOME")));
}

std::string CacheHome() {
    static auto default_ = GetDefaultDirectory("/.cache");
    return ValidatePath(default_(GetEnvironment("XDG_CACHE_HOME")));
}

std::string RuntimeDir() {
    // FIXME: the value of $XDG_CACHE_HOME is what glib defaults to if
    // $XDG_RUNTIME_DIR is empty, but the specification also mandates that the
    // runtime directory be tied to the user's session lifetime (i.e., created
    // at user login time, and fully removed at user logout time), so there
    // might be a better choice for this default value
    static auto default_ = DefaultValue(CacheHome());
    return ValidatePath(default_(GetEnvironment("XDG_RUNTIME_DIR")));
}

std::vector<std::string> DataDirs() {
    static auto default_ = DefaultValue("/usr/local/share:/usr/share");
    return SplitString(default_(GetEnvironment("XDG_DATA_DIRS")), ':');
}

std::vector<std::string> ConfigDirs() {
    static auto default_ = DefaultValue("/etc/xdg");
    return SplitString(default_(GetEnvironment("XDG_CONFIG_DIRS")), ':');
}

} // namespace basedir

} // namespace xdg
