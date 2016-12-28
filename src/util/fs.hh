#ifndef TINT3_UTIL_FS_HH
#define TINT3_UTIL_FS_HH

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <functional>
#include <initializer_list>
#include <ostream>
#include <string>

namespace util {
namespace fs {

class DirectoryContents {
 public:
  explicit DirectoryContents(std::string const& path);
  ~DirectoryContents();

  class iterator {
   public:
    friend class DirectoryContents;

    iterator& operator++();
    std::string const operator*() const;
    bool operator!=(iterator const& other) const;

   private:
    iterator();
    iterator(DIR* dir);

    DIR* dir_;
    struct dirent* entry_;
    long pos_;
  };

  iterator const begin() const;
  iterator const end() const;

 private:
  DIR* dir_;
};

class Path {
 public:
  friend std::ostream& operator<<(std::ostream& os, Path const& path);

  Path() = default;
  Path(Path const& other);
  Path(Path&& other);
  Path(std::string const& path);
  Path(char const* path);

  Path& operator=(Path other);
  Path operator/(std::string const& component);
  Path& operator/=(std::string const& component);
  operator std::string() const;

 private:
  std::string path_;
};

std::ostream& operator<<(std::ostream& os, Path const& path);

std::string BuildPath(std::initializer_list<std::string> parts);
bool CopyFile(std::string const& from_path, std::string const& to_path);
bool CreateDirectory(std::string const& path, mode_t mode = 0700);
bool DirectoryExists(std::string const& path);
bool FileExists(std::string const& path);
bool FileExists(std::initializer_list<std::string> parts);
Path HomeDirectory();
bool IsAbsolutePath(std::string const& path);
bool ReadFile(std::string const& path,
              std::function<bool(std::string const&)> const& fn);
bool ReadFileByLine(std::string const& path,
                    std::function<bool(std::string const&)> const& fn);

}  // namespace fs
}  // namespace util

#endif  // TINT3_UTIL_FS_HH
