#ifndef STD_POLYFILL_H
#define STD_POLYFILL_H

#include <cstdlib>
#include <string>

#include "compiler_features.h"

namespace std {
#ifndef HAVE_STD_STOI
int stoi(std::string const& str, std::size_t* pos = nullptr, int base = 10);
#endif  // HAVE_STD_STOI

#ifndef HAVE_STD_STOL
long stol(std::string const& str, std::size_t* pos = nullptr, int base = 10);
#endif  // HAVE_STD_STOL

#ifndef HAVE_STD_STOF
float stof(std::string const& str, std::size_t* pos = nullptr);
#endif  // HAVE_STD_STOF
}  // namespace std

#endif  // STD_POLYFILL_H
