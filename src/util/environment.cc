#include <cstring>

#include "util/environment.h"
#include "util/log.h"

namespace environment {

std::string Get(std::string const& key) {
  char* value = getenv(key.c_str());
  std::string result;

  if (value != nullptr) {
    result.assign(value);
  }

  return result;
}

bool Set(std::string const& key, std::string const& value) {
  if (setenv(key.c_str(), value.c_str(), 1) != 0) {
    util::log::Error() << "setenv(): " << std::strerror(errno) << '\n';
    return false;
  }

  return true;
}

bool Unset(std::string const& key) {
  if (unsetenv(key.c_str()) != 0) {
    util::log::Error() << "unsetenv(): " << std::strerror(errno) << '\n';
    return false;
  }

  return true;
}

ScopedOverride::ScopedOverride(const std::string& key, const std::string& value)
    : key_(key), original_value_(Get(key)) {
  Set(key, value);
}

ScopedOverride::~ScopedOverride() {
  if (!original_value_.empty()) {
    Set(key_, original_value_);
  } else {
    Unset(key_);
  }
}

}  // namespace environment