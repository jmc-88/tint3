#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "clock/time_utils.h"
#include "util/environment.h"

TEST_CASE("FormatTime", "The interface to strftime() works as expected") {
  // 1970-01-01 00:00 AM UTC
  time_t elapsed = 0;
  struct tm* tm = std::gmtime(&elapsed);

  SECTION("Empty format string") { REQUIRE(FormatTime("", tm).empty()); }

  SECTION("Short YYYY-MM-DD date") {
    REQUIRE(FormatTime("%F", tm) == "1970-01-01");
  }
}

TEST_CASE("ClockGetTimeForTimezone",
          "Can reliably retrive a time in a certain timezone") {
  // 1970-01-01 08:00 AM UTC
  time_t elapsed = (60 * 60 * 8);
  struct tm* tm1 = std::gmtime(&elapsed);

  auto same_time = [](struct tm* lhs, struct tm* rhs) -> bool {
    return lhs->tm_sec == rhs->tm_sec && lhs->tm_min == rhs->tm_min &&
           lhs->tm_hour == rhs->tm_hour && lhs->tm_mday == rhs->tm_mday &&
           lhs->tm_mon == rhs->tm_mon && lhs->tm_year == rhs->tm_year &&
           lhs->tm_wday == rhs->tm_wday && lhs->tm_yday == rhs->tm_yday &&
           lhs->tm_isdst == rhs->tm_isdst;
  };

  SECTION("Empty timezone") {
    environment::ScopedOverride tz{"TZ", "UTC"};
    struct tm* tm2 = ClockGetTimeForTimezone("", &elapsed);
    REQUIRE(same_time(tm1, tm2));
  }

  SECTION("Timezone override works and doesn't change the environment") {
    environment::ScopedOverride tz{"TZ", "UTC"};
    struct tm* tm2 = ClockGetTimeForTimezone("US/Pacific", &elapsed);
    REQUIRE(same_time(tm1, tm2));
    REQUIRE(environment::Get("TZ") == "UTC");
  }
}