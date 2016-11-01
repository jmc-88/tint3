#include "clock/time_utils.hh"
#include "util/environment.hh"

struct tm ClockGetTimeForTimezone(std::string const& timezone, time_t timer) {
  if (timezone.empty()) {
    return (*std::localtime(&timer));
  }

  environment::ScopedOverride tz{"TZ", timezone};
  return (*std::localtime(&timer));
}

std::string FormatTime(std::string format, struct tm const& time_ref) {
  // %p can return a zero-length string, and a returned size of 0 is also used
  // by strftime() to indicate errors, so we need to insert a bogus character
  // in the format string which will be removed at the end, to ensure that a
  // correct run of strftime() will never return 0.
  format.push_back('\a');

  std::string buffer;
  buffer.resize(format.size());

  size_t len =
      std::strftime(&buffer[0], buffer.size(), format.c_str(), &time_ref);
  while (len == 0) {
    buffer.resize(2 * buffer.size());
    len = std::strftime(&buffer[0], buffer.size(), format.c_str(), &time_ref);
  }

  buffer.resize(len - 1);
  return buffer;
}
