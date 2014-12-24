#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "util/common.h"
#include "util/fs.h"

namespace util {

namespace fs {

std::string BuildPath(std::initializer_list<std::string> parts) {
    std::ostringstream ss;
    bool first = true;

    for (auto const& p : parts) {
        if (!first) {
            // tint2 has only ever supported Unix systems, so it's probably not
            // worth bothering with other kinds of path separators at all
            ss << '/';
        }

        first = false;
        ss << p;
    }

    return ss.str();
}

bool CopyFile(std::string const& from_path, std::string const& to_path) {
    std::ifstream from(from_path);

    if (!from) {
        return false;
    }

    std::ofstream to(to_path);

    if (!to) {
        return false;
    }

    std::istreambuf_iterator<char> begin_from(from);
    std::istreambuf_iterator<char> end_from;
    std::ostreambuf_iterator<char> begin_to(to);

    std::copy(begin_from, end_from, begin_to);
    return true;
}

bool CreateDirectory(std::string const& path, mode_t mode) {
    if (DirectoryExists(path)) {
        return true;
    }

    return mkdir(path.c_str(), mode);
}

bool DirectoryExists(std::string const& path) {
    struct stat info;

    if (stat(path.c_str(), &info) != 0) {
        return false;
    }

    return S_ISDIR(info.st_mode);
}

bool FileExists(std::string const& path) {
    struct stat info;

    if (stat(path.c_str(), &info) != 0) {
        return false;
    }

    return S_ISREG(info.st_mode);
}

bool FileExists(std::initializer_list<std::string> parts) {
    return FileExists(BuildPath(parts));
}

std::string HomeDirectory() {
    std::string home = GetEnvironment("HOME");

    if (!home.empty()) {
        return home;
    }

    struct passwd* pwd = getpwuid(getuid());

    if (pwd != nullptr) {
        return pwd->pw_dir;
    }

    // if we got here both getenv() and getpwuid() failed, so let's just return
    // an empty string to indicate failure
    return std::string();
}

bool IsAbsolutePath(std::string const& path) {
    char* real_path = realpath(path.c_str(), nullptr);

    if (real_path == nullptr) {
        return false;
    }

    bool is_absolute = (std::strcmp(path.c_str(), real_path) == 0);
    std::free(real_path);
    return is_absolute;
}

bool ReadFileByLine(std::string const& path,
                    std::function<void(std::string const&)> fn) {
    std::ifstream is(path);

    if (!is.good()) {
        return false;
    }

    while (!is.eof()) {
        std::string line;
        std::getline(is, line);
        fn(line);
    }

    return true;
}

} // namespace fs

} // namespace util
