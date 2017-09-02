#include "util/environment.hh"

namespace environment {

std::string Get(std::string const& key) {
  char* value = getenv(key.c_str());
  if (value != nullptr) {
    return value;
  }
  return std::string{};
}

bool Unset(std::string const& key) {
  if (unsetenv(key.c_str()) != 0) {
    util::log::Error() << "unsetenv(): " << std::strerror(errno) << '\n';
    return false;
  }
  return true;
}

}  // namespace environment
