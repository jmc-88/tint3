#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "util/common.h"
#include "util/fs.h"

namespace util {
namespace fs {

DirectoryContents::DirectoryContents(const std::string& path)
    : dir_(opendir(path.c_str())) {}

DirectoryContents::~DirectoryContents() {
  if (dir_ != nullptr) {
    closedir(dir_);
  }
}

DirectoryContents::iterator const DirectoryContents::begin() const {
  return DirectoryContents::iterator(dir_);
}

DirectoryContents::iterator const DirectoryContents::end() const {
  return DirectoryContents::iterator();
}

DirectoryContents::iterator::iterator()
    : dir_(nullptr), entry_(nullptr), pos_(-1) {}

DirectoryContents::iterator::iterator(DIR* dir)
    : dir_(dir), entry_(nullptr), pos_(-1) {
  if (dir_ != nullptr) {
    pos_ = telldir(dir_);
  }
}

DirectoryContents::iterator& DirectoryContents::iterator::operator++() {
  if (dir_ != nullptr) {
    entry_ = readdir(dir_);

    if (entry_ != nullptr) {
      pos_ = telldir(dir_);
    } else {
      dir_ = nullptr;
      pos_ = -1;
    }
  }

  return (*this);
}

const std::string DirectoryContents::iterator::operator*() const {
  if (entry_ != nullptr) {
    return entry_->d_name;
  }

  return std::string();
}

bool DirectoryContents::iterator::operator!=(
    DirectoryContents::iterator const& other) const {
  return (dir_ != other.dir_ || pos_ != other.pos_);
}

Path::Path(const std::string& path) : path_(path) {}

Path::Path(const char* path) : path_(path) {}

Path& Path::operator/(std::string const& component) {
  path_.append("/");
  path_.append(component);
  return (*this);
}

Path::operator std::string() const { return path_; }

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

Path HomeDirectory() {
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

}  // namespace fss
}  // namespace util
