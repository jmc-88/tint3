#ifndef TINT3_UTIL_ENVIRONMENT_HH
#define TINT3_UTIL_ENVIRONMENT_HH

#include <cstdlib>
#include <cstring>
#include <string>

#include "util/common.hh"
#include "util/log.hh"

namespace environment {

std::string Get(std::string const& key);
bool Unset(std::string const& key);

template <typename T>
bool Set(std::string const& key, T const& value) {
  std::string value_string = util::string::Representation(value);
  if (setenv(key.c_str(), value_string.c_str(), 1) != 0) {
    util::log::Error() << "setenv(): " << std::strerror(errno) << '\n';
    return false;
  }
  return true;
}

template <typename T>
class ScopedOverride {
 public:
  ScopedOverride(std::string const& key, T const& value)
      : key_(key), original_value_(getenv(key.c_str())) {
    Set(key, util::string::Representation(value));
  }

  ~ScopedOverride() {
    if (original_value_ != nullptr) {
      setenv(key_.c_str(), original_value_, 1);
    } else {
      unsetenv(key_.c_str());
    }
  }

 private:
  std::string key_;
  const char* original_value_;
};

template <typename T>
ScopedOverride<T> MakeScopedOverride(std::string const& key, T const& value) {
  return ScopedOverride<T>(key, value);
}

}  // namespace environment

#endif  // TINT3_UTIL_ENVIRONMENT_HH
