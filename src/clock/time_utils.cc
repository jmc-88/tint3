#include "clock/time_utils.h"
#include "util/environment.h"

struct tm* ClockGetTimeForTimezone(std::string const& timezone,
                                   time_t const* time_ptr) {
  if (timezone.empty()) {
    return std::localtime(time_ptr);
  }

  environment::ScopedOverride tz{"TZ", timezone};
  return std::localtime(time_ptr);
}

std::string FormatTime(std::string format, struct tm const* timeptr) {
  // %p can return a zero-length string, and a returned size of 0 is also used
  // by strftime() to indicate errors, so we need to insert a bogus character
  // in the format string which will be removed at the end, to ensure that a
  // correct run of strftime() will never return 0.
  format.push_back('\a');

  std::string buffer;
  buffer.resize(format.size());

  size_t len =
      std::strftime(&buffer[0], buffer.size(), format.c_str(), timeptr);
  while (len == 0) {
    buffer.resize(2 * buffer.size());
    len = std::strftime(&buffer[0], buffer.size(), format.c_str(), timeptr);
  }

  buffer.resize(len - 1);
  return buffer;
}