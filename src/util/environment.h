#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <string>

namespace environment {

std::string Get(std::string const& key);
bool Set(std::string const& key, std::string const& value);
bool Unset(std::string const& key);

class ScopedOverride {
 public:
  ScopedOverride(std::string const& key, std::string const& value);
  ~ScopedOverride();

 private:
  std::string key_;
  std::string original_value_;
};

}  // namespace environment

#endif  // ENVIRONMENT_H