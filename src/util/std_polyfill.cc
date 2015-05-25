#include <cerrno>
#include <cstdlib>
#include <stdexcept>

#include "compiler_features.h"
#include "util/std_polyfill.h"

namespace std {

#ifndef HAVE_STD_STOI
  int stoi(std::string const& str,
           std::size_t* pos,
           int base) {
    return std::stol(str, pos, base);
  }
#endif  // HAVE_STD_STOI

#ifndef HAVE_STD_STOL
  long stol(std::string const& str,
            std::size_t* pos,
            int base) {
    char* endptr = nullptr;
    const char* startptr = str.c_str();
    long value = std::strtol(startptr, &endptr, base);

    if (pos != nullptr) {
      (*pos) = static_cast<size_t>(endptr - startptr);
    }

    return value;
  }
#endif  // HAVE_STD_STOL

#ifndef HAVE_STD_STOF
  float stof(std::string const& str,
             std::size_t* pos) {
    char* endptr = nullptr;
    const char* startptr = str.c_str();
    float value = std::strtod(startptr, &endptr);

    if (errno == ERANGE) {
      throw std::out_of_range(str);
    }

    if (pos != nullptr) {
      (*pos) = static_cast<size_t>(endptr - startptr);
    }

    return value;
  }
#endif  // HAVE_STD_STOF

}  // namespace std
