#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ctime>
#include <string>

struct tm* ClockGetTimeForTimezone(std::string const& timezone,
                                   time_t const* time_ptr);
std::string FormatTime(std::string format, struct tm const* timeptr);

#endif  // TIME_UTILS_H