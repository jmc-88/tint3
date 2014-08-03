#ifndef TINT3_FS_H
#define TINT3_FS_H

#include <sys/stat.h>

#include <functional>
#include <initializer_list>
#include <string>

namespace fs {

std::string BuildPath(std::initializer_list<std::string> parts);
bool CopyFile(std::string const& from_path, std::string const& to_path);
bool CreateDirectory(std::string const& path, mode_t mode = 0700);
bool DirectoryExists(std::string const& path);
bool FileExists(std::string const& path);
bool FileExists(std::initializer_list<std::string> parts);
std::string HomeDirectory();
bool IsAbsolutePath(std::string const& path);
bool ReadFileByLine(std::string const& path,
                    std::function<void(std::string const&)> fn);

} // namespace fs

#endif // TINT3_FILE_H
