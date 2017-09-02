#include "catch.hpp"

#include "clock/time_utils.hh"
#include "util/environment.hh"

TEST_CASE("FormatTime", "The interface to strftime() works as expected") {
  // 1970-01-01 00:00 AM UTC
  time_t elapsed = 0;
  struct tm tm = (*std::gmtime(&elapsed));

  SECTION("Empty format string") { REQUIRE(FormatTime("", tm).empty()); }

  SECTION("Short YYYY-MM-DD date") {
    REQUIRE(FormatTime("%F", tm) == "1970-01-01");
  }
}

TEST_CASE("ClockGetTimeForTimezone",
          "Can reliably retrieve a time in a certain timezone") {
  // 1970-01-01 00:00 AM UTC
  time_t tm_12am_elapsed = 0;
  struct tm tm_12am = (*std::gmtime(&tm_12am_elapsed));

  // 1970-01-01 08:00 AM UTC
  time_t tm_8am_elapsed = (60 * 60 * 8);
  struct tm tm_8am = (*std::gmtime(&tm_8am_elapsed));

  auto same_time = [](struct tm lhs, struct tm rhs) -> bool {
    return std::difftime(std::mktime(&lhs), std::mktime(&rhs)) == 0;
  };

  SECTION("Empty timezone") {
    auto tz = environment::MakeScopedOverride("TZ", "UTC");
    struct tm tm2 = ClockGetTimeForTimezone("", tm_8am_elapsed);
    REQUIRE(same_time(tm_8am, tm2));
  }

  SECTION("Timezone override works and doesn't change the environment") {
    auto tz = environment::MakeScopedOverride("TZ", "UTC");
    struct tm tm2 =
        ClockGetTimeForTimezone("America/Los_Angeles", tm_8am_elapsed);
    REQUIRE(same_time(tm_12am, tm2));
    REQUIRE(environment::Get("TZ") == "UTC");
  }
}
