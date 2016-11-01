#ifndef TINT3_CLOCK_TIME_UTILS_HH
#define TINT3_CLOCK_TIME_UTILS_HH

#include <ctime>
#include <string>

struct tm ClockGetTimeForTimezone(std::string const& timezone, time_t timer);
std::string FormatTime(std::string format, struct tm const& time_ref);

#endif  // TINT3_CLOCK_TIME_UTILS_HH
