#ifndef STD_POLYFILL_H
#define STD_POLYFILL_H

#include <cstdlib>
#include <string>

namespace std {
  int stoi(std::string const& str,
           std::size_t* pos = nullptr,
           int base = 10);

  long stol(std::string const& str,
            std::size_t* pos = nullptr,
            int base = 10);

  float stof(std::string const& str,
             std::size_t* pos = nullptr);
}  // namespace std

#endif  // STD_POLYFILL_H
