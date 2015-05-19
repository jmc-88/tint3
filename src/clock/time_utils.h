#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ctime>

std::string FormatTime(std::string format, struct tm const* timeptr);

#endif  // TIME_UTILS_H