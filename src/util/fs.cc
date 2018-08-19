#include <libgen.h>
#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

#include "util/common.hh"
#include "util/environment.hh"
#include "util/fs.hh"

namespace util {
namespace fs {

bool SystemInterface::stat(std::string const& path, struct stat* buf) {
  return ::stat(path.c_str(), buf) == 0;
}

namespace {

SystemInterface default_system_interface;
SystemInterface* system_interface = &default_system_interface;

}  // namespace

SystemInterface* SetSystemInterface(SystemInterface* interface) {
  SystemInterface* old_interface = system_interface;
  system_interface = interface;
  return old_interface;
}

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

namespace {

absl::string_view StripTrailingSlash(absl::string_view path) {
  if (path == "/") return path;
  return absl::StripSuffix(path, "/");
}

}  // namespace

Path::Path(const char* path) : Path(absl::string_view(path)) {}
Path::Path(std::string const& path) : Path(absl::string_view(path)) {}
Path::Path(absl::string_view path) : path_{StripTrailingSlash(path)} {}

Path& Path::operator=(Path other) {
  std::swap(path_, other.path_);
  return (*this);
}

Path Path::operator/(absl::string_view component) {
  return Path{*this} /= component;
}

Path& Path::operator/=(absl::string_view component) {
  if (!path_.empty() && path_.back() != '/') {
    path_.push_back('/');
  }

  absl::string_view stripped_component = StripTrailingSlash(component);
  if (path_.empty()) {
    absl::StrAppend(&path_, stripped_component);
  } else {
    absl::StrAppend(&path_, absl::StripPrefix(stripped_component, "/"));
  }
  return (*this);
}

Path::operator std::string() const { return path_; }

std::string Path::BaseName() const {
  char data[path_.length() + 1];
  path_.copy(data, path_.length(), 0);
  data[path_.length()] = '\0';
  return basename(data);
}

Path Path::DirectoryName() const {
  char data[path_.length() + 1];
  path_.copy(data, path_.length(), 0);
  data[path_.length()] = '\0';
  return dirname(data);
}

bool Path::operator==(Path const& other) const { return path_ == other.path_; }

std::ostream& operator<<(std::ostream& os, Path const& path) {
  return os << path.path_;
}

std::string BuildPath(std::initializer_list<std::string> parts) {
  Path path;
  for (auto const& p : parts) {
    path /= p;
  }
  return path;
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
  Path partial_path{"/"};

  for (auto const& component : absl::StrSplit(path, '/')) {
    if (component.empty()) {
      continue;
    }
    partial_path /= component;
    if (DirectoryExists(partial_path)) {
      continue;
    }
    std::string missing_directory{partial_path};
    if (mkdir(missing_directory.c_str(), mode) != 0) {
      return false;
    }
  }
  return true;
}

bool DirectoryExists(std::string const& path) {
  struct stat info;
  if (!system_interface->stat(path, &info)) {
    return false;
  }
  return S_ISDIR(info.st_mode);
}

bool FileExists(std::string const& path) {
  struct stat info;
  if (!system_interface->stat(path, &info)) {
    return false;
  }
  return S_ISREG(info.st_mode);
}

bool FileExists(std::initializer_list<std::string> parts) {
  return FileExists(BuildPath(parts));
}

Path HomeDirectory() {
  std::string home = environment::Get("HOME");

  if (!home.empty()) {
    return Path{home};
  }

  struct passwd* pwd = getpwuid(getuid());

  if (pwd != nullptr) {
    return pwd->pw_dir;
  }

  // if we got here both getenv() and getpwuid() failed, so let's just return
  // an empty Path to indicate failure
  return {};
}

bool ReadFile(std::string const& path,
              std::function<bool(std::string const&)> const& fn) {
  static constexpr std::streamsize kBufferSize = (1 << 10);  // 1 KiB

  std::ifstream is{path};
  if (!is.good()) {
    return false;
  }

  std::string contents;
  while (!is.eof()) {
    char buffer[kBufferSize + 1];
    is.read(buffer, kBufferSize);
    if (is.fail() && !is.eof()) {
      return false;
    }

    buffer[is.gcount()] = '\0';
    contents.append(buffer);
  }

  return fn(contents);
}

bool ReadFileByLine(std::string const& path,
                    std::function<bool(std::string const&)> const& fn) {
  std::ifstream is(path);
  if (!is.good()) {
    return false;
  }

  while (!is.eof()) {
    std::string line;
    std::getline(is, line);
    if (is.fail() && !is.eof()) {
      return false;
    }
    if (!fn(line)) {
      return false;
    }
  }

  return true;
}

}  // namespace fs
}  // namespace util
